#include "rdt.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_PACKET_SIZE 1024

bool rdt_receive_file(SOCKET sockfd, const char* save_filename) {
    std::ofstream file(save_filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[RDT Receiver] Lỗi: Không thể tạo file " << save_filename << std::endl;
        return false;
    }

    uint16_t expected_seq = 0;
    char buffer[MAX_PACKET_SIZE];
    
    sockaddr_in src_addr;
    int addr_len = sizeof(src_addr);

    std::cout << "[RDT Receiver] Đang chờ nhận file..." << std::endl;

    while (true) {
        memset(buffer, 0, MAX_PACKET_SIZE);
        
        int bytes_received = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0, (sockaddr*)&src_addr, &addr_len);
        
        if (bytes_received <= 0) {
            std::cerr << "[RDT Receiver] Lỗi kết nối hoặc mất tín hiệu." << std::endl;
            break;
        }

        PacketHeader* header = (PacketHeader*)buffer;
        char* payload = buffer + sizeof(PacketHeader);
        int payload_length = header->length;

        // Kiểm tra tín hiệu kết thúc file (ví dụ: payload_length == 0 và seq_num tương ứng)
        if (payload_length == 0 && header->seq_num == expected_seq) {
            std::cout << "[RDT Receiver] Đã nhận tín hiệu kết thúc file từ Sender." << std::endl;
            
            // Gửi ACK xác nhận cuối cùng
            PacketHeader ack_packet;
            ack_packet.seq_num = expected_seq;
            ack_packet.length = 0;
            ack_packet.checksum = 0;
            sendto(sockfd, (const char*)&ack_packet, sizeof(PacketHeader), 0, (sockaddr*)&src_addr, addr_len);
            break;
        }

        // Tính lại Checksum để đối chiếu
        uint16_t computed_checksum = calculate_checksum(payload, payload_length);

        // Kiểm tra checksum và số thứ tự gói tin
        if (computed_checksum == header->checksum && header->seq_num == expected_seq) {
            
            // Ghi dữ liệu vào file
            if (payload_length > 0) {
                file.write(payload, payload_length);
            }

            // Gửi gói tin ACK phản hồi
            PacketHeader ack_packet;
            ack_packet.seq_num = expected_seq;
            ack_packet.length = 0; 
            ack_packet.checksum = 0; 
            sendto(sockfd, (const char*)&ack_packet, sizeof(PacketHeader), 0, (sockaddr*)&src_addr, addr_len);

            // Tăng số thứ tự gói mong đợi
            expected_seq++; 
        } 
        else {
            // Sửa lỗi tràn số: Nếu expected_seq == 0 thì NAK trả về 0, ngược lại mới trừ 1
            uint16_t nak_seq = (expected_seq > 0) ? (expected_seq - 1) : 0;

            PacketHeader nak_packet;
            nak_packet.seq_num = nak_seq;
            nak_packet.length = 0;
            nak_packet.checksum = 0;
            
            sendto(sockfd, (const char*)&nak_packet, sizeof(PacketHeader), 0, (sockaddr*)&src_addr, addr_len);
        }
    }

    file.close();
    std::cout << "[RDT Receiver] Đã lưu file thành công!" << std::endl;
    return true;
}
