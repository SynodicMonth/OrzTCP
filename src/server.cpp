#include <iostream>
#include <string>
#include <winsock2.h>
#include <chrono>
#include "protocol.cpp"

#pragma comment(lib, "Ws2_32.lib")

const int BUFSIZE = 1400;
const char* IP = "127.0.0.1";
const int PORT = 8888;
const int TIMEOUT_MSEC = 100;

// Reliable Data Transfer 3.0 (RDT3.0) Server for file transfer
class RDTServer {
public:
    RDTServer(const char* ip, int port);
    ~RDTServer();

    void recvFile(const char* filename);

private:
    SOCKET serverSocket;
    struct sockaddr_in serverAddr;
    WSADATA wsa;
    sockaddr_in clientAddr;
    int rdtSeq = 0;

    bool rdtRecv(char* buf, int &len, const sockaddr_in& from); 

    bool udpSendPacket(const OrzTCPPacket* packet, const sockaddr_in& to);
    bool udpRecvPacket(OrzTCPPacket*& packet, const sockaddr_in& from);

    bool tcpAccept(const sockaddr_in& from);
    bool tcpTerminate(const sockaddr_in& from, int initAck);

    void cleanUp();
    void updateSeq();
    void handleError(const char* errorMsg, int errorCode);
};

RDTServer::RDTServer(const char* ip, int port) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        handleError("[ERROR] WSAStartup failed", WSAGetLastError());
    }

    // Create socket
    if ((serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        handleError("[ERROR] Could not create socket", WSAGetLastError());
    }

    // Prepare the sockaddr_in structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip);
    serverAddr.sin_port = htons(port);
    
    // Bind
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        handleError("[ERROR] Bind failed", WSAGetLastError());
    }

    // set timeout 
    int timeout = TIMEOUT_MSEC;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        handleError("[ERROR] setsockopt() failed", WSAGetLastError());
    }

    std::cout << "[INFO ] Server started on " << ip << ":" << port << std::endl;
}

RDTServer::~RDTServer() {
    cleanUp();
}

void RDTServer::cleanUp() {
    closesocket(serverSocket);
    WSACleanup();
}

void RDTServer::handleError(const char* errorMsg, int errorCode) {
    std::cout << errorMsg << " with error code: " << errorCode << std::endl;
    cleanUp();
    exit(EXIT_FAILURE);
}

bool RDTServer::udpSendPacket(const OrzTCPPacket* packet, const sockaddr_in& to) {
    int sendLen = sendto(serverSocket, (const char *)packet, sizeof(OrzTCPHeader) + packet->header.len, 0, (struct sockaddr *)&to, sizeof(to));
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

bool RDTServer::udpRecvPacket(OrzTCPPacket*& packet, const sockaddr_in& from) {
    int fromLen = sizeof(from);
    const int maxPacketSize = BUFSIZE;
    char* buffer = new char[maxPacketSize];
    
    int recvLen = recvfrom(serverSocket, buffer, maxPacketSize, 0, (struct sockaddr*)&from, &fromLen);
    if (recvLen == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
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
        handleError("[ERROR] Received packet is too small.", 0);
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

bool RDTServer::rdtRecv(char* buf, int& len, const sockaddr_in& from) {
    int fromLen = sizeof(from);

    // loop to keep listening
    while (true) {
        OrzTCPPacket* packet = NULL;
        if (udpRecvPacket(packet, from) == false) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                // timeout
                continue;
            } else {
                delete[] reinterpret_cast<char*>(packet);
                handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                return false;
            }
        }

        // check if recieved correct packet
        if (packet->header.rdtSeqAck == rdtSeq && checkSum(&packet->header) && packet->header.type != TYPE_FIN) {
            // std::cout << "[INFO ] Received packet SEQ" << int(packet->header.rdtSeqAck) << std::endl;
            // extract data
            len = packet->header.len;
            memcpy(buf, packet->payload, len);

            // send ack
            OrzTCPPacket ackPacket;
            OrzTCPHeaderEncode(&ackPacket.header, TYPE_ACK, 0, 0, 0, rdtSeq);
            OrzTCPSetHeaderChecksum(&ackPacket.header);
            if (udpSendPacket(&ackPacket, from) == false) {
                delete[] reinterpret_cast<char*>(packet);
                handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
                return false;
            }

            // update seq
            rdtSeq = 1 - rdtSeq;
            delete[] reinterpret_cast<char*>(packet);
            return true;
        } else {
            // check if received packet is a FIN packet
            if (packet->header.type == TYPE_FIN && checkSum(&packet->header)) {
                // call tcpTerminate() to terminate connection
                if (tcpTerminate(from, packet->header.seq + 1) == false) {
                    delete[] reinterpret_cast<char*>(packet);
                    handleError("[ERROR] tcpTerminate() failed", WSAGetLastError());
                    return false;
                } else {
                    delete[] reinterpret_cast<char*>(packet);
                    return false;
                }
            }
            // invalid packet, send ack for previous packet
            OrzTCPPacket ackPacket;
            OrzTCPHeaderEncode(&ackPacket.header, TYPE_ACK, 0, 0, 0, 1 - rdtSeq);
            OrzTCPSetHeaderChecksum(&ackPacket.header);
            if (udpSendPacket(&ackPacket, from) == false) {
                delete[] reinterpret_cast<char*>(packet);
                handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
                return false;
            }
        }
    }
}

bool RDTServer::tcpAccept(const sockaddr_in& from) {
    // simplified TCP 3 way handshake
    const int timeoutDuration = 1000;  // Timeout in milliseconds

    OrzTCPPacket *recvPacket = NULL;
    int ack = 0;
    bool synReceived = false;

    // Wait for SYN
    while (!synReceived) {
        if (udpRecvPacket(recvPacket, from) == false) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                // timeout
                continue;
            } else {
                delete[] reinterpret_cast<char*>(recvPacket);
                handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                return false;
            }
        }

        if (recvPacket->header.type == TYPE_SYN && checkSum(&recvPacket->header)) {
            ack = recvPacket->header.seq + 1;
            synReceived = true;
            std::cout << "[INFO ] Received SYN" << std::endl;
        }

        delete[] reinterpret_cast<char*>(recvPacket);
    }

    // Send SYN-ACK
    OrzTCPPacket synAckPacket;
    OrzTCPHeaderEncode(&synAckPacket.header, TYPE_SYN | TYPE_ACK, ack, ack, 0, 0);
    OrzTCPSetHeaderChecksum(&synAckPacket.header);
    if (udpSendPacket(&synAckPacket, from) == false) {
        handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
        return false;
    }

    // Wait for ACK
    auto startTime = std::chrono::high_resolution_clock::now();
    bool ackReceived = false;
    while (!ackReceived) {
        // Check for timeout
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        if (duration > timeoutDuration) {
            std::cout << "[INFO ] Timeout waiting for ACK" << std::endl;
            // assume half open connection
            return true;
        }

        if (udpRecvPacket(recvPacket, from) == false) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                // timeout
                continue;
            } else {
                delete[] reinterpret_cast<char*>(recvPacket);
                handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                return false;
            }
        }

        if (recvPacket->header.type == TYPE_ACK && checkSum(&recvPacket->header) && recvPacket->header.ack == ack + 1) {
            ackReceived = true;
        }

        delete[] reinterpret_cast<char*>(recvPacket);
    }

    return true;
}

bool RDTServer::tcpTerminate(const sockaddr_in& from, int initAck) {
    // simplified TCP 4 way handshake
    const int timeoutDuration = 1000;  // Timeout in milliseconds
    int ack = initAck;

    // Send FIN + ACK
    OrzTCPPacket finAckPacket;
    OrzTCPHeaderEncode(&finAckPacket.header, TYPE_FIN | TYPE_ACK, ack, ack, 0, 0);
    OrzTCPSetHeaderChecksum(&finAckPacket.header);
    if (udpSendPacket(&finAckPacket, from) == false) {
        handleError("[ERROR] udpSendPacket() failed", WSAGetLastError());
        return false;
    }

    // Wait for ACK
    auto startTime = std::chrono::high_resolution_clock::now();
    bool ackReceived = false;
    while (!ackReceived) {
        // Check for timeout
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        if (duration > timeoutDuration) {
            std::cout << "[INFO ] Timeout waiting for ACK" << std::endl;
            // half open connection
            std::cout << "[INFO ] Connection terminated" << std::endl;
            return true;
        }

        OrzTCPPacket *recvPacket = NULL;
        if (udpRecvPacket(recvPacket, from) == false) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                // timeout
                continue;
            } else {
                delete[] reinterpret_cast<char*>(recvPacket);
                handleError("[ERROR] udpRecvPacket() failed", WSAGetLastError());
                return false;
            }
        }

        if (recvPacket->header.type == TYPE_ACK && checkSum(&recvPacket->header) && recvPacket->header.ack == ack + 1) {
            ackReceived = true;
        }

        delete[] reinterpret_cast<char*>(recvPacket);
    }
    
    std::cout << "[INFO ] Connection terminated" << std::endl;
    return true;
}

void RDTServer::recvFile(const char* filename) {
    // waiting for connection
    std::cout << "[INFO ] Waiting for connection..." << std::endl;
    while (tcpAccept(clientAddr) == false) {
        std::cout << "[INFO ] Attempting to accept connection..." << std::endl;
    }

    std::cout << "[INFO ] Connection established" << std::endl;

    // start receiving file
    std::cout << "[INFO ] Receiving file..." << std::endl;
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        handleError("[ERROR] fopen() failed", WSAGetLastError());
    }

    // set timer
    auto startTime = std::chrono::high_resolution_clock::now();

    char buf[BUFSIZE];
    int len = 0;
    int totalLen = 0;
    while (rdtRecv(buf, len, clientAddr)) {
        // append to file
        fwrite(buf, sizeof(char), len, fp);
        totalLen += len;

        // print progress
        if (totalLen % 100 == 0) {
            std::cout << "[INFO ] Received " << totalLen << " bytes" << std::endl;
        }
    }

    // stop timer
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    fclose(fp);
    std::cout << "[INFO ] File received" << std::endl;

    // print file size
    fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    int fileSize = ftell(fp);
    std::cout << "[INFO ] File size: " << fileSize << " bytes" << std::endl;
    fclose(fp);

    // print time and throughput
    std::cout << "[INFO ] Time elapsed: " << duration << " ms" << std::endl;
    std::cout << "[INFO ] Throughput: " << fileSize / duration * 1000 / 1024 << " KB/s" << std::endl;
}

int main() {
    RDTServer server(IP, PORT);

    // let user input filename
    std::string filename;
    std::cout << "[INFO ] Please input filename: ";
    std::cin >> filename;

    server.recvFile(filename.c_str());

    return 0;
}
