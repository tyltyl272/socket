#include "rdt.h"
#include <iostream>
#include <fstream>
#include <cstring>

bool rdt_send_file(SOCKET sockfd, const char* filename, const char* dest_ip, int dest_port) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[RDT Sender] Lỗi: Không thể mở file " << filename << std::endl;
        return false;
    }

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);

    DWORD tv = TIMEOUT_SEC * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    uint32_t seq_num = 0;
    RDTPacket packet;
    RDTPacket ack_packet;
    sockaddr_in ack_addr;
    int addr_len = sizeof(ack_addr);

    while (true) {
        memset(&packet, 0, sizeof(packet));
        file.read(packet.data, MAX_PAYLOAD_SIZE);
        std::streamsize bytes_read = file.gcount();

        packet.header.seq_num = seq_num;
        packet.header.payload_len = static_cast<uint16_t>(bytes_read);
        packet.header.is_ack = 0;
        packet.header.is_last = (file.eof() || bytes_read < MAX_PAYLOAD_SIZE) ? 1 : 0;
        packet.header.checksum = 0;

        size_t total_pkt_size = sizeof(RDTHeader) + bytes_read;
        packet.header.checksum = calculate_checksum(&packet, total_pkt_size);

        bool ack_received = false;
        int retries = 0;

        while (!ack_received && retries < MAX_RETRIES) {
            sendto(sockfd, (const char*)&packet, (int)total_pkt_size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            std::cout << "[RDT Sender] Đã gửi Gói #" << seq_num << " (" << bytes_read << " bytes). Đang chờ ACK...\n";

            int recv_bytes = recvfrom(sockfd, (char*)&ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&ack_addr, &addr_len);
            if (recv_bytes >= (int)sizeof(RDTHeader)) {
                uint16_t recv_chk = ack_packet.header.checksum;
                ack_packet.header.checksum = 0;
                
                uint16_t calc_chk = calculate_checksum(&ack_packet, sizeof(RDTHeader) + ack_packet.header.payload_len);

                if (recv_chk == calc_chk && ack_packet.header.is_ack == 1 && ack_packet.header.seq_num == seq_num) {
                    std::cout << "[RDT Sender] Nhận ACK hợp lệ cho Gói #" << seq_num << std::endl;
                    ack_received = true;
                } else {
                    std::cout << "[RDT Sender] ACK bị lỗi Checksum hoặc sai SeqNum! Gửi lại...\n";
                }
            } else {
                std::cout << "[RDT Sender] Timeout! Khởi tạo gửi lại lần " << (retries + 1) << "...\n";
                retries++;
            }
        }

        if (!ack_received) {
            std::cerr << "[RDT Sender] Lỗi: Vượt quá số lần gửi lại cho phép!\n";
            file.close();
            return false;
        }

        seq_num = 1 - seq_num;

        if (packet.header.is_last == 1) break;
    }

    std::cout << "[RDT Sender] Truyền file thành công!\n";
    file.close();
    return true;
}