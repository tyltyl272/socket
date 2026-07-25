#include "rdt.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

bool rdt_send_file(int sockfd, const char* filename, const char* dest_ip, int dest_port) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[RDT Sender] Lỗi: Không thể mở file " << filename << std::endl;
        return false;
    }

    // Thiết lập địa chỉ đích
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);

    // Cấu hình Timeout nhận ACK cho Socket
    timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint32_t seq_num = 0;
    RDTPacket packet;
    RDTPacket ack_packet;
    sockaddr_in ack_addr;
    socklen_t addr_len = sizeof(ack_addr);

    while (true) {
        file.read(packet.payload, MAX_PAYLOAD_SIZE);
        std::streamsize bytes_read = file.gcount();

        packet.header.seq_num = seq_num;
        packet.header.payload_len = static_cast<uint32_t>(bytes_read);
        packet.header.is_ack = 0;
        packet.header.is_last = file.eof() ? 1 : 0;
        packet.header.checksum = 0; // Đặt bằng 0 trước khi tính checksum

        // Tính toán Checksum cho toàn bộ Packet (Header + Payload)
        size_t total_pkt_size = sizeof(RDTHeader) + bytes_read;
        packet.header.checksum = calculate_checksum(&packet, total_pkt_size);

        bool ack_received = false;
        int retries = 0;

        // Vòng lặp Stop-and-Wait với Retransmission
        while (!ack_received && retries < MAX_RETRIES) {
            // Send Data
            sendto(sockfd, &packet, total_pkt_size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            std::cout << "[RDT Sender] Da gui Goi #" << seq_num << " (" << bytes_read << " bytes). Dang cho ACK..." << std::endl;

            // Chờ nhận ACK
            ssize_t recv_bytes = recvfrom(sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&ack_addr, &addr_len);

            if (recv_bytes >= (ssize_t)sizeof(RDTHeader)) {
                // Kiểm tra checksum của gói ACK
                uint16_t recv_chk = ack_packet.header.checksum;
                ack_packet.header.checksum = 0;
                uint16_t calc_chk = calculate_checksum(&ack_packet, sizeof(RDTHeader));

                if (recv_chk == calc_chk && ack_packet.header.is_ack == 1 && ack_packet.header.seq_num == seq_num) {
                    std::cout << "[RDT Sender] Nhan ACK hop le cho Goi #" << seq_num << std::endl;
                    ack_received = true;
                    seq_num++; // Tăng Sequence Number cho gói tiếp theo
                }
                else {
                    std::cout << "[RDT Sender] ACK bi loi Checksum hoac sai Sequence Number! Gui lai..." << std::endl;
                }
            }
            else {
                // Hết hạn chờ (Timeout)
                std::cout << "[RDT Sender] Timeout! Khong nhan duoc ACK gói #" << seq_num << ". Dang gui lai lần " << (retries + 1) << "..." << std::endl;
                retries++;
            }
        }

        if (!ack_received) {
            std::cerr << "[RDT Sender] Lỗi: Vượt quá số lần gửi lại cho phép!" << std::endl;
            file.close();
            return false;
        }

        if (packet.header.is_last == 1) {
            break; // Hoàn tất truyền file
        }
    }

    std::cout << "[RDT Sender] Truyen file thanh cong!" << std::endl;
    file.close();
    return true;
}