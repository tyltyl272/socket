#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

class CommandHandler {
private:
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    bool isAuthenticated;
    std::string currentUsername;
    std::string currentDir;
    std::string renameFromPath;
    int dataUdpPort;

    void sendReply(const std::string& reply);
    std::string trim(const std::string& str);
    void handleUSER(const std::string& param);
    void handlePASS(const std::string& param);
    void handlePWD();
    void handleCWD(const std::string& param);
    void handleLIST();
    void handleQUIT();
    void handleRETR(const std::string& param);
    void handleSTOR(const std::string& param);
    std::string calculateFileHash(const std::string& filePath);

public:
    CommandHandler(SOCKET socket, sockaddr_in addr);
    ~CommandHandler();
    void processCommands();
};

#endif