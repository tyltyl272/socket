#include "rdt.h"
#include <iostream>
#include <fstream>
#include <cstring>

bool rdt_receive_file(SOCKET sockfd, const char* save_filename) {
    std::ofstream file(save_filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[RDT Receiver] Lỗi: Không thể tạo file " << save_filename << std::endl;
        return false;
    }

    DWORD tv = TIMEOUT_SEC * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    uint32_t expected_seq = 0;
    uint32_t last_success_seq = 1;
    bool has_received_any = false;

    RDTPacket packet;
    sockaddr_in src_addr;
    int addr_len = sizeof(src_addr);

    std::cout << "[RDT Receiver] Đang chờ nhận file..." << std::endl;

    while (true) {
        memset(&packet, 0, sizeof(packet));
        int bytes_received = recvfrom(sockfd, (char*)&packet, sizeof(packet), 0, (sockaddr*)&src_addr, &addr_len);
        
        if (bytes_received <= 0) {
            continue;
        }

        uint16_t recv_checksum = packet.header.checksum;
        packet.header.checksum = 0;
        size_t total_pkt_size = sizeof(RDTHeader) + packet.header.payload_len;
        uint16_t computed_checksum = calculate_checksum(&packet, total_pkt_size);

        if (computed_checksum == recv_checksum && packet.header.seq_num == expected_seq) {
            if (packet.header.payload_len > 0) {
                file.write(packet.data, packet.header.payload_len);
            }

            RDTPacket ack_packet{};
            ack_packet.header.seq_num = expected_seq;
            ack_packet.header.is_ack = 1;
            ack_packet.header.payload_len = 0;
            ack_packet.header.checksum = 0;
            ack_packet.header.checksum = calculate_checksum(&ack_packet, sizeof(RDTHeader));

            sendto(sockfd, (const char*)&ack_packet, sizeof(RDTHeader), 0, (sockaddr*)&src_addr, addr_len);

            last_success_seq = expected_seq;
            has_received_any = true;
            expected_seq = 1 - expected_seq;

            if (packet.header.is_last == 1) {
                std::cout << "[RDT Receiver] Đã nhận file hoàn tất!" << std::endl;
                break;
            }
        } else {
            if (has_received_any) {
                RDTPacket ack_packet{};
                ack_packet.header.seq_num = last_success_seq;
                ack_packet.header.is_ack = 1;
                ack_packet.header.payload_len = 0;
                ack_packet.header.checksum = 0;
                ack_packet.header.checksum = calculate_checksum(&ack_packet, sizeof(RDTHeader));

                sendto(sockfd, (const char*)&ack_packet, sizeof(RDTHeader), 0, (sockaddr*)&src_addr, addr_len);
            }
        }
    }

    file.close();
    return true;
}