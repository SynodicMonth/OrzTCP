#include "utils.hpp"
#include "protocol.hpp"
#include "udp.hpp"

int main() {
    // create udp socket
    UDP udp("127.0.0.1", 8887);
    // test log
    log("reciever start");
    // test err
    err("reciever start");
    // test debug
    debug("reciever start");
}