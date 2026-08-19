#include "tcp_client.h"
#include "rdt.h"
#include <iostream>
#include <sstream>
#include <iomanip>
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

static std::string formatSizeHelper(long long bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    std::stringstream ss;
    if (bytes < 1024 * 1024) {
        ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return ss.str();
    }
    if (bytes < 1024LL * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
        return ss.str();
    }
    ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    return ss.str();
}

void TCPClient::handleRetrCommand(const std::string& filename) {
    if (!isConnected || clientSocket == INVALID_SOCKET) return;

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "[LỖI] Không thể tạo UDP Socket!\n";
        return;
    }

    sockaddr_in clientUdpAddr{};
    clientUdpAddr.sin_family = AF_INET;
    clientUdpAddr.sin_addr.s_addr = INADDR_ANY;
    clientUdpAddr.sin_port = htons(0);
    bind(udpSocket, (struct sockaddr*)&clientUdpAddr, sizeof(clientUdpAddr));

    int addrLen = sizeof(clientUdpAddr);
    getsockname(udpSocket, (struct sockaddr*)&clientUdpAddr, &addrLen);
    int localUdpPort = ntohs(clientUdpAddr.sin_port);

    std::string cmd = "RETR " + filename + " " + std::to_string(localUdpPort);
    if (!sendCommand(cmd)) {
        closesocket(udpSocket);
        return;
    }

    std::string response = receiveReply();
    if (response.empty()) {
        std::cerr << "[LỖI] Phản hồi từ Server bị trống!\n";
        closesocket(udpSocket);
        return;
    }

    long long fileSize = 0;
    size_t openParen = response.find('(');
    size_t closeParen = response.find(" bytes)", openParen);
    if (openParen != std::string::npos && closeParen != std::string::npos) {
        std::string sizeStr = response.substr(openParen + 1, closeParen - openParen - 1);
        try { fileSize = std::stoll(sizeStr); } catch (...) { fileSize = 0; }
    }

    if (fileSize > 0) {
        std::cout << "150 Opening UDP Data connection for " << filename << " (" << formatSizeHelper(fileSize) << ")\n";
    } else {
        std::cout << response;
    }

    if (response.rfind("150", 0) == 0 || response[0] == '1') {
        bool success = rdt_receive_file(udpSocket, filename.c_str(), fileSize);
        if (success) {
            std::string finalReply = receiveReply();
            if (!finalReply.empty()) {
                std::cout << finalReply;
            }
        }
    }

    closesocket(udpSocket);
}