#include "rdt.h"
#include "ui_manager.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <chrono>

bool rdt_receive_file(SOCKET sockfd, const char* save_filename, long long total_file_size) {
    char file_buffer[65536];
    std::ofstream file;
    file.rdbuf()->pubsetbuf(file_buffer, sizeof(file_buffer));
    file.open(save_filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[RDT Receiver] Lỗi: Không tạo được file " << save_filename << std::endl;
        return false;
    }

    int rcvbuf_size = 8 * 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf_size, sizeof(rcvbuf_size));

    DWORD tv = 3000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    uint32_t expected_seq = 0;
    long long total_received_bytes = 0;

    RDTPacket packet;
    sockaddr_in src_addr;
    int addr_len = sizeof(src_addr);

    auto last_ui_update = std::chrono::steady_clock::now();

    while (true) {
        memset(&packet, 0, sizeof(packet));
        int bytes_received = recvfrom(sockfd, (char*)&packet, sizeof(packet), 0, (sockaddr*)&src_addr, &addr_len);
        
        if (bytes_received <= 0) continue;

        uint16_t recv_checksum = packet.header.checksum;
        packet.header.checksum = 0;
        size_t total_pkt_size = sizeof(RDTHeader) + packet.header.payload_len;
        uint16_t computed_checksum = calculate_checksum(&packet, total_pkt_size);

        if (computed_checksum == recv_checksum) {
            if (packet.header.seq_num == expected_seq) {
                if (packet.header.payload_len > 0) {
                    file.write(packet.data, packet.header.payload_len);
                    total_received_bytes += packet.header.payload_len;
                }

                RDTPacket ack_packet{};
                ack_packet.header.seq_num = expected_seq;
                ack_packet.header.is_ack = 1;
                ack_packet.header.checksum = calculate_checksum(&ack_packet, sizeof(RDTHeader));
                sendto(sockfd, (const char*)&ack_packet, sizeof(RDTHeader), 0, (sockaddr*)&src_addr, addr_len);

                expected_seq++;

                auto now = std::chrono::steady_clock::now();
                bool is_last = (packet.header.is_last == 1);
                
                // Cập nhật Progress Bar mỗi 100ms hoặc khi nhận gói tin cuối cùng
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ui_update).count() >= 100 || is_last) {
                    // Nếu là gói cuối, đảm bảo dung lượng truyền vào bằng đúng dung lượng đã nhận để Bar nhảy trọn 100%
                    long long target_size = (is_last || total_file_size <= 0) ? total_received_bytes : total_file_size;
                    UIManager::printProgressBar(total_received_bytes, target_size);
                    last_ui_update = now;
                }

                if (is_last) {
                    // Bỏ \n ở đầu vì printProgressBar đã tự kết thúc dòng khi hoàn tất 100%
                    std::cout << "[RDT Receiver] Tải file hoàn tất qua GBN!" << std::endl;
                    break;
                }
            } else if (expected_seq > 0) {
                RDTPacket ack_packet{};
                ack_packet.header.seq_num = expected_seq - 1;
                ack_packet.header.is_ack = 1;
                ack_packet.header.checksum = calculate_checksum(&ack_packet, sizeof(RDTHeader));
                sendto(sockfd, (const char*)&ack_packet, sizeof(RDTHeader), 0, (sockaddr*)&src_addr, addr_len);
            }
        }
    }

    file.close();
    return true;
}