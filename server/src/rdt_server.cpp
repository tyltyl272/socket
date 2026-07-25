#include "../include/rdt_server.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

#define WINDOW_SIZE 4       // Kích thước cửa sổ trượt (số gói gửi đi trước khi đợi ACK)
#define TIMEOUT_MS 500      // Thời gian timeout (milliseconds)

uint16_t calculate_checksum(const char* data, int length) {
    uint32_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += (uint8_t)data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

bool rdt_send_file(SOCKET sockfd, sockaddr_in& client_addr, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Server RDT] Không thể mở file để gửi: " << filepath << std::endl;
        return false;
    }

    // Đọc toàn bộ file vào bộ nhớ đệm
    std::vector<std::vector<char>> packets;
    while (!file.eof()) {
        std::vector<char> buffer(1024);
        file.read(buffer.data(), buffer.size());
        std::streamsize bytes_read = file.gcount();
        if (bytes_read > 0) {
            buffer.resize(bytes_read);
            packets.push_back(buffer);
        }
    }
    file.close();

    int total_packets = packets.size();
    int base = 0;          // Cạnh dưới cửa sổ trượt
    int next_seq_num = 0;  // Gói tiếp theo sẽ được gửi
    int addr_len = sizeof(client_addr);

    // Đặt socket sang chế độ không chặn (non-blocking) hoặc dùng select() để timeout ACK
    u_long mode = 1;
    ioctlsocket(sockfd, FIONBIO, &mode);

    std::cout << "[Server RDT] Bắt đầu truyền " << total_packets << " gói tin bằng Go-Back-N..." << std::endl;

    while (base < total_packets) {
        // 1. Gửi các gói trong giới hạn cửa sổ trượt
        while (next_seq_num < base + WINDOW_SIZE && next_seq_num < total_packets) {
            RDTPacket pkt;
            pkt.header.seq_num = next_seq_num;
            pkt.header.length = packets[next_seq_num].size();
            memcpy(pkt.data, packets[next_seq_num].data(), pkt.header.length);
            pkt.header.checksum = calculate_checksum(pkt.data, pkt.header.length);
            pkt.header.flags = 0;

            sendto(sockfd, (char*)&pkt, sizeof(RDTHeader) + pkt.header.length, 0, (struct sockaddr*)&client_addr, addr_len);
            std::cout << "[Server RDT] Đã gửi gói số: " << next_seq_num << std::endl;
            next_seq_num++;
        }

        // 2. Chờ nhận phản hồi ACK từ Client
        RDTPacket ack_pkt;
        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            int bytes_in = recvfrom(sockfd, (char*)&ack_pkt, sizeof(RDTPacket), 0, nullptr, nullptr);
            if (bytes_in > 0) {
                // Nhận được ACK hợp lệ
                if (ack_pkt.header.flags == 1) { // Cờ 1 biểu thị là gói ACK
                    std::cout << "[Server RDT] Nhận ACK cho gói: " << ack_pkt.header.ack_num << std::endl;
                    base = ack_pkt.header.ack_num + 1; // Trượt cửa sổ lên
                    break;
                }
            }

            // Kiểm tra timeout (nếu quá thời gian mà chưa nhận được ACK -> Trôi thời gian)
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
            if (elapsed > TIMEOUT_MS) {
                std::cout << "[Server RDT] Timeout! Gửi lại từ gói số: " << base << std::endl;
                next_seq_num = base; // Quay lại truyền lại từ đầu cửa sổ (Go-Back-N)
                break;
            }
        }
    }

    // Gửi gói tin kết thúc (FIN) báo hiệu hoàn tất truyền file
    RDTPacket fin_pkt;
    fin_pkt.header.seq_num = total_packets;
    fin_pkt.header.length = 0;
    fin_pkt.header.flags = 2; // Cờ 2 biểu thị kết thúc
    sendto(sockfd, (char*)&fin_pkt, sizeof(RDTHeader), 0, (struct sockaddr*)&client_addr, addr_len);

    // Trả socket về chế độ chặn bình thường (blocking)
    mode = 0;
    ioctlsocket(sockfd, FIONBIO, &mode);

    std::cout << "[Server RDT] Truyền file hoàn tất thành công!" << std::endl;
    return true;
}