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
        err("UDP::UDP(): Error converting ip address");
    }
    // bind addr
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        err("UDP::UDP(): Error binding socket");
    }
    // // set non-blocking mode
    // int flags = fcntl(sock, F_GETFL, 0);
    // if (flags < 0) {
    //     err("UDP::UDP(): Error getting socket flags");
    // }
    // if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
    //     err("UDP::UDP(): Error setting socket flags");
    // }
}

UDP::~UDP() {
    close(sock);
}

// send packet to addr and return the number of bytes sent
int UDP::send_packet(OrzTCPPacket *packet, const struct sockaddr_in *addr) {
    packets_sent++;
    int packet_size = sizeof(OrzTCPHeader) + packet->header.len;
    int ret = sendto(sock, packet, packet_size, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        err("UDP::send_packet(): Error sending packet");
    }

    debug(get_debug_str("UDP::send_packet(): Sent packet", packet).c_str());
    return ret;
}


// recv packet from addr with timout in milliseconds (0 for no timeout) and store it in packet
int UDP::recv_packet(OrzTCPPacket *packet, struct sockaddr_in *addr, unsigned int timeout) {
    packets_recv++;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    char* buffer = new char[BUF_SIZE];
    if (timeout > 0) {
        // set timeout
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            delete[] buffer;
            err("UDP::recv_packet(): Error setting timeout");
        }
    } 
    int ret = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr *)addr, &addr_len);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            delete[] buffer;
            return 0;
        }
        delete[] buffer;
        err("UDP::recv_packet(): Error receiving packet");
    }
    // reset timeout
    if (timeout > 0) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            delete[] buffer;
            err("UDP::recv_packet(): Error resetting timeout");
        }
    }
    if (ret < sizeof(OrzTCPHeader)) {
        delete[] buffer;
        err("UDP::recv_packet(): Packet too small");
    }
    if (ret > BUF_SIZE) {
        delete[] buffer;
        err("UDP::recv_packet(): Packet too large");
    }
    if (ret != sizeof(OrzTCPHeader) + ((OrzTCPHeader *)buffer)->len) {
        delete[] buffer;
        err("UDP::recv_packet(): Packet size mismatch");
    }
    // print log
    debug(get_debug_str("UDP::recv_packet(): Recv packet", (OrzTCPPacket *)buffer).c_str());
    // copy buffer to packet
    memcpy(packet, buffer, ret);
    delete[] buffer;
    return ret;
}
