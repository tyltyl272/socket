#include "../include/rdt_client.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

// Khai báo hàm checksum chuẩn đã có trong utils.cpp của nhóm
uint16_t calculate_checksum(const void* data, size_t length);

bool rdt_receive_file(SOCKET sockfd, sockaddr_in& server_addr, const std::string& output_filepath) {
    std::ofstream outfile(output_filepath, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "[Client RDT] Không thể tạo file để lưu: " << output_filepath << std::endl;
        return false;
    }

    uint32_t expected_seq_num = 0;                        // Số thứ tự gói mong đợi tiếp theo
    int server_addr_len = sizeof(server_addr);

    std::cout << "[Client RDT] Đang chờ nhận file từ Server (Go-Back-N)..." << std::endl;

    while (true) {
        RDTPacket pkt;
        int bytes_in = recvfrom(sockfd, (char*)&pkt, sizeof(RDTPacket), 0, (struct sockaddr*)&server_addr, &server_addr_len);

        if (bytes_in > 0) {
            // 1. Kiểm tra xem có phải gói tin kết thúc (FIN) không (Cờ flags == 2)
            if (pkt.header.flags == 2) {
                std::cout << "[Client RDT] Nhận tín hiệu kết thúc (FIN) từ Server." << std::endl;
                break;
            }

            // 2. Kiểm tra tính toàn vẹn dữ liệu bằng Checksum chuẩn từ utils.cpp
            // Tạm thời đặt checksum trong header về 0 để tính toán chính xác gói tin
            RDTHeader temp_header = pkt.header;
            temp_header.checksum = 0;
            
            size_t total_size = sizeof(RDTHeader) + pkt.header.length;
            char temp_buffer[sizeof(RDTPacket)];
            memcpy(temp_buffer, &temp_header, sizeof(RDTHeader));
            memcpy(temp_buffer + sizeof(RDTHeader), pkt.data, pkt.header.length);

            uint16_t computed_checksum = calculate_checksum(temp_buffer, total_size);

            if (computed_checksum != pkt.header.checksum) {
                std::cout << "[Client RDT] Cảnh báo: Lỗi Checksum ở gói số " << pkt.header.seq_num << "! Bỏ qua gói." << std::endl;
                continue; // Sai checksum thì bỏ qua, không gửi ACK để Server timeout gửi lại
            }

            // 3. Nếu đúng gói mong đợi theo cơ chế Go-Back-N
            if (pkt.header.seq_num == expected_seq_num) {
                if (pkt.header.length > 0) {
                    outfile.write(pkt.data, pkt.header.length);
                }
                std::cout << "[Client RDT] Đã nhận và ghi gói chuẩn: " << expected_seq_num << std::endl;
                expected_seq_num++;
            } else {
                std::cout << "[Client RDT] Nhận sai thứ tự gói (Mong đợi: " << expected_seq_num 
                          << ", Nhận được: " << pkt.header.seq_num << "). Gửi lại ACK cũ." << std::endl;
            }

            // 4. Gửi lại gói ACK xác nhận cho Server (Cumulative ACK của Go-Back-N)
            RDTPacket ack_pkt;
            ack_pkt.header.ack_num = expected_seq_num - 1;
            ack_pkt.header.flags = 1; // Cờ ACK
            ack_pkt.header.checksum = calculate_checksum(&ack_pkt.header, sizeof(RDTHeader));
            
            sendto(sockfd, (char*)&ack_pkt, sizeof(RDTHeader), 0, (struct sockaddr*)&server_addr, server_addr_len);
        }
    }

    outfile.close();
    std::cout << "[Client RDT] Đã nhận file thành công và lưu tại: " << output_filepath << std::endl;
    return true;
}
