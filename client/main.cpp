#include "tcp_client.h"
#include "ui_manager.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    TCPClient client;
    std::string serverIP = "";
    int serverPort = 0;

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
        std::cout << welcomeMsg;
    }

    std::string inputCommand;
    while (client.checkConnection()) {
        UIManager::printPrompt();
        std::getline(std::cin, inputCommand);

        if (inputCommand.empty()) continue;

        if (inputCommand == "HELP" || inputCommand == "help") {
            UIManager::printHelp();
            continue;
        }

        if (!client.sendCommand(inputCommand)) {
            std::cout << "[LỖI] Mất kết nối tới Server!\n";
            break;
        }

        std::string response = client.receiveReply();
        if (!response.empty()) {
            std::cout << response;
        }

        if (inputCommand == "QUIT" || inputCommand == "quit") {
            break;
        }
    }

    std::cout << "Đã đóng kết nối Client.\n";
    return 0;
}