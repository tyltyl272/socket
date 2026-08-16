#include "tcp_client.h"
#include "ui_manager.h"
#include "rdt.h"
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <csignal>
#include <cstdlib>

namespace fs = std::filesystem;

TCPClient* g_clientPtr = nullptr;

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

    if (argc >= 2) {
        serverIP = argv[1];
    }
    if (argc >= 3) {
        try {
            serverPort = std::stoi(argv[2]);
        } catch (...) {
            serverPort = 0;
        }
    }

    UIManager::printHeader();

    if (serverIP.empty()) {
        std::cout << "Nhập IP Server (Ấn Enter để dùng 127.0.0.1): ";
        std::getline(std::cin, serverIP);
        if (serverIP.empty()) {
            serverIP = "127.0.0.1";
        }
    }

    if (serverPort <= 0 || serverPort > 65535) {
        std::cout << "Nhập Port Server (Ấn Enter để dùng 8888): ";
        std::string inputPort;
        std::getline(std::cin, inputPort);
        if (inputPort.empty()) {
            serverPort = 8888;
        } else {
            try {
                serverPort = std::stoi(inputPort);
            } catch (...) {
                serverPort = 8888;
            }
        }
    }

    std::cout << "Đang kết nối tới Server (" << serverIP << ":" << serverPort << ")..." << std::endl;

    if (!client.connectToServer(serverIP, serverPort)) {
        std::cerr << "[LỖI] Không thể kết nối tới Server! Hãy đảm bảo TCP Server đã chạy.\n";
        system("pause");
        return 1;
    }

    std::string welcomeMsg = client.receiveReply();
    if (!welcomeMsg.empty()) {
        UIManager::printResponse(welcomeMsg);
    }

    std::string inputCommand;
    while (client.checkConnection()) {
        UIManager::printPrompt();
        if (!std::getline(std::cin, inputCommand)) {
            break;
        }

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
        if (!response.empty()) {
            UIManager::printResponse(response);
        }

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
                        rdt_receive_file(udpSocket, savePath.c_str());

                        std::string finalReply = client.receiveReply();
                        if (!finalReply.empty()) UIManager::printResponse(finalReply);
                    } else {
                        std::cout << "[CLIENT LỖI] Không thể bind Port UDP 8081! Mã lỗi: " << WSAGetLastError() << "\n";
                    }
                }

                closesocket(udpSocket);
            }
        }

        if (upperCmd == "QUIT") {
            break;
        }
    }

    client.disconnect();
    std::cout << "Đã đóng kết nối Client.\n";
    return 0;
}