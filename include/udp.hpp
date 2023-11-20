#ifndef UDP_HPP
#define UDP_HPP
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <fcntl.h>
#include "protocol.hpp"
#include "utils.hpp"

class UDP {
public:
    UDP(const char *ip, int port);
    ~UDP();
    // send packet to addr
    int send_packet(OrzTCPPacket *packet, const struct sockaddr_in *addr);
    // recv packet from addr and store it in packet
    int recv_packet(OrzTCPPacket *packet, struct sockaddr_in *addr);
private:
    int sock;
    struct sockaddr_in addr;
};

#endif