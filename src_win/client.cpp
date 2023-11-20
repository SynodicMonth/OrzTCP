#include <iostream>
#include <string>
#include <winsock2.h>
#include <chrono>
#include "protocol.cpp"

#pragma comment(lib, "Ws2_32.lib")

const int BUFSIZE = 1400;
const char *IP = "127.0.0.1";
const int PORT = 8889;
const char *SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8887;
const int TIMEOUT_MSEC = 100;


// Reliable Data Transfer 3.0 (RDT3.0) Client for file transfer
class RDTClient {
public:
    RDTClient(const char* ip, int port);
    ~RDTClient();

    bool rdtSendFile(FILE* file, const char* targetIP, int targetPort);

private:
    SOCKET clientSocket;
    struct sockaddr_in clientAddr;
    WSADATA wsa;
    int rdtSeqAck = 0;
    int seq = 0;

    bool rdtSend(const char* buffer, int len, const sockaddr_in& to);

    bool udpSendPacket(const OrzTCPPacket* packet, const sockaddr_in& to);
    bool udpRecvPacket(OrzTCPPacket*& packet, const sockaddr_in& from);

    bool tcpConnect(const sockaddr_in& to);
    bool tcpTerminate(const sockaddr_in& to);

    void cleanUp();
    void updateSeq();
    void handleError(const char* errorMsg, int errorCode);
};

RDTClient::RDTClient(const char* ip, int port) {
    // Initialise Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        handleError("[ERROR] WSAStartup() failed", WSAGetLastError());
    }

    // Create a socket
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {
        handleError("[ERROR] socket() failed", WSAGetLastError());
    }

    // Prepare the sockaddr_in structure
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.S_un.S_addr = inet_addr(ip);
    clientAddr.sin_port = htons(port);

    // set timeout 
    int timeout = TIMEOUT_MSEC;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        handleError("[ERROR] setsockopt() failed", WSAGetLastError());
    }

    // Bind
    if (bind(clientSocket, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
        handleError("[ERROR] bind() failed", WSAGetLastError());
    }

    std::cout << "[INFO ] Client is up on port " << port << std::endl;
}

RDTClient::~RDTClient() {
    cleanUp();
}

void RDTClient::cleanUp() {
    closesocket(clientSocket);
    WSACleanup();
}

void RDTClient::handleError(const char* errorMsg, int errorCode) {
    std::cout << errorMsg << " with error code: " << errorCode << std::endl;
    cleanUp();
    exit(EXIT_FAILURE);
}

bool RDTClient::udpSendPacket(const OrzTCPPacket* packet, const sockaddr_in& to) {
    int sendLen = sendto(clientSocket, (const char *)packet, sizeof(OrzTCPHeader) + packet->header.len, 0, (struct sockaddr *)&to, sizeof(to));
    if (sendLen == SOCKET_ERROR) {
        handleError("[ERROR] sendto() failed", WSAGetLastError());
        return false;
    }
    std::cout << "[INFO ] Sent packet FLAG:";
    if (packet->header.type == TYPE_SYN) {
        std::cout << "SYN";
    } else if (packet->header.type == TYPE_ACK) {
        std::cout << "ACK";
    } else if (packet->header.type == TYPE_FIN) {
        std::cout << "FIN";
    } else if (packet->header.type == (TYPE_SYN | TYPE_ACK)) {
        std::cout << "SYNACK";
    } else if (packet->header.type == (TYPE_FIN | TYPE_ACK)) {
        std::cout << "FINACK";
    } else {
        std::cout << "DAT";
    }
    std::cout << " RDTSEQ/ACK:" << int(packet->header.rdtSeqAck);
    // std::cout << " SEQ:" << packet->header.seq << " ACK:" << packet->header.ack;
    std::cout << " CKSUM:" << int(packet->header.checksum) << std::endl;
    return true;
}

bool RDTClient::udpRecvPacket(OrzTCPPacket*& packet, const sockaddr_in& from) {
    int fromLen = sizeof(from);
    const int maxPacketSize = BUFSIZE;
    char* buffer = new char[maxPacketSize];
    
    int recvLen = recvfrom(clientSocket, buffer, maxPacketSize, 0, (struct sockaddr*)&from, &fromLen);
    if (recvLen == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            // std::cout << "[INFO ] Timeout" << std::endl;
            // timeout
            delete[] buffer;
            return false;
        } else {
            handleError("[ERROR] recvfrom() failed", WSAGetLastError());
            delete[] buffer;
            return false;
        }
    }

    // Process the received packet
    if (recvLen < sizeof(OrzTCPHeader)) {
        handleError("[ERROR] Recv packet is too small.", 0);
        delete[] buffer;
        return false;
    }

    // Now you have the full packet, allocate memory for OrzTCPPacket and copy the data
    packet = reinterpret_cast<OrzTCPPacket*>(new char[recvLen]);
    memcpy(packet, buffer, recvLen);

    delete[] buffer;

    std::cout << "[INFO ] Recv packet FLAG:";
    if (packet->header.type == TYPE_SYN) {
        std::cout << "SYN";
    } else if (packet->header.type == TYPE_ACK) {
        std::cout << "ACK";
    } else if (packet->header.type == TYPE_FIN) {
        std::cout << "FIN";
    } else if (packet->header.type == (TYPE_SYN | TYPE_ACK)) {
        std::cout << "SYNACK";
    } else if (packet->header.type == (TYPE_FIN | TYPE_ACK)) {
        std::cout << "FINACK";
    } else {
        std::cout << "DAT";
    }
    std::cout << " RDTSEQ/ACK:" << int(packet->header.rdtSeqAck);
    // std::cout << " SEQ:" << packet->header.seq << " ACK:" << packet->header.ack;
    std::cout << " CKSUM:" << int(packet->header.checksum) << std::endl;

    return true;
}

bool RDTClient::rdtSend(const char* buffer, int len, const sockaddr_in& to) {
    int retries = 0;
    const int maxRetries = 30;

    while (retries < maxRetries) {
        // send packet
        OrzTCPPacket* packet = reinterpret_cast<OrzTCPPacket*>(new char[sizeof(OrzTCPHeader) + len]);
        OrzTCPHeaderEncode(&packet->header, TYPE_DATA, 0, 0, len, rdtSeqAck);
        memcpy(packet->payload, buffer, len);
        OrzTCPSetHeaderChecksum(&packet->header);
        if (udpSendPacket(packet, to) == false) {
            handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
            delete[] reinterpret_cast<char*>(packet);
            return false;
        }
        delete[] reinterpret_cast<char*>(packet);

        // start timer
        auto startTime = std::chrono::high_resolution_clock::now();

        bool acked = false;
        // wait for ACK
        while (!acked) {
            // receive packet
            OrzTCPPacket *recvPacket = NULL;
            sockaddr_in from;
            if (udpRecvPacket(recvPacket, from) == false) {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    // timeout
                    delete[] reinterpret_cast<char*>(recvPacket);
                    std::cout << "[INFO ] Timeout, retrying " << retries + 1 << "/" << maxRetries << std::endl;
                    break;
                } else {
                    delete[] reinterpret_cast<char*>(recvPacket);
                    handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                    return false;
                }
            }

            // check if ACK and not corrupted
            if (recvPacket->header.type == TYPE_ACK && recvPacket->header.rdtSeqAck == rdtSeqAck && checkSum(&recvPacket->header)) {
                acked = true;
                delete[] reinterpret_cast<char*>(recvPacket);
                break;
            }

            // otherwise, ignore the packet
        }

        // check if ACK is received
        if (acked) {
            // stop timer
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            // std::cout << "[INFO ] ACK" << rdtSeqAck << " received" << std::endl;
            // toggle ACK
            rdtSeqAck = 1 - rdtSeqAck;
            return true;
        }

        // timeout reached, resend packet
        retries++;
    }

    // max retries reached
    std::cout << "[ERROR] Max retries reached" << std::endl;
    return false;
}

void RDTClient::updateSeq() {
    // generate random number for seq
    seq = rand() % 10000;
}

bool RDTClient::tcpConnect(const sockaddr_in& to) {
    // simplified TCP 3 way handshake
    updateSeq();
    int retries = 0;
    const int maxRetries = 30;

    // start timer
    auto startTime = std::chrono::high_resolution_clock::now();

    while (retries < maxRetries) {
        // send SYN
        OrzTCPPacket packet;
        OrzTCPHeaderEncode(&packet.header, TYPE_SYN, seq, 0, 0, 0);
        OrzTCPSetHeaderChecksum(&packet.header);
        if (udpSendPacket(&packet, to) == false) {
            handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
            return false;
        }

        bool acked = false;
        int ack = 0;
        // wait for SYNACK
        while (!acked) {
            // receive packet
            OrzTCPPacket *recvPacket = NULL;
            sockaddr_in from;
            if (udpRecvPacket(recvPacket, from) == false) {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    // timeout
                    delete[] reinterpret_cast<char*>(recvPacket);
                    std::cout << "[INFO ] Timeout, retrying" << retries + 1 << "/" << maxRetries << std::endl;
                    break;
                } else {
                    delete[] reinterpret_cast<char*>(recvPacket);
                    handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                    return false;
                }
            }

            // check if SYNACK and not corrupted
            if (recvPacket->header.type == (TYPE_SYN | TYPE_ACK) && checkSum(&recvPacket->header) && recvPacket->header.ack == seq + 1) {
                acked = true;
                ack = recvPacket->header.seq + 1;
                delete[] reinterpret_cast<char*>(recvPacket);
                break;
            }

            // otherwise, ignore the packet
        }

        // check if SYNACK is received
        if (acked) {
            // stop timer
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            std::cout << "[INFO ] SYNACK received in " << duration << "ms" << std::endl;
            // send ACK
            OrzTCPHeaderEncode(&packet.header, TYPE_ACK, seq + 1, ack, 0, 0);
            OrzTCPSetHeaderChecksum(&packet.header);
            if (udpSendPacket(&packet, to) == false) {
                handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
                return false;
            }
            return true;
        }

        // timeout reached, resend packet
        retries++;
    }

    // max retries reached
    std::cout << "[ERROR] Max retries reached" << std::endl;
    return false;
}

bool RDTClient::tcpTerminate(const sockaddr_in& to) {
    // simplified TCP 4 way handshake
    updateSeq();
    int retries = 0;
    const int maxRetries = 10;
    const int timeoutDuration = 1000; // timeout in milliseconds

    while (retries < maxRetries) {
        // send FIN
        OrzTCPPacket packet;
        OrzTCPHeaderEncode(&packet.header, TYPE_FIN, seq, 0, 0, 0);
        OrzTCPSetHeaderChecksum(&packet.header);
        if (udpSendPacket(&packet, to) == false) {
            handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
            return false;
        }

        // start timer
        auto startTime = std::chrono::high_resolution_clock::now();

        bool acked = false;
        int ack = 0;
        // wait for FINACK
        while (!acked) {
            // check if timeout
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            if (duration > timeoutDuration) {
                // timeout reched
                std::cout << "[INFO ] Timeout, retrying" << retries + 1 << "/" << maxRetries << std::endl;
                break;
            }

            // receive packet
            OrzTCPPacket *recvPacket = NULL;
            sockaddr_in from;
            if (udpRecvPacket(recvPacket, from) == false) {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    // timeout
                    delete[] reinterpret_cast<char*>(recvPacket);
                    std::cout << "[INFO ] Timeout, retrying" << retries + 1 << "/" << maxRetries << std::endl;
                    break;
                } else {
                    delete[] reinterpret_cast<char*>(recvPacket);
                    handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                    return false;
                }
            }

            // check if FINACK and not corrupted
            if (recvPacket->header.type == (TYPE_FIN | TYPE_ACK) && checkSum(&recvPacket->header) && recvPacket->header.ack == seq + 1) {
                acked = true;
                ack = recvPacket->header.seq + 1;
                break;
            }

            // otherwise, ignore the packet
        }

        // check if FINACK is received
        if (acked) {
            // stop timer
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            std::cout << "[INFO ] FINACK received in " << duration << "ms" << std::endl;
            // send ACK
            OrzTCPHeaderEncode(&packet.header, TYPE_ACK, seq + 1, ack, 0, 0);
            OrzTCPSetHeaderChecksum(&packet.header);
            if (udpSendPacket(&packet, to) == false) {
                handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
                return false;
            }
            return true;
        }

        // timeout reached, resend packet
        retries++;
    }

    // max retries reached
    std::cout << "[ERROR] Max retries reached" << std::endl;
    return false;
}

bool RDTClient::rdtSendFile(FILE* file, const char* targetIP, int targetPort) {
    // establish TCP connection
    sockaddr_in to;
    memset((char *)&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(targetPort);
    to.sin_addr.S_un.S_addr = inet_addr(targetIP);
    while (tcpConnect(to) == false) {
        std::cout << "[ERROR] tcpConnect() failed, retrying" << std::endl;
    } 

    std::cout << "[INFO ] OrzTCP connection established" << std::endl;

    // send file
    char buffer[BUFSIZE];
    int readLen = 0;
    int totalReadLen = 0;
    while ((readLen = fread(buffer, 1, BUFSIZE - sizeof(OrzTCPHeader), file)) > 0) {
        // send packet
        if (rdtSend(buffer, readLen, to) == false) {
            handleError("[ERROR] rdtSend() failed", WSAGetLastError());
            return false;
        }
        totalReadLen += readLen;
        // std::cout << "[INFO ] " << totalReadLen << " bytes sent" << std::endl;
    }

    std::cout << "[INFO ] File sent" << std::endl;

    // terminate TCP connection
    if (tcpTerminate(to) == false) {
        handleError("[ERROR] tcpTerminate() failed", WSAGetLastError());
        return false;
    }

    std::cout << "[INFO ] OrzTCP connection terminated" << std::endl;

    return true;
}

int main() {
    // let user select file
    std::cout << "[INFO ] Please select a file to send" << std::endl;
    std::string filename;
    std::cin >> filename;

    // open file
    FILE* file = fopen(filename.c_str(), "rb");
    if (file == NULL) {
        std::cout << "[ERROR] fopen() failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // print file info
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::cout << "[INFO ] File size: " << fileSize << " bytes" << std::endl;

    // send file
    RDTClient client(IP, PORT);
    if (client.rdtSendFile(file, SERVER_IP, SERVER_PORT) == false) {
        std::cout << "[ERROR] rdtSendFile() failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // close file
    fclose(file);

    return 0;
}
