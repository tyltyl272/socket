#include "tcp_client.h"
#include "ui_manager.h"
#include "rdt.h"
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <cstdint>
#include <cstring>

namespace fs = std::filesystem;

TCPClient* g_clientPtr = nullptr;

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

static std::string calculateLocalMD5(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";

    MD5 md5;
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        md5.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(file.gcount()));
    }

    return md5.finalize();
}

long long parseFileSizeFromResponse(const std::string& response) {
    size_t start = response.find('(');
    size_t end = response.find(')', start);

    if (start == std::string::npos || end == std::string::npos || end <= start) {
        return 0;
    }

    std::string content = response.substr(start + 1, end - start - 1);
    std::stringstream ss(content);
    
    double value = 0.0;
    std::string unit = "";
    ss >> value >> unit;

    for (auto &c : unit) c = toupper(c);

    if (unit == "MB") {
        return static_cast<long long>(value * 1024 * 1024);
    } else if (unit == "KB") {
        return static_cast<long long>(value * 1024);
    } else if (unit == "GB") {
        return static_cast<long long>(value * 1024 * 1024 * 1024);
    } else {
        return static_cast<long long>(value);
    }
}

void handleSignal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[HỆ THỐNG] Đã nhận tín hiệu ngắt (Ctrl+C). Đang ngắt kết nối an toàn...\n";
        if (g_clientPtr != nullptr) {
            g_clientPtr->disconnect();
        }
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    TCPClient client;
    g_clientPtr = &client;

    std::string serverIP = "";
    int serverPort = 0;

    if (!fs::exists("download")) {
        fs::create_directories("download");
    }

    if (argc >= 2) serverIP = argv[1];
    if (argc >= 3) {
        try { serverPort = std::stoi(argv[2]); } catch (...) { serverPort = 0; }
    }

    UIManager::printHeader();

    if (serverIP.empty()) {
        std::cout << "Nhập IP Server: ";
        std::getline(std::cin, serverIP);
        if (serverIP.empty()) serverIP = "127.0.0.1";
    }

    if (serverPort <= 0 || serverPort > 65535) {
        std::cout << "Nhập Port Server (Ấn Enter để dùng 8888): ";
        std::string inputPort;
        std::getline(std::cin, inputPort);
        serverPort = inputPort.empty() ? 8888 : std::stoi(inputPort);
    }

    std::cout << "Đang kết nối tới Server (" << serverIP << ":" << serverPort << ")..." << std::endl;

    if (!client.connectToServer(serverIP, serverPort)) {
        std::cerr << "[LỖI] Không thể kết nối tới Server! Hãy đảm bảo TCP Server đã chạy.\n";
        system("pause");
        return 1;
    }

    std::string welcomeMsg = client.receiveReply();
    if (!welcomeMsg.empty()) UIManager::printResponse(welcomeMsg);

    std::string inputCommand;
    while (client.checkConnection()) {
        UIManager::printPrompt();
        if (!std::getline(std::cin, inputCommand)) break;
        if (inputCommand.empty()) continue;

        std::stringstream ss(inputCommand);
        std::string cmd;
        ss >> cmd;
        std::string upperCmd = cmd;
        for (auto &c : upperCmd) c = toupper(c);

        if (upperCmd == "HELP") {
            UIManager::printHelp();
            continue;
        }

        if (!client.sendCommand(inputCommand)) {
            std::cout << "[LỖI] Mất kết nối tới Server!\n";
            break;
        }

        std::string response = client.receiveReply();
        if (!response.empty()) UIManager::printResponse(response);

        if (response.rfind("150", 0) == 0) {
            SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (udpSocket != INVALID_SOCKET) {
                BOOL reuse = TRUE;
                setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

                DWORD timeoutMs = 5000;
                setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

                std::string filename;
                std::getline(ss, filename);
                
                size_t first = filename.find_first_not_of(" \t\r\n");
                if (first != std::string::npos) {
                    size_t last = filename.find_last_not_of(" \t\r\n");
                    filename = filename.substr(first, (last - first + 1));
                }

                if (upperCmd == "STOR" || upperCmd == "APPE") {
                    std::string clientFilePath = "download/" + filename; 

                    if (fs::exists(clientFilePath)) {
                        rdt_send_file(udpSocket, clientFilePath.c_str(), serverIP.c_str(), 8081);

                        std::string finalReply = client.receiveReply();
                        if (!finalReply.empty()) UIManager::printResponse(finalReply);

                        std::string localHash = calculateLocalMD5(clientFilePath);
                        std::cout << "[CLIENT LOG] Upload Hash (MD5): " << localHash << "\n";
                    } else {
                        std::cout << "[CLIENT LỖI] File không tồn tại trong thư mục download: " << clientFilePath << "\n";
                    }
                } 
                else if (upperCmd == "RETR") {
                    sockaddr_in clientUdpAddr{};
                    clientUdpAddr.sin_family = AF_INET;
                    clientUdpAddr.sin_port = htons(8081);
                    clientUdpAddr.sin_addr.s_addr = INADDR_ANY;

                    if (bind(udpSocket, (sockaddr*)&clientUdpAddr, sizeof(clientUdpAddr)) != SOCKET_ERROR) {
                        std::string savePath = "download/" + filename;
                        
                        long long total_file_size = parseFileSizeFromResponse(response);
                        rdt_receive_file(udpSocket, savePath.c_str(), total_file_size);

                        std::string finalReply = client.receiveReply();
                        if (!finalReply.empty()) UIManager::printResponse(finalReply);

                        std::string downloadedHash = calculateLocalMD5(savePath);
                        std::cout << "[CLIENT INTEGRITY] Downloaded File MD5: " << downloadedHash << "\n";
                    } else {
                        std::cout << "[CLIENT LỖI] Không thể bind Port UDP 8081!\n";
                    }
                }

                closesocket(udpSocket);
            }
        }
        else if (upperCmd == "HASH" && response.rfind("200", 0) == 0) {
            std::stringstream resStream(response);
            std::string code, cmdName, fname, serverHash;
            resStream >> code >> cmdName >> fname >> serverHash;

            std::string localFilePath = "download/" + fname;
            if (fs::exists(localFilePath)) {
                std::string localHash = calculateLocalMD5(localFilePath);
                std::cout << "\n[KIỂM TRA TÍNH TOÀN VẸN DỮ LIỆU MD5]\n";
                std::cout << "  - Server Hash : " << serverHash << "\n";
                std::cout << "  - Client Hash : " << localHash << "\n";
                if (localHash == serverHash) {
                    std::cout << "  => KẾT QUẢ: KHỚP 100% (File toàn vẹn, không bị lỗi truyền dẫn).\n\n";
                } else {
                    std::cout << "  => KẾT QUẢ: KHÔNG KHỚP! (File đã bị thay đổi hoặc truyền lỗi).\n\n";
                }
            }
        }

        if (upperCmd == "QUIT") break;
    }

    client.disconnect();
    std::cout << "Đã đóng kết nối Client.\n";
    return 0;
}