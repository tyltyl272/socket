#include "command_handler.h"
#include "rdt.h"
#include "session_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <chrono>

extern SessionManager g_sessionManager;

namespace fs = std::filesystem;

static std::string formatSize(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    
    std::stringstream ss;
    if (bytes < 1024 * 1024) {
        ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return ss.str();
    }
    if (bytes < 1024ULL * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
        return ss.str();
    }
    
    ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    return ss.str();
}

CommandHandler::CommandHandler(SOCKET socket, sockaddr_in addr)
    : clientSocket(socket), clientAddr(addr), isAuthenticated(false), dataUdpPort(0) {
    
    clientIP = inet_ntoa(clientAddr.sin_addr);

    currentDir = "./storage";
    try {
        if (!fs::exists(currentDir)) {
            fs::create_directories(currentDir);
        }
    } catch (...) {}
}

CommandHandler::~CommandHandler() {}

void CommandHandler::sendReply(const std::string& reply) {
    send(clientSocket, reply.c_str(), static_cast<int>(reply.length()), 0);
}

std::string CommandHandler::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string CommandHandler::calculateFileHash(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";

    uint32_t hash = 0x811C9DC5;
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytesRead = file.gcount();
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 0x01000193;
        }
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << hash;
    return ss.str();
}

void CommandHandler::processCommands() {
    sendReply("220 Hybrid FTP Server Ready.\r\n");

    std::string streamBuffer = "";
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived <= 0) {
            std::cout << "[INFO] Client " << clientIP << " đã ngắt kết nối TCP.\n";
            break;
        }

        streamBuffer.append(buffer, bytesReceived);

        size_t pos = 0;
        while ((pos = streamBuffer.find('\n')) != std::string::npos) {
            std::string rawInput = streamBuffer.substr(0, pos);
            streamBuffer.erase(0, pos + 1);

            std::string cleanInput = trim(rawInput);
            if (cleanInput.empty()) continue;

            std::stringstream ss(cleanInput);
            std::string cmd, param;
            ss >> cmd;
            std::getline(ss, param);
            param = trim(param);

            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
            std::cout << "[LỆNH NHẬN]: " << cmd << " | Tham số: " << param << std::endl;
            
            if (cmd == "USER") { handleUSER(param); continue; }
            if (cmd == "PASS") { handlePASS(param); continue; }
            if (cmd == "QUIT") { handleQUIT(); return; }

            if (!isAuthenticated) {
                sendReply("530 Please login with USER and PASS\r\n");
                continue;
            }

            if (cmd == "PWD") handlePWD();
            else if (cmd == "CWD") handleCWD(param);
            else if (cmd == "CDUP") handleCWD("..");
            else if (cmd == "LIST") handleLIST();
            else if (cmd == "NLST") {
                std::string response = "150 Here comes the name list\r\n";
                for (const auto& entry : fs::directory_iterator(currentDir)) {
                    response += entry.path().filename().string() + "\r\n";
                }
                response += "226 Transfer complete\r\n";
                sendReply(response);
            }
            else if (cmd == "MKD") {
                if (param.empty()) sendReply("501 Syntax error in parameters\r\n");
                else {
                    std::string path = currentDir + "/" + param;
                    if (fs::create_directory(path)) sendReply("257 Directory created\r\n");
                    else sendReply("550 Create directory failed\r\n");
                }
            }
            else if (cmd == "RMD") {
                if (param.empty()) sendReply("501 Syntax error in parameters\r\n");
                else {
                    std::string path = currentDir + "/" + param;
                    if (fs::exists(path) && fs::remove(path)) sendReply("250 Directory removed\r\n");
                    else sendReply("550 Remove directory failed\r\n");
                }
            }
            else if (cmd == "DELE") {
                if (param.empty()) sendReply("501 Syntax error in parameters\r\n");
                else {
                    std::string path = currentDir + "/" + param;
                    if (fs::exists(path) && fs::remove(path)) sendReply("250 File deleted\r\n");
                    else sendReply("550 File not found or action failed\r\n");
                }
            }
            else if (cmd == "SIZE") {
                if (param.empty()) sendReply("501 Syntax error in parameters\r\n");
                else {
                    std::string path = currentDir + "/" + param;
                    if (fs::exists(path) && !fs::is_directory(path)) {
                        auto fileSize = fs::file_size(path);
                        sendReply("213 " + formatSize(fileSize) + "\r\n");
                    } else sendReply("550 File not found\r\n");
                }
            }
            else if (cmd == "HASH") {
                if (param.empty()) sendReply("501 Syntax error in parameters\r\n");
                else {
                    std::string path = currentDir + "/" + param;
                    if (fs::exists(path) && !fs::is_directory(path)) {
                        std::string hashVal = calculateFileHash(path);
                        sendReply("200 HASH " + param + " " + hashVal + "\r\n");
                    } else sendReply("550 File not found\r\n");
                }
            }
            else if (cmd == "RETR") handleRETR(param);
            else if (cmd == "STOR") handleSTOR(param);
            else if (cmd == "APPE") {
                if (param.empty()) sendReply("501 Syntax error in parameters\r\n");
                else handleSTOR(param);
            }
            else if (cmd == "STOU") {
                std::string uniqueName = "file_" + std::to_string(time(nullptr)) + ".dat";
                handleSTOR(uniqueName);
            }
            else if (cmd == "ABOR") sendReply("226 Abort command successful\r\n");
            else if (cmd == "RNFR") {
                std::string path = currentDir + "/" + param;
                if (fs::exists(path)) {
                    renameFromPath = path;
                    sendReply("350 Requested file action pending RNTO\r\n");
                } else sendReply("550 File not found\r\n");
            }
            else if (cmd == "RNTO") {
                if (renameFromPath.empty()) {
                    sendReply("503 Bad sequence of commands. Send RNFR first\r\n");
                } else {
                    std::string newPath = currentDir + "/" + param;
                    try {
                        fs::rename(renameFromPath, newPath);
                        sendReply("250 File renamed successfully\r\n");
                    } catch (...) {
                        sendReply("550 Rename failed\r\n");
                    }
                    renameFromPath = "";
                }
            }
            else if (cmd == "STAT") sendReply("211 Server status OK\r\n");
            else if (cmd == "MDTM") {
                std::string path = currentDir + "/" + param;
                if (fs::exists(path)) {
                    auto ftime = fs::last_write_time(path);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    std::time_t ctime = std::chrono::system_clock::to_time_t(sctp);
                    
                    std::tm gmtBuf;
#if defined(_WIN32) || defined(_WIN64)
                    gmtime_s(&gmtBuf, &ctime);
#else
                    gmtime_r(&ctime, &gmtBuf);
#endif
                    char timeBuf[32];
                    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d%H%M%S", &gmtBuf);
                    sendReply("213 " + std::string(timeBuf) + "\r\n");
                } else sendReply("550 File not found\r\n");
            }
            else if (cmd == "NOOP") sendReply("200 OK\r\n");
            else if (cmd == "TYPE") sendReply("200 Type set to " + param + "\r\n");
            else if (cmd == "MODE") sendReply("200 Mode set to " + param + "\r\n");
            else if (cmd == "PORT") {
                sendReply("200 Active mode client IP/Port received\r\n");
            }
            else if (cmd == "PASV") {
                sendReply("227 Entering Passive Mode (127,0,0,1,31,145)\r\n");
            }
            else if (cmd == "HELP") {
                sendReply("214 Supported: USER PASS PWD CWD CDUP LIST NLST RETR STOR APPE STOU DELE MKD RMD RNFR RNTO SIZE HASH STAT MDTM TYPE MODE PORT PASV ABOR NOOP QUIT\r\n");
            }
            else {
                sendReply("500 Syntax error, command unrecognized\r\n");
            }
        }
    }
}

void CommandHandler::handleUSER(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }
    currentUsername = param;
    sendReply("331 Username OK, need password\r\n");
}

void CommandHandler::handlePASS(const std::string& param) {
    if (currentUsername.empty()) {
        sendReply("503 Bad sequence of commands. Send USER first\r\n");
        return;
    }
    isAuthenticated = true;
    sendReply("230 Login successful\r\n");
    // Cập nhật tên user và trạng thái Logged In vào bảng
    g_sessionManager.updateAuth(clientSocket, currentUsername, true);
    g_sessionManager.printActiveSessions(); // In lại bảng ra Console
}

void CommandHandler::handlePWD() {
    sendReply("257 \"" + currentDir + "\" is current directory\r\n");
}

void CommandHandler::handleCWD(const std::string& param) {
    std::string newPath = currentDir + "/" + param;
    if (fs::exists(newPath) && fs::is_directory(newPath)) {
        try {
            currentDir = fs::weakly_canonical(newPath).string();
            sendReply("250 Directory successfully changed\r\n");
        } catch (...) {
            sendReply("550 Failed to change directory\r\n");
        }
    } else {
        sendReply("550 Failed to change directory. Directory does not exist\r\n");
    }
}

void CommandHandler::handleLIST() {
    std::string fileList = "150 Here comes the directory listing\r\n";
    try {
        for (const auto& entry : fs::directory_iterator(currentDir)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                fileList += "[DIR] " + name + "\r\n";
            } else {
                fileList += "[FILE] " + name + " (" + formatSize(entry.file_size()) + ")\r\n";
            }
        }
        fileList += "226 Directory send OK\r\n";
        sendReply(fileList);
    } catch (...) {
        sendReply("550 Directory read error\r\n");
    }
}

void CommandHandler::handleQUIT() {
    sendReply("221 Service closing control connection. Goodbye!\r\n");
}

void CommandHandler::handleRETR(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }

    std::stringstream ss(param);
    std::string filenameOnly;
    int clientPortFromParam = 0;
    ss >> filenameOnly >> clientPortFromParam;

    filenameOnly = fs::path(filenameOnly).filename().string();
    std::string filePath = currentDir + "/" + filenameOnly;

    if (!fs::exists(filePath)) {
        sendReply("550 File not found\r\n");
        return;
    }

    uintmax_t fileSize = fs::file_size(filePath);

    std::string reply = "150 Opening UDP Data connection for " + filenameOnly + " (" + formatSize(fileSize) + ")\r\n";
    sendReply(reply);

    int targetPort = (clientPortFromParam != 0) ? clientPortFromParam : ((dataUdpPort != 0) ? dataUdpPort : 8081);

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket != INVALID_SOCKET) {
        std::cout << "[SERVER RDT] Đang truyền file " << filenameOnly << " tới " << clientIP << ":" << targetPort << "...\n";
        
        bool ok = rdt_send_file(udpSocket, filePath.c_str(), clientIP.c_str(), targetPort);
        if (ok) {
            sendReply("226 Transfer complete\r\n");
        } else {
            sendReply("426 Connection closed; transfer aborted\r\n");
        }
        closesocket(udpSocket);
    } else {
        sendReply("425 Can't open data connection\r\n");
    }
}

void CommandHandler::handleSTOR(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }

    std::string filenameOnly = fs::path(param).filename().string();

    std::string reply = "150 Ready to receive file " + filenameOnly + " via UDP\r\n";
    sendReply(reply);

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        sendReply("425 Can't open data connection\r\n");
        return;
    }

    BOOL reuse = TRUE;
    setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in serverUdpAddr{};
    serverUdpAddr.sin_family = AF_INET;
    serverUdpAddr.sin_port = htons(8081);
    serverUdpAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, (sockaddr*)&serverUdpAddr, sizeof(serverUdpAddr)) == SOCKET_ERROR) {
        sendReply("425 Can't bind data port\r\n");
        closesocket(udpSocket);
        return;
    }

    std::string filePath = currentDir + "/" + filenameOnly;

    if (rdt_receive_file(udpSocket, filePath.c_str(), 0)) {
        sendReply("226 Transfer complete\r\n");
    } else {
        sendReply("426 Connection closed; transfer aborted\r\n");
    }

    closesocket(udpSocket);
}