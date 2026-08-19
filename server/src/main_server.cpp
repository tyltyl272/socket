#include "tcp_server.h"
#include "session_manager.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

volatile std::sig_atomic_t g_running = 1;
TCPServer* g_serverPtr = nullptr;

// Đối tượng quản lý Session toàn cục
SessionManager g_sessionManager;

void handleSignal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = 0;
        if (g_serverPtr != nullptr) {
            g_serverPtr->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // Tự động tạo thư mục chứa dữ liệu nếu chưa tồn tại
    if (!fs::exists("storage")) {
        fs::create_directories("storage");
    }

    int port = 0;

    // Đọc Port từ tham số dòng lệnh (nếu có)
    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            port = 0;
        }
    }

    // Nếu không nhập Port từ tham số, yêu cầu nhập từ bàn phím
    if (port < 1024 || port > 65535) {
        std::cout << "==================================================" << std::endl;
        std::cout << "    HYBRID FTP SERVER - TCP CONTROL CHANNEL      " << std::endl;
        std::cout << "    System: POSIX Threads (pthread) Multi-thread " << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "Nhập số Port lắng nghe (Ấn Enter để dùng mặc định 8888): ";
        
        std::string inputPort;
        std::getline(std::cin, inputPort);

        if (inputPort.empty()) {
            port = 8888;
        } else {
            try {
                port = std::stoi(inputPort);
            } catch (...) {
                port = 8888;
            }
        }
    } else {
        std::cout << "==================================================" << std::endl;
        std::cout << "    HYBRID FTP SERVER - TCP CONTROL CHANNEL      " << std::endl;
        std::cout << "    System: POSIX Threads (pthread) Multi-thread " << std::endl;
        std::cout << "==================================================" << std::endl;
    }

    // Kiểm tra tính hợp lệ của Port
    if (port < 1024 || port > 65535) {
        std::cout << "[CẢNH BÁO] Port không hợp lệ hoặc thuộc dải Privileged (1-1023). Tự động đặt về 8888.\n";
        port = 8888;
    }

    std::cout << "[KHỞI ĐỘNG] Khởi tạo TCP Server tại Port: " << port << "..." << std::endl;

    g_serverPtr = new TCPServer(port);

    // Hàm start() sẽ đứng yên lắng nghe kết nối và xử lý multithread bên trong TCPServer
    if (!g_serverPtr->start()) {
        std::cerr << "[LỖI TẬP TRUNG] Không thể khởi chạy Server!" << std::endl;
        delete g_serverPtr;
        g_serverPtr = nullptr;
        return EXIT_FAILURE;
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << "[HỆ THỐNG] Đang giải phóng tài nguyên Server..." << std::endl;

    if (g_serverPtr != nullptr) {
        delete g_serverPtr;
        g_serverPtr = nullptr;
    }

    std::cout << "[HỆ THỐNG] Server đã dừng an toàn. Tạm biệt!" << std::endl;
    std::cout << "==================================================" << std::endl;

    return EXIT_SUCCESS;
}