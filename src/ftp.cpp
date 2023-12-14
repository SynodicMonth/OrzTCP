#include "ftp.hpp"

FTPSender::FTPSender(const char *ip, int port, const char *target_ip, 
    int target_port, RDTSender::ARQType arq_type, int window_size, int timeout) 
    : sender(ip, port, arq_type) {
        log("FTPSender: Initialized");
        sender.connect(target_ip, target_port);
        log("FTPSender: Connected");
        if (arq_type != RDTSender::ARQType::STOP_AND_WAIT) {
            sender.set_window_size(window_size);
        }
        sender.set_timeout(timeout);
        log(("FTPSender: Window size " + std::to_string(window_size) + ", timeout " + std::to_string(timeout) + "ms").c_str());
}

FTPSender::~FTPSender() {
    if (sender.get_state() != RDTSender::State::FIN_ACKED) {
        sender.terminate();
    }
    log("FTPSender: Terminated");
}

void FTPSender::send_file(const char *filename) {
    // open the file
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        err("FTPSender: Error opening file");
    }

    // send file
    char data[MAX_MSG_SIZE];
    int len;
    int total_len = 0;
    while ((len = fread(data, sizeof(char), MAX_MSG_SIZE, fp)) > 0) {
        total_len += len;
        sender.send_data(data, len);
    }

    log("FTPSender: EOF reached");

    sender.terminate();

    // total length
    log(("FTPSender: File size: " + std::to_string(total_len) + " Bytes").c_str());

    // missed packets
    log(("FTPSender: Missed packets: " + std::to_string(sender.misses)).c_str());

    // missrate
    log(("FTPSender: Missrate: " + std::to_string((float)sender.misses / (float)sender.get_packets_sent() * 100.0) + "%").c_str());

    // close the file
    fclose(fp);
}

float FTPSender::benchmark(const char *filename) {
    // open the file
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        err("FTPSender: Error opening file");
    }

    // send file
    char data[MAX_MSG_SIZE];
    int len;
    int total_len = 0;
    auto start = std::chrono::steady_clock::now();
    while ((len = fread(data, sizeof(char), MAX_MSG_SIZE, fp)) > 0) {
        total_len += len;
        sender.send_data(data, len);
    }
    auto end = std::chrono::steady_clock::now();

    // total length
    log(("FTPSender: File size: " + std::to_string(total_len) + " Bytes").c_str());

    // time elapsed
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    log(("FTPSender: Time elapsed: " + std::to_string((float)elapsed.count() / 1000.0) + " s").c_str());

    // throughput
    log(("FTPSender: Throughput: " + std::to_string((float)total_len * 1000.0 / elapsed.count()) + " B/s").c_str());

    // missed packets
    log(("FTPSender: Missed packets: " + std::to_string(sender.misses)).c_str());

    // missrate
    log(("FTPSender: Missrate: " + std::to_string((float)sender.misses / (float)sender.get_packets_sent() * 100.0) + "%").c_str());

    // close the file
    fclose(fp);

    return (float)total_len * 1000.0 / elapsed.count();
}

FTPReceiver::FTPReceiver(const char *ip, int port, RDTReciever::ARQType arq_type, int window_size)
    : receiver(ip, port, arq_type) {
        log("FTPReceiver: Initialized");
        if (arq_type == RDTReciever::ARQType::SELECTIVE_REPEAT) {
            receiver.set_window_size(window_size);
        }
        receiver.startup();
        log("FTPReceiver: Connected");
}

FTPReceiver::~FTPReceiver() {
    log("FTPReceiver: Terminated");
}

void FTPReceiver::receive_file(const char *filename) {
    // loop to recv file
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        err("FTPReceiver: Error opening file");
    }
    char data[MAX_MSG_SIZE];
    int len;
    int total_len = 0;
    auto start = std::chrono::steady_clock::now();
    while ((len = receiver.recv_data(data, MAX_MSG_SIZE)) > 0) {
        total_len += len;
        fwrite(data, sizeof(char), len, fp);
    }
    if (len == E_PASSIVE_CLOSE) {
        log("FTPReceiver: Connection closed");
    }
    auto end = std::chrono::steady_clock::now();

    // total length
    log(("FTPReceiver: File size: " + std::to_string(total_len) + " Bytes").c_str());

    // time elapsed
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    log(("FTPReceiver: Time elapsed: " + std::to_string((float)elapsed.count() / 1000.0) + " s").c_str());

    // throughput
    log(("FTPReceiver: Throughput: " + std::to_string((float)total_len * 1000.0 / elapsed.count()) + " B/s").c_str());

    // close the file
    fclose(fp);
}