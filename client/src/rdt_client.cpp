#include "../include/rdt_client.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

uint16_t calculate_checksum(const char* data, int length) {
    uint32_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += (uint8_t)data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

bool rdt_receive_file(SOCKET sockfd, sockaddr_in& server_addr, const std::string& output_filepath) {
    std::ofstream outfile(output_filepath, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "[Client RDT] Không thể tạo file để lưu: " << output_filepath << std::endl;
        return false;
    }

    std::map<uint32_t, std::vector<char>> buffer_packets; // Lưu tạm các gói tin đến
    uint32_t expected_seq_num = 0;                        // Số thứ tự gói mong đợi tiếp theo
    int server_addr_len = sizeof(server_addr);

    std::cout << "[Client RDT] Đang chờ nhận file từ Server..." << std::endl;

    while (true) {
        RDTPacket pkt;
        int bytes_in = recvfrom(sockfd, (char*)&pkt, sizeof(RDTPacket), 0, (struct sockaddr*)&server_addr, &server_addr_len);

        if (bytes_in > 0) {
            // Kiểm tra xem có phải gói tin kết thúc (FIN) không
            if (pkt.header.flags == 2) {
                std::cout << "[Client RDT] Nhận tín hiệu kết thúc (FIN) từ Server." << std::endl;
                break;
            }

            // Kiểm tra tính toàn vẹn dữ liệu bằng Checksum
            uint16_t computed_checksum = calculate_checksum(pkt.data, pkt.header.length);
            if (computed_checksum != pkt.header.checksum) {
                std::cout << "[Client RDT] Cảnh báo: Lỗi Checksum ở gói số " << pkt.header.seq_num << "! Bỏ qua gói." << std::endl;
                continue; // Sai checksum thì bỏ qua, không gửi ACK để Server timeout gửi lại
            }

            // Nếu đúng gói mong đợi theo cơ chế Go-Back-N
            if (pkt.header.seq_num == expected_seq_num) {
                // Ghi dữ liệu vào file ngay lập tức
                outfile.write(pkt.data, pkt.header.length);
                std::cout << "[Client RDT] Đã nhận và ghi gói: " << expected_seq_num << std::endl;
                expected_seq_num++;
            }

            // Gửi lại gói ACK xác nhận cho Server
            RDTPacket ack_pkt;
            ack_pkt.header.ack_num = expected_seq_num - 1;
            ack_pkt.header.flags = 1; // Cờ ACK
            sendto(sockfd, (char*)&ack_pkt, sizeof(RDTHeader), 0, (struct sockaddr*)&server_addr, server_addr_len);
        }
    }

    outfile.close();
    std::cout << "[Client RDT] Đã nhận file thành công và lưu tại: " << output_filepath << std::endl;
    return true;
}