#include "tcp_server.h"
#include "command_handler.h"
#include "session_manager.h"
#include <iostream>
#include <algorithm>

extern SessionManager g_sessionManager;

TCPServer::TCPServer(int port) : port(port), serverSocket(INVALID_SOCKET), isRunning(false) {
    pthread_mutex_init(&clientsMutex, NULL);

    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "[LỖI] WSAStartup thất bại! Mã lỗi: " << res << std::endl;
    }
}

TCPServer::~TCPServer() {
    stop();
    pthread_mutex_destroy(&clientsMutex);

    WSACleanup();
}

void TCPServer::stop() {
    if (!isRunning && serverSocket == INVALID_SOCKET) return;

    isRunning = false;

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    pthread_mutex_lock(&clientsMutex);
    for (SOCKET clientSocket : activeClientSockets) {
        if (clientSocket != INVALID_SOCKET) {
            shutdown(clientSocket, SD_BOTH);
            closesocket(clientSocket);
        }
    }
    activeClientSockets.clear();
    pthread_mutex_unlock(&clientsMutex);
}

void* TCPServer::handleClientPthread(void* arg) {
    ClientThreadArgs* args = static_cast<ClientThreadArgs*>(arg);
    SOCKET clientSocket = args->clientSocket;
    sockaddr_in clientAddr = args->clientAddr;
    TCPServer* serverPtr = args->serverInstance;
    delete args;

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddr.sin_port);
    std::string clientKey = std::string(clientIP) + ":" + std::to_string(clientPort);

    if (serverPtr) {
        pthread_mutex_lock(&(serverPtr->clientsMutex));
        serverPtr->activeClientSockets.push_back(clientSocket);
        pthread_mutex_unlock(&(serverPtr->clientsMutex));
    }

    // 1. Thêm session mới và in bảng ngay khi accept kết nối
    g_sessionManager.addSession(clientSocket, clientIP, clientPort);
    g_sessionManager.printActiveSessions();

    CommandHandler handler(clientSocket, clientAddr);
    handler.processCommands();

    std::cout << "[KẾT NỐI ĐÓNG] Client " << clientKey << " đã ngắt kết nối.\n";

    // 2. Xóa session và in lại bảng khi client ngắt kết nối
    g_sessionManager.removeSession(clientSocket);
    g_sessionManager.printActiveSessions();

    closesocket(clientSocket);

    if (serverPtr) {
        pthread_mutex_lock(&(serverPtr->clientsMutex));
        auto it = std::find(serverPtr->activeClientSockets.begin(), serverPtr->activeClientSockets.end(), clientSocket);
        if (it != serverPtr->activeClientSockets.end()) {
            serverPtr->activeClientSockets.erase(it);
        }
        pthread_mutex_unlock(&(serverPtr->clientsMutex));
    }

    pthread_exit(NULL);
    return NULL;
}

bool TCPServer::start() {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[LỖI] Tạo Socket TCP thất bại!\n";
        return false;
    }

    BOOL optVal = TRUE;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Bind Port " << port << " thất bại!" << std::endl;
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[LỖI] Listen thất bại!" << std::endl;
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
        return false;
    }

    isRunning = true;
    std::cout << "=============================================\n";
    std::cout << "[TCP SERVER CONTROL CHANNEL] Đang lắng nghe tại Port: " << port << std::endl;
    std::cout << "=============================================\n";

    while (isRunning) {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            if (!isRunning) break;
            std::cerr << "[CẢNH BÁO] Accept thất bại hoặc Socket bị đóng!\n";
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(clientAddr.sin_port);

        std::cout << "[KẾT NỐI MỚI] Client " << clientIP << ":" << clientPort << " kết nối thành công!\n";

        ClientThreadArgs* args = new ClientThreadArgs();
        args->clientSocket = clientSocket;
        args->clientAddr = clientAddr;
        args->serverInstance = this;

        pthread_t threadId;
        int rc = pthread_create(&threadId, NULL, TCPServer::handleClientPthread, (void*)args);

        if (rc != 0) {
            std::cerr << "[LỖI] Không thể tạo luồng pthread! Mã lỗi: " << rc << std::endl;
            delete args;
            closesocket(clientSocket);
        } else {
            pthread_detach(threadId);
        }
    }

    return true;
}