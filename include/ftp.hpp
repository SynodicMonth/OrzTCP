#ifndef FTP_HPP
#define FTP_HPP
#include "rdt.hpp"

class FTPSender {
public:
    FTPSender(const char *ip, int port, const char *target_ip, 
        int target_port, RDTSender::ARQType arq_type = RDTSender::ARQType::GO_BACK_N,
        int window_size = 8, int timeout = 200);
    
    ~FTPSender();

    void send_file(const char *filename);
    float benchmark(const char *filename);

private:
    RDTSender sender;
};

class FTPReceiver {
public:
    FTPReceiver(const char *ip, int port, RDTReciever::ARQType arq_type = RDTReciever::ARQType::GO_BACK_N, int window_size = 8);

    ~FTPReceiver();

    void receive_file(const char *filename);

private:
    RDTReciever receiver;
};

#endif