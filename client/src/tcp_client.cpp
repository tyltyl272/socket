#include "tcp_client.h"
#include <iostream>
#include <ws2tcpip.h>

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

    char nodelayOpt = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &nodelayOpt, sizeof(nodelayOpt));

    DWORD timeout = 3000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

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

    std::string fullResponse = "";
    char buffer[2048];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0) {
            fullResponse += buffer;
            if (fullResponse.find('\n') != std::string::npos) {
                break;
            }
        } else if (bytesReceived == 0) {
            isConnected = false;
            break;
        } else {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                if (!fullResponse.empty()) break;
                return "";
            }
            isConnected = false;
            break;
        }
    }

    return fullResponse;
}

void TCPClient::disconnect() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    isConnected = false;
}

bool TCPClient::checkConnection() {
    if (!isConnected || clientSocket == INVALID_SOCKET) {
        return false;
    }

    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    char dummyBuffer;
    int res = recv(clientSocket, &dummyBuffer, 1, MSG_PEEK);
    int err = WSAGetLastError();

    mode = 0;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    if (res == 0) {
        isConnected = false;
        return false;
    } 
    else if (res == SOCKET_ERROR) {
        if (err != WSAEWOULDBLOCK && err != WSAETIMEDOUT) {
            isConnected = false;
            return false;
        }
    }

    return true;
}