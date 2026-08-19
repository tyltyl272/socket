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
#include <cstdint>
#include <cstring>

extern SessionManager g_sessionManager;

namespace fs = std::filesystem;

// Class MD5 thuần C++ không phụ thuộc thư viện ngoài
class MD5 {
public:
    MD5() { init(); }
    void update(const uint8_t* input, size_t inputLen) {
        uint32_t i, index, partLen;
        index = (uint32_t)((count[0] >> 3) & 0x3F);
        if ((count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3)) count[1]++;
        count[1] += ((uint32_t)inputLen >> 29);
        partLen = 64 - index;
        if (inputLen >= partLen) {
            memcpy(&buffer[index], input, partLen);
            transform(buffer);
            for (i = partLen; i + 63 < inputLen; i += 64) transform(&input[i]);
            index = 0;
        } else i = 0;
        memcpy(&buffer[index], &input[i], inputLen - i);
    }
    std::string finalize() {
        uint8_t bits[8];
        for (int i = 0; i < 8; ++i) bits[i] = (uint8_t)((count[i >> 2] >> ((i & 3) * 8)) & 0xFF);
        uint32_t index = (uint32_t)((count[0] >> 3) & 0x3f);
        uint32_t padLen = (index < 56) ? (56 - index) : (120 - index);
        static const uint8_t PADDING[64] = { 0x80 };
        update(PADDING, padLen);
        update(bits, 8);
        std::stringstream ss;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                ss << std::hex << std::setfill('0') << std::setw(2) << (int)((state[i] >> (j * 8)) & 0xFF);
            }
        }
        return ss.str();
    }
private:
    uint32_t state[4], count[2];
    uint8_t buffer[64];
    void init() {
        count[0] = count[1] = 0;
        state[0] = 0x67452301; state[1] = 0xefcdab89;
        state[2] = 0x98badcfe; state[3] = 0x10325476;
    }
    static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
    static inline uint32_t rotate_left(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
    static inline void FF(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
    }
    static inline void GG(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + G(b, c, d) + x + ac, s) + b;
    }
    static inline void HH(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + H(b, c, d) + x + ac, s) + b;
    }
    static inline void II(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + I(b, c, d) + x + ac, s) + b;
    }
    void transform(const uint8_t block[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
        for (int i = 0; i < 16; ++i)
            x[i] = (uint32_t)block[i*4] | ((uint32_t)block[i*4+1] << 8) | ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);
        FF(a, b, c, d, x[ 0],  7, 0xd76aa478); FF(d, a, b, c, x[ 1], 12, 0xe8c7b756); FF(c, d, a, b, x[ 2], 17, 0x242070db); FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[ 4],  7, 0xf57c0faf); FF(d, a, b, c, x[ 5], 12, 0x4787c62a); FF(c, d, a, b, x[ 6], 17, 0xa8304613); FF(b, c, d, a, x[ 7], 22, 0xfd469501);
        FF(a, b, c, d, x[ 8],  7, 0x698098d8); FF(d, a, b, c, x[ 9], 12, 0x8b44f7af); FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12],  7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193); FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);
        GG(a, b, c, d, x[ 1],  5, 0xf61e2562); GG(d, a, b, c, x[ 6],  9, 0xc040b340); GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[ 5],  5, 0xd62f105d); GG(d, a, b, c, x[10],  9, 0x02441453); GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[ 9],  5, 0x21e1cde6); GG(d, a, b, c, x[14],  9, 0xc33707d6); GG(c, d, a, b, x[ 3], 14, 0xf4d50d87); GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13],  5, 0xa9e3e905); GG(d, a, b, c, x[ 2],  9, 0xfcefa3f8); GG(c, d, a, b, x[ 7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);
        HH(a, b, c, d, x[ 5],  4, 0xfffa3942); HH(d, a, b, c, x[ 8], 11, 0x8771f681); HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[ 1],  4, 0xa4beea44); HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9); HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13],  4, 0x289b7ec6); HH(d, a, b, c, x[ 0], 11, 0xeaa127fa); HH(c, d, a, b, x[ 3], 16, 0xd4ef3085); HH(b, c, d, a, x[ 6], 23, 0x04881d05);
        HH(a, b, c, d, x[ 9],  4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5); HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);
        II(a, b, c, d, x[ 0],  6, 0xf4292244); II(d, a, b, c, x[ 7], 10, 0x432aff97); II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[ 5], 21, 0xfc93a039);
        II(a, b, c, d, x[12],  6, 0x655b59c3); II(d, a, b, c, x[ 3], 10, 0x8f0ccc92); II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[ 1], 21, 0x85845dd1);
        II(a, b, c, d, x[ 8],  6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0); II(c, d, a, b, x[ 6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    }
};

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
    : clientSocket(socket), clientAddr(addr), isAuthenticated(false), dataUdpPort(8081), isPassiveMode(false) {
    
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

    MD5 md5;
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        md5.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(file.gcount()));
    }

    return md5.finalize();
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
            else if (cmd == "HASH") handleHASH(param);
            else if (cmd == "RETR") handleRETR(param);
            else if (cmd == "STOR") handleSTOR(param);
            else if (cmd == "APPE") handleAPPE(param);
            else if (cmd == "STOU") {
                std::string uniqueName = "file_" + std::to_string(time(nullptr)) + ".dat";
                handleSTOR(uniqueName);
            }
            else if (cmd == "ABOR") handleABOR();
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
            else if (cmd == "STAT") handleSTAT(param);
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
            else if (cmd == "TYPE") handleTYPE(param);
            else if (cmd == "MODE") handleMODE(param);
            else if (cmd == "PORT") handlePORT(param);
            else if (cmd == "PASV") handlePASV();
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
    g_sessionManager.updateAuth(clientSocket, currentUsername, true);
    g_sessionManager.printActiveSessions();
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

void CommandHandler::handleTYPE(const std::string& param) {
    std::string typeUpper = param;
    std::transform(typeUpper.begin(), typeUpper.end(), typeUpper.begin(), ::toupper);

    if (typeUpper == "A" || typeUpper == "I") {
        sendReply("200 Type set to " + typeUpper + "\r\n");
    } else {
        sendReply("504 Command not implemented for that parameter\r\n");
    }
}

void CommandHandler::handleMODE(const std::string& param) {
    std::string modeUpper = param;
    std::transform(modeUpper.begin(), modeUpper.end(), modeUpper.begin(), ::toupper);

    if (modeUpper == "S") {
        sendReply("200 Mode set to S\r\n");
    } else if (modeUpper == "C" || modeUpper == "B") {
        sendReply("504 Unimplemented MODE type. Only S (Stream) is supported\r\n");
    } else {
        sendReply("501 Syntax error in parameters\r\n");
    }
}

void CommandHandler::handleSTAT(const std::string& param) {
    if (param.empty()) {
        std::string status = "211-Hybrid FTP Server Status:\r\n";
        status += " Connected Client: " + clientIP + "\r\n";
        status += " Logged User: " + (currentUsername.empty() ? "None" : currentUsername) + "\r\n";
        status += " Data Mode: " + std::string(isPassiveMode ? "PASV" : "PORT") + "\r\n";
        status += " Data Port: " + std::to_string(dataUdpPort) + "\r\n";
        status += "211 End of status\r\n";
        sendReply(status);
    } else {
        std::string filePath = currentDir + "/" + param;
        if (fs::exists(filePath)) {
            sendReply("211 Status of " + param + ": OK (" + formatSize(fs::file_size(filePath)) + ")\r\n");
        } else {
            sendReply("550 File not found\r\n");
        }
    }
}

void CommandHandler::handleABOR() {
    sendReply("226 Abort command successful\r\n");
}

void CommandHandler::handlePORT(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }

    std::string cleanParam = param;
    std::replace(cleanParam.begin(), cleanParam.end(), ',', ' ');
    std::stringstream ss(cleanParam);
    int h1, h2, h3, h4, p1, p2;

    if (ss >> h1 >> h2 >> h3 >> h4 >> p1 >> p2) {
        clientIP = std::to_string(h1) + "." + std::to_string(h2) + "." + std::to_string(h3) + "." + std::to_string(h4);
        dataUdpPort = p1 * 256 + p2;
        isPassiveMode = false;
        sendReply("200 PORT command successful.\r\n");
    } else {
        try {
            dataUdpPort = std::stoi(param);
            isPassiveMode = false;
            sendReply("200 PORT command successful.\r\n");
        } catch (...) {
            sendReply("501 Syntax error in parameters\r\n");
        }
    }
}

void CommandHandler::handlePASV() {
    isPassiveMode = true;
    if (dataUdpPort == 0) dataUdpPort = 8081;

    int p1 = dataUdpPort / 256;
    int p2 = dataUdpPort % 256;

    std::string reply = "227 Entering Passive Mode (127,0,0,1," + std::to_string(p1) + "," + std::to_string(p2) + ")\r\n";
    sendReply(reply);
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
        BOOL reuse = TRUE;
        setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        if (isPassiveMode) {
            sockaddr_in serverUdpAddr{};
            serverUdpAddr.sin_family = AF_INET;
            serverUdpAddr.sin_port = htons(dataUdpPort != 0 ? dataUdpPort : 8081);
            serverUdpAddr.sin_addr.s_addr = INADDR_ANY;
            bind(udpSocket, (sockaddr*)&serverUdpAddr, sizeof(serverUdpAddr));
        }

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
    std::string filePath = currentDir + "/" + filenameOnly;

    std::cout << "\n[SERVER LOG] Receiving file via UDP RDT...\n";

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
    serverUdpAddr.sin_port = htons(dataUdpPort != 0 ? dataUdpPort : 8081);
    serverUdpAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, (sockaddr*)&serverUdpAddr, sizeof(serverUdpAddr)) == SOCKET_ERROR) {
        sendReply("425 Can't bind data port\r\n");
        closesocket(udpSocket);
        return;
    }

    if (rdt_receive_file(udpSocket, filePath.c_str(), 0)) {
        std::cout << "[SERVER LOG] File saved to " << filePath << "\n";
        
        // Tự động tính toán Hash MD5 sau khi lưu file hoàn tất
        std::string serverHash = calculateFileHash(filePath);
        std::cout << "[SERVER LOG] Calculated Stored Hash (MD5): " << serverHash << "\n\n";

        sendReply("226 Transfer complete\r\n");
    } else {
        std::cout << "[SERVER ERROR] Transfer failed or aborted\n";
        sendReply("426 Connection closed; transfer aborted\r\n");
    }

    closesocket(udpSocket);
}

void CommandHandler::handleAPPE(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }

    std::string filenameOnly = fs::path(param).filename().string();
    std::string filePath = currentDir + "/" + filenameOnly;

    std::string reply = "150 Ready to append to file " + filenameOnly + " via UDP\r\n";
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
    serverUdpAddr.sin_port = htons(dataUdpPort != 0 ? dataUdpPort : 8081);
    serverUdpAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, (sockaddr*)&serverUdpAddr, sizeof(serverUdpAddr)) == SOCKET_ERROR) {
        sendReply("425 Can't bind data port\r\n");
        closesocket(udpSocket);
        return;
    }

    uintmax_t offset = (fs::exists(filePath) && !fs::is_directory(filePath)) ? fs::file_size(filePath) : 0;

    if (rdt_receive_file(udpSocket, filePath.c_str(), offset)) {
        sendReply("226 Transfer complete\r\n");
    } else {
        sendReply("426 Connection closed; transfer aborted\r\n");
    }

    closesocket(udpSocket);
}

void CommandHandler::handleHASH(const std::string& param) {
    if (param.empty()) {
        sendReply("501 Syntax error in parameters\r\n");
        return;
    }

    // Chống Path Traversal: Chỉ lấy tên file
    std::string filenameOnly = fs::path(param).filename().string();
    std::string path = currentDir + "/" + filenameOnly;

    if (fs::exists(path) && !fs::is_directory(path)) {
        uintmax_t fSize = fs::file_size(path);
        
        std::cout << "[SERVER HASH] Đang tính toán checksum cho file: " << filenameOnly 
                  << " (" << formatSize(fSize) << ")..." << std::endl;

        std::string hashVal = calculateFileHash(path);

        if (!hashVal.empty()) {
            std::cout << "[SERVER HASH] Hoàn tất | File: " << filenameOnly 
                      << " | Hash (MD5): " << hashVal << std::endl;
            
            sendReply("200 HASH " + filenameOnly + " " + hashVal + "\r\n");
        } else {
            std::cout << "[SERVER ERROR] Không thể đọc file để tính Hash: " << filenameOnly << std::endl;
            sendReply("550 Cannot read file for hashing\r\n");
        }
    } else {
        std::cout << "[SERVER ERROR] Lệnh HASH thất bại - Không tìm thấy file: " << filenameOnly << std::endl;
        sendReply("550 File not found\r\n");
    }
}