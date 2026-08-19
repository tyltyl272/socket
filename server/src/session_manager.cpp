#include "session_manager.h"
#include <iostream>
#include <iomanip>

void SessionManager::addSession(SOCKET sock, const std::string& ip, int port) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    ClientSession session{};
    session.socketTCP = sock;
    session.clientIP = ip;
    session.clientPort = port;
    session.username = "Anonymous";
    session.isAuthenticated = false;
    session.currentDir = "./shared_folder"; // Thư mục mặc định
    session.connectTime = std::chrono::steady_clock::now();

    sessions[sock] = session;
}

void SessionManager::removeSession(SOCKET sock) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessions.erase(sock);
}

void SessionManager::updateAuth(SOCKET sock, const std::string& username, bool status) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(sock);
    if (it != sessions.end()) {
        it->second.username = username;
        it->second.isAuthenticated = status;
    }
}

void SessionManager::updateDirectory(SOCKET sock, const std::string& newDir) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(sock);
    if (it != sessions.end()) {
        it->second.currentDir = newDir;
    }
}

bool SessionManager::getSession(SOCKET sock, ClientSession& outSession) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(sock);
    if (it != sessions.end()) {
        outSession = it->second;
        return true;
    }
    return false;
}

void SessionManager::printActiveSessions() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::cout << "\n=================== ACTIVE SESSIONS TABLE (" << sessions.size() << ") ===================\n";
    std::cout << std::left 
              << std::setw(8)  << "SOCKET" 
              << std::setw(18) << "IP ADDRESS" 
              << std::setw(8)  << "PORT" 
              << std::setw(15) << "USERNAME" 
              << std::setw(15) << "STATUS" << "\n";
    std::cout << "------------------------------------------------------------------\n";

    for (const auto& [sock, s] : sessions) {
        std::cout << std::left 
                  << std::setw(8)  << sock 
                  << std::setw(18) << s.clientIP 
                  << std::setw(8)  << s.clientPort 
                  << std::setw(15) << s.username 
                  << std::setw(15) << (s.isAuthenticated ? "Logged In" : "Unauthenticated") << "\n";
    }
    std::cout << "==================================================================\n\n";
}