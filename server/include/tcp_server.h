#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class TCPServer;

struct ClientThreadArgs {
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    TCPServer* serverInstance;
};

class TCPServer {
private:
    int port;
    SOCKET serverSocket;
    bool isRunning;
    
    std::vector<SOCKET> activeClientSockets;
    pthread_mutex_t clientsMutex;

    static void* handleClientPthread(void* arg);

public:
    TCPServer(int port = 8888);
    ~TCPServer();

    bool start();
    void stop();
};

#endif