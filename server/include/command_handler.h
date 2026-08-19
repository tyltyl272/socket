#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
    #define BOOL bool
    #define TRUE true
    #define FALSE false
#endif

#include <string>

class CommandHandler {
private:
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    std::string clientIP;
    std::string currentUsername;
    bool isAuthenticated;
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
    void handleQUIT();
    void handleTYPE(const std::string& param);
    void handleMODE(const std::string& param);
    void handleSTAT(const std::string& param);
    void handleABOR();
    void handlePORT(const std::string& param);
    void handlePASV();
    void handleRETR(const std::string& param);
    void handleSTOR(const std::string& param);
    void handleAPPE(const std::string& param);
    void handleHASH(const std::string& param);

public:
    CommandHandler(SOCKET socket, sockaddr_in addr);
    ~CommandHandler();
    void processCommands();
};

#endif