#include "ui_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <mutex>
#include <windows.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RED     "\033[1;31m"

static void enableVTModeOnce() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
    });
}

void UIManager::printHeader() {
    enableVTModeOnce();
    std::cout << "===========================================\n";
    std::cout << "        HYBRID FTP CLIENT TCP CONTROL      \n";
    std::cout << "===========================================\n";
    std::cout << " Gõ 'HELP' để xem danh sách các lệnh hỗ trợ.\n\n" << std::flush;
}

void UIManager::printPrompt() {
    std::cout << "ftp> " << std::flush;
}

void UIManager::printHelp() {
    std::cout << "\n------------------- DANH SÁCH LỆNH HỖ TRỢ -------------------\n";
    std::cout << " [XÁC THỰC & HỆ THỐNG]\n";
    std::cout << "  USER <username>   : Đăng nhập với tên người dùng\n";
    std::cout << "  PASS <password>   : Nhập mật khẩu đăng nhập\n";
    std::cout << "  NOOP              : Kiểm tra duy trì kết nối (No Operation)\n";
    std::cout << "  QUIT              : Thoát chương trình\n\n";

    std::cout << " [QUẢN LÝ THƯ MỤC & FILE]\n";
    std::cout << "  PWD               : Hiển thị đường dẫn thư mục hiện tại\n";
    std::cout << "  CWD <folder>      : Chuyển sang thư mục con\n";
    std::cout << "  CDUP              : Quay lại thư mục cha\n";
    std::cout << "  LIST              : Xem danh sách file & thư mục chi tiết\n";
    std::cout << "  NLST              : Xem danh sách tên file dạng rút gọn\n";
    std::cout << "  MKD <folder>      : Tạo thư mục mới\n";
    std::cout << "  RMD <folder>      : Xóa thư mục rỗng\n";
    std::cout << "  DELE <filename>   : Xóa file trên Server\n";
    std::cout << "  RNFR <old_name>   : Bắt đầu đổi tên file (Chỉ định file cũ)\n";
    std::cout << "  RNTO <new_name>   : Hoàn tất đổi tên file (Chỉ định tên mới)\n\n";

    std::cout << " [TRUYỀN DỮ LIỆU & KIỂM TRA (UDP)]\n";
    std::cout << "  RETR <filename>   : Tải file từ Server về Client (Qua UDP)\n";
    std::cout << "  STOR <filename>   : Upload file mới lên Server (Qua UDP)\n";
    std::cout << "  APPE <filename>   : Ghi nối tiếp dữ liệu vào file có sẵn (Qua UDP)\n";
    std::cout << "  STOU              : Upload file với tên tự động ngẫu nhiên\n";
    std::cout << "  ABOR              : Hủy tiến trình truyền tải dữ liệu\n";
    std::cout << "  SIZE <filename>   : Xem kích thước file (bytes)\n";
    std::cout << "  HASH <filename>   : Lấy mã Checksum Hash kiểm tra toàn vẹn\n";
    std::cout << "  MDTM <filename>   : Xem thời gian sửa đổi file lần cuối\n\n";

    std::cout << " [CẤU HÌNH KÊNH TRUYỀN]\n";
    std::cout << "  TYPE <A/I>        : Đặt kiểu truyền (A: ASCII, I: Binary)\n";
    std::cout << "  MODE <S/C/B>      : Đặt chế độ truyền dữ liệu\n";
    std::cout << "  PASV / PORT       : Chuyển đổi chế độ Passive / Active\n";
    std::cout << "  STAT              : Xem trạng thái hoạt động của Server\n";
    std::cout << "--------------------------------------------------------------\n\n" << std::flush;
}

static std::string formatSize(long long bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return ss.str();
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
    return ss.str();
}

void UIManager::printProgressBar(long long currentBytes, long long totalBytes) {
    if (totalBytes <= 0) return;
    
    int barWidth = 35;
    float progress = static_cast<float>(currentBytes) / totalBytes;
    if (progress > 1.0f) progress = 1.0f;
    
    int pos = static_cast<int>(barWidth * progress);

    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    
    std::cout << "] " << std::setw(3) << static_cast<int>(progress * 100.0f) << "% ("
              << formatSize(currentBytes) << " / " << formatSize(totalBytes) << ")" 
              << std::flush;

    if (currentBytes >= totalBytes) {
        std::cout << " - Hoàn tất!\n" << std::flush;
    }
}

void UIManager::printResponse(const std::string& response) {
    if (response.empty()) return;

    enableVTModeOnce();

    int code = 0;
    size_t firstDigit = response.find_first_of("0123456789");
    if (firstDigit != std::string::npos && firstDigit + 2 < response.length()) {
        try {
            code = std::stoi(response.substr(firstDigit, 3));
        } catch (...) {
            code = 0;
        }
    }

    if (code >= 200 && code < 300) {
        std::cout << COLOR_GREEN;
    } 
    else if (code >= 300 && code < 400) {
        std::cout << COLOR_YELLOW;
    } 
    else if (code >= 400 && code < 600) {
        std::cout << COLOR_RED;
    } 
    else {
        std::cout << COLOR_RESET;
    }

    std::cout << response;
    if (response.back() != '\n') {
        std::cout << "\n";
    }

    std::cout << COLOR_RESET << std::flush;
}