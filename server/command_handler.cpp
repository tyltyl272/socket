#include "command_handler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

CommandHandler::CommandHandler(SOCKET socket, sockaddr_in addr)
    : clientSocket(socket), clientAddr(addr), isAuthenticated(false) {
    
    currentDir = "./storage";
    if (!fs::exists(currentDir)) {
        fs::create_directory(currentDir);
    }
}

CommandHandler::~CommandHandler() {}

void CommandHandler::sendReply(const std::string& reply) {
    send(clientSocket, reply.c_str(), reply.length(), 0);
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

    uint32_t hash = 5381;
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytesRead = file.gcount();
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(buffer[i]);
        }
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << hash;
    return ss.str();
}

void CommandHandler::processCommands() {
    sendReply("220 Hybrid FTP Server Ready.\r\n");

    std::string renameFromPath = "";
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived <= 0) {
            std::cout << "[INFO] Client đã ngắt kết nối\n";
            break;
        }

        std::string rawInput(buffer);
        std::string cleanInput = trim(rawInput);
        if (cleanInput.empty()) continue;

        std::stringstream ss(cleanInput);
        std::string cmd, param;
        ss >> cmd;
        std::getline(ss, param);
        param = trim(param);

        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
        std::cout << "[LỆNH NHẬN]: " << cmd << " | Tham số: " << param << std::endl;

        if (cmd == "USER") {
            handleUSER(param);
            continue;

        } else if (cmd == "PASS") {
            handlePASS(param);
            continue;

        } else if (cmd == "QUIT") {
            handleQUIT();
            break;
        }

        if (!isAuthenticated) {
            sendReply("530 Please login with USER and PASS\r\n");
            continue;
        }

        if (cmd == "PWD") {
            handlePWD();

        } else if (cmd == "CWD") {
            handleCWD(param);

        } else if (cmd == "CDUP") {
            handleCWD("..");

        } else if (cmd == "LIST") {
            // Gộp toàn bộ thông tin gửi trong 1 gói duy nhất để không bị trễ buffer
            std::string response = "150 Here comes the directory listing\r\n";
            for (const auto& entry : fs::directory_iterator(currentDir)) {
                std::string name = entry.path().filename().string();
                if (fs::is_directory(entry)) {
                    response += "[DIR] " + name + "\r\n";
                } else {
                    response += "[FILE] " + name + " (" + std::to_string(fs::file_size(entry)) + " bytes)\r\n";
                }
            }
            response += "226 Directory send OK\r\n";
            sendReply(response);

        } else if (cmd == "NLST") {
            std::string response = "150 Here comes the name list\r\n";
            for (const auto& entry : fs::directory_iterator(currentDir)) {
                response += entry.path().filename().string() + "\r\n";
            }
            response += "226 Transfer complete\r\n";
            sendReply(response);

        } else if (cmd == "MKD") {
            if (param.empty()) {
                sendReply("501 Syntax error in parameters\r\n");
            } else {
                std::string path = currentDir + "/" + param;
                if (fs::create_directory(path)) sendReply("257 Directory created\r\n");
                else sendReply("550 Create directory failed\r\n");
            }

        } else if (cmd == "RMD") {
            if (param.empty()) {
                sendReply("501 Syntax error in parameters\r\n");
            } else {
                std::string path = currentDir + "/" + param;
                if (fs::exists(path) && fs::remove(path)) sendReply("250 Directory removed\r\n");
                else sendReply("550 Remove directory failed\r\n");
            }

        } else if (cmd == "DELE") {
            if (param.empty()) {
                sendReply("501 Syntax error in parameters\r\n");
            } else {
                std::string path = currentDir + "/" + param;
                if (fs::exists(path) && fs::remove(path)) sendReply("250 File deleted\r\n");
                else sendReply("550 File not found or action failed\r\n");
            }

        } else if (cmd == "SIZE") {
            if (param.empty()) {
                sendReply("501 Syntax error in parameters\r\n");
            } else {
                std::string path = currentDir + "/" + param;
                if (fs::exists(path) && !fs::is_directory(path)) {
                    sendReply("213 " + std::to_string(fs::file_size(path)) + "\r\n");
                } else sendReply("550 File not found\r\n");
            }

        } else if (cmd == "HASH") {
            if (param.empty()) {
                sendReply("501 Syntax error in parameters\r\n");
            } else {
                std::string path = currentDir + "/" + param;
                if (fs::exists(path) && !fs::is_directory(path)) {
                    std::string hashVal = calculateFileHash(path);
                    sendReply("200 HASH " + param + " " + hashVal + "\r\n");
                } else sendReply("550 File not found\r\n");
            }

        } else if (cmd == "RETR") {
            handleRETR(param);

        } else if (cmd == "STOR") {
            handleSTOR(param);

        } else if (cmd == "APPE") {
            sendReply("150 Ready to append data via UDP\r\n");

        } else if (cmd == "STOU") {
            std::string uniqueName = "file_" + std::to_string(time(nullptr)) + ".dat";
            sendReply("150 FILE: " + uniqueName + "\r\n");

        } else if (cmd == "ABOR") {
            sendReply("226 Abort command successful\r\n");

        } else if (cmd == "RNFR") {
            std::string path = currentDir + "/" + param;
            if (fs::exists(path)) {
                renameFromPath = path;
                sendReply("350 Requested file action pending RNTO\r\n");
            } else sendReply("550 File not found\r\n");

        } else if (cmd == "RNTO") {
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

        } else if (cmd == "STAT") {
            sendReply("211 Server status OK\r\n");

        } else if (cmd == "MDTM") {
            sendReply("213 20260801120000\r\n");

        } else if (cmd == "NOOP") {
            sendReply("200 OK\r\n");

        } else if (cmd == "TYPE") {
            sendReply("200 Type set to " + param + "\r\n");

        } else if (cmd == "MODE") {
            sendReply("200 Mode set to " + param + "\r\n");

        } else if (cmd == "PORT") {
            sendReply("200 Active mode client IP/Port received\r\n");

        } else if (cmd == "PASV") {
            sendReply("227 Entering Passive Mode (127,0,0,1,31,144)\r\n");

        } else if (cmd == "HELP") {
            sendReply("214 Supported: USER PASS PWD CWD CDUP LIST NLST RETR STOR APPE STOU DELE MKD RMD RNFR RNTO SIZE HASH STAT MDTM TYPE MODE PORT PASV ABOR NOOP QUIT\r\n");

        } else {
            sendReply("500 Syntax error, command unrecognized\r\n");
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
}

void CommandHandler::handlePWD() {
    std::string reply = "257 \"" + currentDir + "\" is current directory\r\n";
    sendReply(reply);
}

void CommandHandler::handleCWD(const std::string& param) {
    std::string newPath = currentDir + "/" + param;
    if (fs::exists(newPath) && fs::is_directory(newPath)) {
        currentDir = fs::canonical(newPath).string();
        sendReply("250 Directory successfully changed\r\n");
    } else {
        sendReply("550 Failed to change directory. Directory does not exist\r\n");
    }
}

void CommandHandler::handleLIST() {
    sendReply("150 Here comes the directory listing\r\n");
    std::string fileList = "";
    try {
        for (const auto& entry : fs::directory_iterator(currentDir)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                fileList += "[DIR] " + name + "\r\n";
            } else {
                fileList += "[FILE] " + name + " (" + std::to_string(entry.file_size()) + " bytes)\r\n";
            }
        }
        sendReply(fileList);
        sendReply("226 Directory send OK\r\n");
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

    std::string filePath = currentDir + "/" + param;

    if (!fs::exists(filePath) || fs::is_directory(filePath)) {
        sendReply("550 File not found or is a directory\r\n");
        return;
    }

    uintmax_t fileSize = fs::file_size(filePath);
    std::string reply = "150 Opening UDP Data connection for " + param + 
                        " (" + std::to_string(fileSize) + " bytes)\r\n";
    sendReply(reply);
}

void CommandHandler::handleSTOR(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }

    std::string reply = "150 Ready to receive file " + param + " via UDP\r\n";
    sendReply(reply);
}