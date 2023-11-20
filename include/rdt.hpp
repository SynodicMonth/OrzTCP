#ifndef RDT_HPP
#define RDT_HPP
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "protocol.hpp"
#include "utils.hpp"
#include "udp.hpp"


class RDTSender {
public:
    enum ARQType {
        STOP_AND_WAIT,
        GO_BACK_N,
        SELECTIVE_REPEAT
    };
    RDTSender(const char *ip, int port, ARQType type);
    ~RDTSender();

    int connect(const char *ip, int port);
    int terminate();

    int send_data(const char *data, int len);
    int recv_ack();

private:
    UDP udp;
    ARQType type;

};

#endif