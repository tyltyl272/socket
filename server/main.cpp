#include "tcp_server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

TCPServer* g_serverPtr = nullptr;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\n==================================================" << std::endl;
        std::cout << "[HỆ THỐNG] Nhận tín hiệu ngắt Ctrl+C từ người dùng." << std::endl;
        std::cout << "[HỆ THỐNG] Đang đóng Socket và giải phóng tài nguyên Server..." << std::endl;

        if (g_serverPtr != nullptr) {
            delete g_serverPtr;
            g_serverPtr = nullptr;
        }

        std::cout << "[HỆ THỐNG] Server đã dừng an toàn. Tạm biệt!" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::exit(EXIT_SUCCESS);
    }
}

int main() {
    std::signal(SIGINT, handleSignal);

    const int PORT = 8080;
    std::cout << "==================================================" << std::endl;
    std::cout << "     HYBRID FTP SERVER - TCP CONTROL CHANNEL      " << std::endl;
    std::cout << "     System: POSIX Threads (pthread) Multi-thread " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "[KHỞI ĐỘNG] Khởi tạo TCP Server tại Port: " << PORT << "..." << std::endl;

    g_serverPtr = new TCPServer(PORT);

    if (!g_serverPtr->start()) {
        std::cerr << "[LỖI TẬP TRUNG] Không thể khởi chạy Server!" << std::endl;
        delete g_serverPtr;
        g_serverPtr = nullptr;
        return EXIT_FAILURE;
    }

    if (g_serverPtr != nullptr) {
        delete g_serverPtr;
        g_serverPtr = nullptr;
    }

    return EXIT_SUCCESS;
}