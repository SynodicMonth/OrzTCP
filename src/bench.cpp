#include "utils.hpp"
#include "protocol.hpp"
#include "udp.hpp"
#include "rdt.hpp"
#include "ftp.hpp"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cmath>

std::string filenames[] = {
    "testcases/helloworld.txt",
    "testcases/1.jpg",
    "testcases/2.jpg",
    "testcases/3.jpg"
};
std::string recv_filenames[] = {
    "testcases/helloworld_recv.txt",
    "testcases/1_recv.jpg",
    "testcases/2_recv.jpg",
    "testcases/3_recv.jpg"
};
int Ns[] = { 1, 2, 3, 4, 5, 6, 7, 8, 16, 32 };
std::vector<float> results[4];

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

int main() {
    for (int nfile = 0; nfile < 4; nfile++) {
        for (int j = 0; j < 10; j++) {
            int i = Ns[j];
            // receiver thread
            std::thread receiver_thread([i, nfile]() {
                // ftp receiver
                FTPReceiver receiver("127.0.0.1", 8098, RDTReciever::ARQType::SELECTIVE_REPEAT, i);
                // receive file
                receiver.receive_file(recv_filenames[nfile].c_str());
            });

            float throughput = 0;
            // sender thread
            std::thread sender_thread([&throughput, i, nfile]() {
                // ftp sender
                FTPSender sender("127.0.0.1", 8100, "127.0.0.1", 8099, RDTSender::ARQType::SELECTIVE_REPEAT, i, 200);
                // send file
                throughput = sender.benchmark(filenames[nfile].c_str());
            });

            // wait for threads to finish
            sender_thread.join();
            receiver_thread.join();

            // compare files
            if (compare_files(filenames[nfile].c_str(), recv_filenames[nfile].c_str())) {
                log(("File: " + filenames[nfile] + " N = " + std::to_string(i) + ": Test passed! Throughput: " + std::to_string(throughput) + " B/s").c_str());
                results[nfile].push_back(throughput);
            } else {
                log("Test failed!");
            }
        }
    }

    // print results
    for (int nfile = 0; nfile < 4; nfile++) {
        log(("File: " + filenames[nfile]).c_str());
        for (int i = 0; i < 10; i++) {
            log(("  N = " + std::to_string(Ns[i]) + ": " + std::to_string(results[nfile][i]) + " B/s").c_str());
        }
    }
}