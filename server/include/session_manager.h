#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <winsock2.h>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

// Cấu trúc lưu trữ thông tin của 1 Client Session
struct ClientSession {
    SOCKET socketTCP;
    std::string clientIP;
    int clientPort;
    std::string username;
    bool isAuthenticated;
    std::string currentDir;
    std::chrono::steady_clock::time_point connectTime;
};

// Class quản lý danh sách các Session
class SessionManager {
private:
    std::map<SOCKET, ClientSession> sessions;
    std::mutex sessionMutex;

public:
    // Thêm một session mới khi client kết nối TCP
    void addSession(SOCKET sock, const std::string& ip, int port);

    // Xóa session khi client ngắt kết nối
    void removeSession(SOCKET sock);

    // Cập nhật thông tin đăng nhập (USER/PASS)
    void updateAuth(SOCKET sock, const std::string& username, bool status);

    // Cập nhật thư mục làm việc hiện tại (CWD)
    void updateDirectory(SOCKET sock, const std::string& newDir);

    // Lấy thông tin session của một socket
    bool getSession(SOCKET sock, ClientSession& outSession);

    // In bảng Active Sessions ra màn hình Console Server
    void printActiveSessions();
};

#endif // SESSION_MANAGER_H