#include "rdt.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

bool rdt_send_file(SOCKET sockfd, const char* filename, const char* dest_ip, int dest_port) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[RDT Sender] Lỗi: Không mở được file " << filename << std::endl;
        return false;
    }

    int sndbuf_size = 8 * 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbuf_size, sizeof(sndbuf_size));

    u_long mode = 1;
    ioctlsocket(sockfd, FIONBIO, &mode);

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);

    std::vector<RDTPacket> packets;
    uint32_t seq = 0;

    while (!file.eof()) {
        RDTPacket pkt{};
        file.read(pkt.data, PAYLOAD_SIZE);
        pkt.header.payload_len = static_cast<uint16_t>(file.gcount());
        pkt.header.seq_num = seq++;
        pkt.header.is_ack = 0;
        pkt.header.is_last = file.eof() ? 1 : 0;
        
        pkt.header.checksum = 0;
        size_t total_len = sizeof(RDTHeader) + pkt.header.payload_len;
        pkt.header.checksum = calculate_checksum(&pkt, total_len);

        packets.push_back(pkt);
    }
    file.close();

    uint32_t base = 0;
    uint32_t next_seq = 0;
    uint32_t total_packets = static_cast<uint32_t>(packets.size());

    DWORD timer_start = 0;
    bool timer_running = false;

    std::cout << "[RDT Sender] Bắt đầu truyền GBN (" << total_packets << " gói tin)..." << std::endl;

    while (base < total_packets) {
        while (next_seq < base + WINDOW_SIZE && next_seq < total_packets) {
            size_t pkt_size = sizeof(RDTHeader) + packets[next_seq].header.payload_len;
            sendto(sockfd, (const char*)&packets[next_seq], pkt_size, 0, (sockaddr*)&dest_addr, sizeof(dest_addr));
            
            if (base == next_seq) {
                timer_start = GetTickCount();
                timer_running = true;
            }
            next_seq++;
        }

        RDTPacket ack_pkt;
        sockaddr_in from_addr;
        int from_len = sizeof(from_addr);
        int bytes = recvfrom(sockfd, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&from_addr, &from_len);

        if (bytes > 0) {
            uint16_t recv_cs = ack_pkt.header.checksum;
            ack_pkt.header.checksum = 0;
            if (calculate_checksum(&ack_pkt, sizeof(RDTHeader)) == recv_cs && ack_pkt.header.is_ack == 1) {
                if (ack_pkt.header.seq_num >= base) {
                    base = ack_pkt.header.seq_num + 1;
                    if (base == next_seq) {
                        timer_running = false;
                    } else {
                        timer_start = GetTickCount();
                        timer_running = true;
                    }
                }
            }
        }

        if (timer_running && (GetTickCount() - timer_start >= TIMEOUT_MS)) {
            timer_start = GetTickCount();
            for (uint32_t i = base; i < next_seq; ++i) {
                size_t pkt_size = sizeof(RDTHeader) + packets[i].header.payload_len;
                sendto(sockfd, (const char*)&packets[i], pkt_size, 0, (sockaddr*)&dest_addr, sizeof(dest_addr));
            }
        }
    }

    mode = 0;
    ioctlsocket(sockfd, FIONBIO, &mode);
    return true;
}