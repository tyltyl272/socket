#include "tcp_client.h"
#include <iostream>

TCPClient::TCPClient() : clientSocket(INVALID_SOCKET), isConnected(false) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

TCPClient::~TCPClient() {
    disconnect();
    WSACleanup();
}

bool TCPClient::connectToServer(const std::string& ip, int port) {
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "[LỖI] Không thể tạo Socket Client!\n";
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    isConnected = true;
    return true;
}

bool TCPClient::sendCommand(const std::string& command) {
    if (!isConnected || clientSocket == INVALID_SOCKET) return false;
    std::string msg = command + "\r\n";
    int bytesSent = send(clientSocket, msg.c_str(), (int)msg.length(), 0);
    return bytesSent != SOCKET_ERROR;
}

std::string TCPClient::receiveReply() {
    if (!isConnected || clientSocket == INVALID_SOCKET) return "";
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
        return std::string(buffer);
    } else {
        isConnected = false;
        return "";
    }
}

void TCPClient::disconnect() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    isConnected = false;
}