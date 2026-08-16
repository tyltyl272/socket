#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

class CommandHandler {
private:
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    std::string clientIP;
    bool isAuthenticated;
    std::string currentUsername;
    std::string currentDir;
    std::string renameFromPath;
    int dataUdpPort;

    void sendReply(const std::string& reply);
    std::string trim(const std::string& str);
    std::string calculateFileHash(const std::string& filePath);

    void handleUSER(const std::string& param);
    void handlePASS(const std::string& param);
    void handlePWD();
    void handleCWD(const std::string& param);
    void handleLIST();
    void handleQUIT();
    void handleRETR(const std::string& param);
    void handleSTOR(const std::string& param);

public:
    CommandHandler(SOCKET socket, sockaddr_in addr);
    ~CommandHandler();
    void processCommands();
};

#endif