#include "tcp_server.h"
#include "command_handler.h"
#include <iostream>

TCPServer::TCPServer(int port) : port(port), serverSocket(INVALID_SOCKET) {}

TCPServer::~TCPServer() {
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    WSACleanup();
}

void* TCPServer::handleClientPthread(void* arg) {
    ClientThreadArgs* args = static_cast<ClientThreadArgs*>(arg);
    SOCKET clientSocket = args->clientSocket;
    sockaddr_in clientAddr = args->clientAddr;
    delete args;

    CommandHandler handler(clientSocket, clientAddr);
    handler.processCommands();

    closesocket(clientSocket);
    pthread_exit(NULL);
    return NULL;
}

bool TCPServer::start() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[LỖI] Khởi tạo Winsock thất bại!\n";
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[LỖI] Tạo Socket TCP thất bại!\n";
        WSACleanup();
        return false;
    }

    BOOL optVal = TRUE;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Bind Port " << port << " thất bại!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Listen thất bại!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    std::cout << "=============================================\n";
    std::cout << "[TCP SERVER CLASS & PTHREAD] Đang lắng nghe tại Port: " << port << std::endl;
    std::cout << "=============================================\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "[CẢNH BÁO] Accept thất bại!\n";
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(clientAddr.sin_port);

        std::cout << "[KẾT NỐI MỚI] Client " << clientIP << ":" << clientPort << " kết nối thành công!\n";

        ClientThreadArgs* args = new ClientThreadArgs();
        args->clientSocket = clientSocket;
        args->clientAddr = clientAddr;

        pthread_t threadId;
        int rc = pthread_create(&threadId, NULL, TCPServer::handleClientPthread, (void*)args);

        if (rc != 0) {
            std::cerr << "[LỖI] Không thể tạo luồng pthread! Mã lỗi: " << rc << std::endl;
            delete args;
            closesocket(clientSocket);
            continue;
        }

        pthread_detach(threadId);
    }
    return true;
}