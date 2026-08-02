#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

class TCPClient {
private:
    SOCKET clientSocket;
    bool isConnected;

public:
    TCPClient();
    ~TCPClient();
    bool connectToServer(const std::string& ip, int port);
    bool sendCommand(const std::string& command);
    std::string receiveReply();
    void disconnect();
    bool checkConnection() const { return isConnected; }
};

#endif