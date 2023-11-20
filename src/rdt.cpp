#include "rdt.hpp"

RDTSender::RDTSender(const char *ip, int port, ARQType type) {
    // create udp socket
    udp = UDP(ip, port);
    // set ARQ type
    this->type = type;
}

RDTSender::~RDTSender() {
    
}

int RDTSender::connect(const char *ip, int port) {
    // symmetrical handshake
    // send SYN
    
}