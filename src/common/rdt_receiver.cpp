#include "rdt.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

// Báo cho Visual Studio biết cần nhúng thư viện mạng của Windows
#pragma comment(lib, "ws2_32.lib")

// Khai báo kích thước tối đa của một gói tin (VD: 1024 bytes)
#define MAX_PACKET_SIZE 1024

bool rdt_receive_file(SOCKET sockfd, const char* save_filename) {
    // Mở file để ghi dữ liệu nhị phân (binary)
    std::ofstream file(save_filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[RDT Receiver] Lỗi: Không thể tạo file " << save_filename << std::endl;
        return false;
    }

    uint16_t expected_seq = 0;
    char buffer[MAX_PACKET_SIZE];

    sockaddr_in src_addr;
    int addr_len = sizeof(src_addr); // Trên Windows bắt buộc dùng int thay vì socklen_t

    std::cout << "[RDT Receiver] Đang chờ nhận file..." << std::endl;

    while (true) {
        memset(buffer, 0, MAX_PACKET_SIZE);

        // Nhận dữ liệu qua UDP
        int bytes_received = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0, (sockaddr*)&src_addr, &addr_len);

        if (bytes_received <= 0) {
            std::cerr << "[RDT Receiver] Lỗi kết nối hoặc mất tín hiệu." << std::endl;
            break;
        }

        // Tách Header và Payload (dữ liệu thực) ra khỏi buffer
        PacketHeader* header = (PacketHeader*)buffer;
        char* payload = buffer + sizeof(PacketHeader);
        int payload_length = header->length;

        // Tính lại Checksum để đối chiếu (giả sử bạn đã có hàm calculate_checksum ở utils.cpp)
        uint16_t computed_checksum = calculate_checksum(payload, payload_length);

        // Kiểm tra xem gói tin có bị hỏng (sai checksum) hoặc sai thứ tự không
        if (computed_checksum == header->checksum && header->seq_num == expected_seq) {

            // Ghi dữ liệu vào file nếu có payload
            if (payload_length > 0) {
                file.write(payload, payload_length);
            }

            // Gửi gói tin ACK phản hồi cho Sender biết đã nhận đúng
            PacketHeader ack_packet;
            ack_packet.seq_num = expected_seq;
            ack_packet.length = 0;
            ack_packet.checksum = 0;

            sendto(sockfd, (const char*)&ack_packet, sizeof(PacketHeader), 0, (sockaddr*)&src_addr, addr_len);

            // Gói tin có length = 0 được dùng làm cờ báo hiệu đã truyền xong file
            if (payload_length == 0) {
                std::cout << "[RDT Receiver] Đã nhận file hoàn tất!" << std::endl;
                break;
            }

            // Tăng số thứ tự để đợi gói tiếp theo
            expected_seq++;
        }
        else {
            // Nếu hỏng hoặc sai thứ tự, gửi lại ACK của gói cũ để Sender gửi lại
            PacketHeader nak_packet;
            nak_packet.seq_num = expected_seq - 1;
            nak_packet.length = 0;
            nak_packet.checksum = 0;

            sendto(sockfd, (const char*)&nak_packet, sizeof(PacketHeader), 0, (sockaddr*)&src_addr, addr_len);
        }
    }

    file.close();
    return true;
}