# HYBRID FTP PROTOCOL SYSTEM (TCP Control + UDP Data)

Hệ thống truyền nhận tệp lai (Hybrid FTP) triển khai mô hình Client-Server đa luồng (Multi-threaded). Chương trình kết hợp ưu điểm của kênh điều khiển TCP tin cậy và kênh truyền dữ liệu UDP hiệu năng cao tích hợp giao thức truyền tin cậy RDT (Reliable Data Transfer).

---

## 1. TỔNG QUAN HỆ THỐNG

### Kiến trúc Hybrid Protocol
* **TCP Control Channel (Default Port 8080):** Đảm nhận việc truyền nhận các câu lệnh điều khiển (USER, PASS, LIST, CWD, DELE, RNFR/RNTO,...), quản lý phiên làm việc, xác thực người dùng và bắt tay thống nhất tham số truyền tải.
* **UDP Data Channel:** Đảm nhận công việc truyền tải tệp tin (RETR, STOR). Dữ liệu được đóng gói và kiểm soát độ tin cậy thông qua thuật toán RDT (Reliable Data Transfer) tích hợp cơ chế Checksum 16-bit (RFC 1071) và Timeout/Retransmit.

### Công nghệ & Thư viện sử dụng
* **Ngôn ngữ lập trình:** C++ (C++17 trở lên).
* **Mạng & Socket:** `Winsock2` (`ws2_32.lib`) cho môi trường Windows / `sys/socket.h` cho Linux POSIX.
* **Đa luồng (Multithreading):** `pthread` (POSIX Threads) cho phép Server phục vụ đồng thời nhiều Client.
* **Hệ thống tệp (Filesystem):** `std::filesystem` để thao tác duyệt thư mục, kiểm tra kích thước và thuộc tính file.

---

## 2. CẤU TRÚC THƯ MỤC DỰ ÁN

```text
Hybrid-FTP/
├── client/                      --> Toàn bộ code phía Client
│   ├── download/                --> Thư mục chứa file sau khi Client tải về
│   ├── include/                 --> Chứa các file khai báo (.h) phía Client
│   │   ├── tcp_client.h         --> Khai báo kết nối TCP Client
│   │   └── ui_manager.h         --> Khai báo giao diện CLI Client
│   ├── src/                     --> Chứa các file triển khai (.cpp) phía Client
│   │   ├── main_client.cpp      --> Hàm main() khởi chạy ứng dụng Client
│   │   ├── rdt_receiver.cpp     --> Logic nhận file RDT over UDP phía Client
│   │   ├── tcp_client.cpp       --> Code gửi/nhận lệnh qua TCP Client
│   │   └── ui_manager.cpp       --> Code vẽ giao diện CLI Client
│   └── client.exe               --> File thực thi Client sau khi biên dịch
│
├── server/                      --> Toàn bộ code phía Server
│   ├── include/                 --> Chứa các file khai báo (.h) phía Server
│   │   ├── command_handler.h    --> Khai báo bộ xử lý các lệnh FTP
│   │   └── tcp_server.h         --> Khai báo Socket TCP Server & luồng
│   ├── src/                     --> Chứa các file triển khai (.cpp) phía Server
│   │   ├── command_handler.cpp  --> Code logic xử lý các lệnh FTP
│   │   ├── main_server.cpp      --> Hàm main() khởi chạy Server
│   │   ├── rdt_sender.cpp       --> Code truyền file bằng UDP RDT phía Server
│   │   └── tcp_server.cpp       --> Code Socket TCP listen/accept (pthread)
│   ├── storage/                 --> Thư mục chứa các file thực tế nằm trên Server
│   └── server.exe               --> File thực thi Server sau khi biên dịch
│
├── include/                     --> Thư mục chứa Header dùng chung
│   └── rdt.h                    --> Khai báo cấu trúc gói tin RDTHeader, RDTPacket
│
└── src/common/                  --> Chứa code hàm tiện ích dùng chung
    └── utils.cpp                --> Thuật toán tính Checksum 16-bit RFC 1071

## HƯỚNG DẪN BIÊN DỊCH VÀ CHẠY CHƯƠNG TRÌNH

### Yêu cầu môi trường
* **Hệ điều hành:** Windows hoặc Linux (Ubuntu/Debian).
* **Trình biên dịch:** `g++` (MinGW-w64 trên Windows hoặc GCC trên Linux) hỗ trợ tiêu chuẩn **C++17** trở lên.
* **Thư viện liên kết (Windows):** `ws2_32` (Winsock) và `pthread` (POSIX Threads).

---

### Biên dịch chương trình (Sử dụng Terminal / CMD)

Mở Terminal tại thư mục gốc của dự án (`Hybrid-FTP/`):

#### A. Biên dịch Server
Chạy lệnh sau để biên dịch toàn bộ source code của Server thành file thực thi `server.exe` nằm trong thư mục:

`server/`:
g++ -std=c++17 -Iinclude -Iserver/include -Iclient/include server/src/*.cpp client/src/rdt_receiver.cpp src/common/*.cpp -o server/server.exe -lws2_32 -lpthread

`client/`:
g++ -std=c++17 -Iinclude -Iclient/include -Iserver/include client/src/*.cpp server/src/rdt_sender.cpp src/common/*.cpp -o client/client.exe -lws2_32
