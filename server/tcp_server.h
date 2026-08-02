#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>

struct ClientThreadArgs {
    SOCKET clientSocket;
    sockaddr_in clientAddr;
};

class TCPServer {
private:
    int port;
    SOCKET serverSocket;
    static void* handleClientPthread(void* arg);

public:
    TCPServer(int port);
    ~TCPServer();
    bool start();
};

#endif