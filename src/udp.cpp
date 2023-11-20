#include "udp.hpp"

UDP::UDP(const char *ip, int port) {
    // create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        err("UDP::UDP(): Error creating socket");
    }
    // set addr
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        err("Error converting ip address");
    }
    // set non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        err("Error getting socket flags");
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        err("Error setting socket flags");
    }
}

UDP::~UDP() {
    close(sock);
}

int UDP::send_packet(OrzTCPPacket *packet, const struct sockaddr_in *addr) {
    int packet_size = sizeof(OrzTCPHeader) + packet->header.len;
    int ret = sendto(sock, packet, packet_size, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        err("Error sending packet");
    }
    return ret;
}

int UDP::recv_packet(OrzTCPPacket *packet, struct sockaddr_in *addr) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int ret = recvfrom(sock, packet, sizeof(OrzTCPPacket), 0, (struct sockaddr *)addr, &addr_len);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        err("Error receiving packet");
    }
    return ret;
}
