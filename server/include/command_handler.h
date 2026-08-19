#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

class CommandHandler {
public:
    CommandHandler(SOCKET socket, sockaddr_in addr);
    ~CommandHandler();

    void processCommands();

private:
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    std::string clientIP;
    bool isAuthenticated;
    std::string currentUsername;
    std::string currentDir;
    std::string renameFromPath;
    int dataUdpPort;
    bool isPassiveMode;

    void sendReply(const std::string& reply);
    std::string trim(const std::string& str);
    std::string calculateFileHash(const std::string& filePath);

    void handleUSER(const std::string& param);
    void handlePASS(const std::string& param);
    void handlePWD();
    void handleCWD(const std::string& param);
    void handleLIST();
    void handleTYPE(const std::string& param);
    void handleMODE(const std::string& param);
    void handleSTAT(const std::string& param);
    void handleABOR();
    void handlePORT(const std::string& param);
    void handlePASV();
    void handleRETR(const std::string& param);
    void handleSTOR(const std::string& param);
    void handleAPPE(const std::string& param);
    void handleQUIT();
};

#endif // COMMAND_HANDLER_H