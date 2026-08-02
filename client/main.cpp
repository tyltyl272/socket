#include "tcp_client.h"
#include "ui_manager.h"
#include <iostream>
#include <string>

int main() {
    TCPClient client;
    std::string serverIP = "127.0.0.1";
    int serverPort = 8080;

    UIManager::printHeader();
    std::cout << "Đang kết nối tới Server (" << serverIP << ":" << serverPort << ")..." << std::endl;

    if (!client.connectToServer(serverIP, serverPort)) {
        std::cerr << "[LỖI] Không thể kết nối tới Server! Hãy đảm bảo TCP Server đã chạy.\n";
        system("pause");
        return 1;
    }

    // Nhận mã chào mừng (220) từ server
    std::cout << client.receiveReply();

    std::string inputCommand;
    while (client.checkConnection()) {
        UIManager::printPrompt();
        std::getline(std::cin, inputCommand);

        if (inputCommand.empty()) continue;

        if (inputCommand == "HELP" || inputCommand == "help") {
            UIManager::printHelp();
            continue;
        }

        // Gửi lệnh qua TCP
        if (!client.sendCommand(inputCommand)) {
            std::cout << "[LỖI] Mất kết nối tới Server!\n";
            break;
        }

        // Nhận phản hồi từ server
        std::string response = client.receiveReply();
        std::cout << response;

        // Lệnh QUIT -> ngắt vòng lặp
        if (inputCommand == "QUIT" || inputCommand == "quit") {
            break;
        }
    }

    std::cout << "Đã đóng kết nối Client.\n";
    return 0;
}