#include "utils.hpp"
#include "protocol.hpp"
#include "udp.hpp"
#include "rdt.hpp"
#include "ftp.hpp"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cmath>
#include <getopt.h>
#include <cstdlib>
#include <iostream>

bool compare_files(const char *file1, const char *file2) {
    std::ifstream f1(file1, std::ios::binary | std::ios::ate);
    std::ifstream f2(file2, std::ios::binary | std::ios::ate);

    if (f1.fail() || f2.fail()) {
        return false;
    }

    if (f1.tellg() != f2.tellg()) {
        return false;
    }

    f1.seekg(0, std::ios::beg);
    f2.seekg(0, std::ios::beg);

    return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
        std::istreambuf_iterator<char>(),
        std::istreambuf_iterator<char>(f2.rdbuf()));
}

int main(int argc, char *argv[]) {
    int sender_port = 8100;
    int router_port = 8099;
    int receiver_port = 8089;
    std::string arq;
    int sender_n = 8;
    int receiver_n = 8;
    int timeout = 200;
    std::string file;

    struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"sender_port", optional_argument, 0, 's'},
        {"router_port", optional_argument, 0, 'r'},
        {"receiver_port", optional_argument, 0, 'c'},
        {"arq", required_argument, 0, 'a'},
        {"sender_n", optional_argument, 0, 'n'},
        {"receiver_n", optional_argument, 0, 'v'},
        {"timeout", optional_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "f:s::r::c::a:n::v::t::", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'f':
                file = optarg;
                break;
            case 's':
                sender_port = optarg ? std::atoi(optarg) : 8100;
                break;
            case 'r':
                router_port = optarg ? std::atoi(optarg) : 8099;
                break;
            case 'c':
                receiver_port = optarg ? std::atoi(optarg) : 8089;
                break;
            case 'a':
                arq = optarg;
                break;
            case 'n':
                sender_n = optarg ? std::atoi(optarg) : 8;
                break;
            case 'v':
                receiver_n = optarg ? std::atoi(optarg) : 8;
                break;
            case 't':
                timeout = optarg ? std::atoi(optarg) : 200;
                break;
            case '?':
                // getopt_long already printed an error message.
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " --file <file> [--sender_port <port>] [--router_port <port>] [--receiver_port <port>] --arq <arq> [--sender_n <n>] [--receiver_n <n>] [--timeout <ms>]" << std::endl;
                return 1;
        }
    }

    if (file.empty() || arq.empty()) {
        std::cerr << "Error: Missing required arguments --file or --arq." << std::endl;
        return 1;
    }
    
    RDTReciever::ARQType receiver_arq_type;
    RDTSender::ARQType sender_arq_type;

    if (arq == "sw" || arq == "stop_and_wait") {
        receiver_arq_type = RDTReciever::ARQType::STOP_AND_WAIT;
        sender_arq_type = RDTSender::ARQType::STOP_AND_WAIT;
    } else if (arq == "gbn" || arq == "go_back_n") {
        receiver_arq_type = RDTReciever::ARQType::GO_BACK_N;
        sender_arq_type = RDTSender::ARQType::GO_BACK_N;
    } else if (arq == "sr" || arq == "selective_repeat") {
        receiver_arq_type = RDTReciever::ARQType::SELECTIVE_REPEAT;
        sender_arq_type = RDTSender::ARQType::SELECTIVE_REPEAT;
    } else {
        std::cerr << "Error: Invalid argument --arq." << std::endl;
        return 1;
    }

    std::thread receiver_thread([receiver_n, file, receiver_arq_type]() {
        // ftp receiver
        FTPReceiver receiver("127.0.0.1", 8098, receiver_arq_type, receiver_n);
        // receive file
        receiver.receive_file((file + ".recv").c_str());
    });

    float throughput = 0;
    // sender thread
    std::thread sender_thread([&throughput, sender_n, file, timeout, sender_arq_type]() {
        // ftp sender
        FTPSender sender("127.0.0.1", 8100, "127.0.0.1", 8099, sender_arq_type, sender_n, timeout);
        // send file
        throughput = sender.benchmark(file.c_str());
    });

    // wait for threads to finish
    sender_thread.join();
    receiver_thread.join();

    // compare files
    if (compare_files(file.c_str(), file.c_str())) {
        if (sender_arq_type == RDTSender::ARQType::STOP_AND_WAIT) {
            log(("File: " + file + 
             " ARQ: " + "Stop-and-Wait" +
             " Timeout = " + std::to_string(timeout) +
             " Throughput: " + std::to_string(throughput) + " B/s").c_str());
        } else if (sender_arq_type == RDTSender::ARQType::GO_BACK_N) {
            log(("File: " + file + 
             " ARQ: " + "Go-Back-N" +
             " N = " + std::to_string(sender_n) +
             " Timeout = " + std::to_string(timeout) +
             " Throughput: " + std::to_string(throughput) + " B/s").c_str());
        } else if (sender_arq_type == RDTSender::ARQType::SELECTIVE_REPEAT) {
            log(("File: " + file + 
             " ARQ: " + "Selective Repeat" +
             " Sender N = " + std::to_string(sender_n) +
             " Receiver N = " + std::to_string(receiver_n) +
             " Timeout = " + std::to_string(timeout) +
             " Throughput: " + std::to_string(throughput) + " B/s").c_str());
        }
        return 0;
    } else {
        return 1;
    }
}
