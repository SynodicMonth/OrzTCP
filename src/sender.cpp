#include "utils.hpp"
#include "protocol.hpp"
#include "udp.hpp"
#include "rdt.hpp"
#include "ftp.hpp"

int main() {
    log("Welcome to OrzTCP!");

    // ftp sender
    FTPSender sender("127.0.0.1", 8100, "127.0.0.1", 8099, RDTSender::ARQType::SELECTIVE_REPEAT, 16, 200);

    // choose file to send
    std::string filename;
    std::cout << "Enter file name: ";
    std::cin >> filename;

    // send file
    sender.send_file(filename.c_str());
}