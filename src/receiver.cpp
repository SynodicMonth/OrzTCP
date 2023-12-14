#include "utils.hpp"
#include "protocol.hpp"
#include "udp.hpp"
#include "rdt.hpp"
#include "ftp.hpp"

int main() {
    log("Welcome to OrzTCP!");

    // choose file name
    std::string filename;
    std::cout << "Enter file name: ";
    std::cin >> filename;

    // ftp receiver
    FTPReceiver receiver("127.0.0.1", 8098, RDTReciever::ARQType::SELECTIVE_REPEAT, 8);
    
    // receive file
    receiver.receive_file(filename.c_str());
}