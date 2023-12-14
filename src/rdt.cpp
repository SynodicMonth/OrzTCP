#include "rdt.hpp"

const int E_PASSIVE_CLOSE = -2;

RDTSender::RDTSender(const char *ip, int port, ARQType type)
    : udp(ip, port), type(type) {
    if (type == STOP_AND_WAIT) {
        N = 1;
    } else if (type == GO_BACK_N) {
        // N = 8;
    } else if (type == SELECTIVE_REPEAT) {
        N = 8;
    } else {
        err("RDTSender::RDTSender(): Unknown ARQ type");
    }
}

RDTSender::~RDTSender() {

}

int RDTSender::connect(const char *ip, int port) {
    // if already connected
    if (state != CLOSED) {
        err("RDTSender::connect(): Already connected");
    }
    // set target addr
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &target_addr.sin_addr) <= 0) {
        err("RDTSender::connect(): Error converting ip address");
    }
    // 2 way handshake
    OrzTCPPacket packet;
    OrzTCPHeaderEncode(&packet.header, TYPE_SYN, 0, 0, 0);
    // retry until SYN+ACK received
    for (int i = 0; i < MAX_RETRIES; i++) {
        // send SYN
        udp.send_packet(&packet, &target_addr);
        state = SYN_SENT;
        log("RDTSender::connect(): SYN sent");
        // recv SYN+ACK
        OrzTCPPacket recv_packet;
        if (udp.recv_packet(&recv_packet, &target_addr, TIMEOUT) > 0) {
            // check if SYN+ACK and not corrupted
            if (recv_packet.header.type & TYPE_SYN 
                && recv_packet.header.type & TYPE_ACK 
                && checkSum(&recv_packet.header)) {
                    log("RDTSender::connect(): Connection established");
                state = ESTABLISHED;
                // start recv thread
                if (type == GO_BACK_N || type == STOP_AND_WAIT) {
                    recv_thread = std::thread(&RDTSender::recv_ack_go_back_n, this);
                } else if (type == SELECTIVE_REPEAT) {
                    recv_thread = std::thread(&RDTSender::recv_ack_selective_repeat, this);
                }
                // start timer thread
                if (type == GO_BACK_N || type == STOP_AND_WAIT) {
                    timer_thread = std::thread(&RDTSender::timer_go_back_n, this);
                } else if (type == SELECTIVE_REPEAT) {
                    timer_thread = std::thread(&RDTSender::timer_selective_repeat, this);
                }
                return 0;
            }
        } else {
            state = CLOSED;
            log("RDTSender::connect(): Timeout, retry");
        }
    }
    err("RDTSender::connect(): Timeout");
    return -1;
}

int RDTSender::terminate() {
    // if not connected
    if (target_addr.sin_port == 0) {
        err("RDTSender::terminate(): Not connected");
    }
    // 2 way handshake
    OrzTCPPacket packet;
    OrzTCPHeaderEncode(&packet.header, TYPE_FIN, next_seq, 0, 0);
    // retry until FIN-ACK received
    for (int i = 0; i < MAX_RETRIES; i++) {
        // send FIN
        state = FIN_SENT;
        // join recv thread and timer thread
        if (recv_thread.joinable()) recv_thread.join();
        if (timer_thread.joinable()) timer_thread.join();
        udp.send_packet(&packet, &target_addr);
        // recv FIN+ACK
        OrzTCPPacket recv_packet;
        if (udp.recv_packet(&recv_packet, &target_addr, TIMEOUT) > 0) {
            // check if FIN+ACK and not corrupted
            if (recv_packet.header.type & TYPE_FIN
                && recv_packet.header.type & TYPE_ACK
                && checkSum(&recv_packet.header)) {
                    log("RDTSender::terminate(): Connection terminated");
                    state = FIN_ACKED;
                    // clean up
                    for (auto packet : send_buffer) {
                        delete[] reinterpret_cast<char *>(packet);
                    }
                return 0;
            }
        }
    }

    err("RDTSender::terminate(): Timeout");
    return -1;
}

int RDTSender::send_data(const char *data, int len) {
    if (state != ESTABLISHED) {
        err("RDTSender::send_data(): Not connected");
    }
    if (type == STOP_AND_WAIT) {
        // just use go back n but with N = 1
        return send_data_go_back_n(data, len);
    } else if (type == GO_BACK_N) {
        return send_data_go_back_n(data, len);
    } else if (type == SELECTIVE_REPEAT) {
        return send_data_selective_repeat(data, len);
    } else {
        err("RDTSender::send_data(): Unknown ARQ type");
    }
    return -1;
}

int RDTSender::send_data_go_back_n(const char *data, int len) {
    std::unique_lock<std::mutex> lck(window_mtx);
    window_cv.wait(lck, [this] { return next_seq < base + N; });
    // send packet
    OrzTCPPacket *packet = reinterpret_cast<OrzTCPPacket *>(new char[sizeof(OrzTCPPacket) + len]);
    OrzTCPHeaderEncode(&packet->header, TYPE_DATA, next_seq, 0, len);
    memcpy(packet->payload, data, len);
    OrzTCPSetHeaderChecksum(&packet->header);
    send_buffer.push_back(packet);
    udp.send_packet(packet, &target_addr);

    // start timer
    if (next_seq == base) {
        start_timer_go_back_n();
    }

    // update next_seq
    next_seq++;
    return len;
}

int RDTSender::send_data_selective_repeat(const char *data, int len) {
    std::unique_lock<std::mutex> lck(window_mtx);
    // wait until window is not full
    window_cv.wait(lck, [this] { return next_seq < base + N; });
    // send packet
    OrzTCPPacket *packet = reinterpret_cast<OrzTCPPacket *>(new char[sizeof(OrzTCPPacket) + len]);
    OrzTCPHeaderEncode(&packet->header, TYPE_DATA, next_seq, 0, len);
    memcpy(packet->payload, data, len);
    OrzTCPSetHeaderChecksum(&packet->header);
    send_buffer.push_back(packet);
    acked[packet] = false;

    if (timer_queue.empty()) {
        timer_queue.push(PacketTimer(packet, std::chrono::system_clock::now() + std::chrono::milliseconds(arq_timeout)));
        udp.send_packet(packet, &target_addr);
        timer_cv.notify_one();
    } else {
        timer_queue.push(PacketTimer(packet, std::chrono::system_clock::now() + std::chrono::milliseconds(arq_timeout)));
        udp.send_packet(packet, &target_addr);
    }

    // update next_seq
    next_seq++;
    return len;
}

int RDTSender::recv_ack_go_back_n() {
    // thread that receives ack and updates base
    OrzTCPPacket packet;
    while (true) {
        if (state == FIN_SENT && base == next_seq) {
            stop_timer_go_back_n();
            return 0;
        }
        int len = udp.recv_packet(&packet, &target_addr);
        {
            // fetch lock
            std::unique_lock<std::mutex> lck(window_mtx);
            // check if ack and not corrupted
            if (len > 0) {
                if (packet.header.type & TYPE_ACK && checkSum(&packet.header)) {
                    if (packet.header.ack >= base) {
                        // pop buffer
                        for (int i = 0; i < packet.header.ack - base + 1; i++) {
                            delete[] reinterpret_cast<char *>(send_buffer.front());
                            send_buffer.pop_front();
                        }
                        // update base
                        base = packet.header.ack + 1;
                        if (base == next_seq) {
                            stop_timer_go_back_n();
                        } else {
                            reset_timer_go_back_n();
                        }
                        debug(("RDTSender::recv_ack_go_back_n(): slide window to [" 
                            + std::to_string(base) 
                            + ", "
                            + std::to_string(next_seq) 
                            + "]").c_str());
                        window_cv.notify_one();
                    }
                }
            }
        }

    }
    return 0;
}

void RDTSender::update_timer_selective_repeat(OrzTCPPacket *packet) {
    std::unique_lock<std::mutex> lck(timer_mtx);
    if (timer_queue.top().packet == packet) {
        timer_queue.pop();
        timer_reset_cv.notify_one();
    } else {
        // remove the packet from the queue
        std::priority_queue<PacketTimer, std::vector<PacketTimer>, std::greater<>> new_queue;
        while (!timer_queue.empty()) {
            if (timer_queue.top().packet == packet) {
                timer_queue.pop();
            } else {
                new_queue.push(timer_queue.top());
                timer_queue.pop();
            }
        }
        timer_queue = new_queue;
    }
}

int RDTSender::recv_ack_selective_repeat() {
    // thread that receives ack and updates base
    OrzTCPPacket packet;
    while (true) {
        if (state == FIN_SENT && base == next_seq) {
            return 0;
        }
        int len = udp.recv_packet(&packet, &target_addr);
        {
            // fetch lock
            std::unique_lock<std::mutex> lck(window_mtx);
            // check if ack and not corrupted
            if (len > 0) {
                if (packet.header.type & TYPE_ACK && checkSum(&packet.header)) {
                    if (packet.header.ack >= base) {
                        unsigned int old_base = base;
                        // mark acked packets
                        acked[send_buffer[packet.header.ack - base]] = true;
                        // update timer
                        update_timer_selective_repeat(send_buffer[packet.header.ack - base]);
                        // pop buffer
                        while (!send_buffer.empty() && acked[send_buffer.front()]) {
                            acked.erase(send_buffer.front());
                            delete[] reinterpret_cast<char *>(send_buffer.front());
                            send_buffer.pop_front();
                            base++;
                        }
                        if (base != old_base) {
                            debug(("RDTSender::recv_ack_sr(): slide window to [" 
                            + std::to_string(base) 
                            + ", "
                            + std::to_string(next_seq) 
                            + "]").c_str());
                            window_cv.notify_one();
                        } else {
                            std::string s = "RDTSender::recv_ack_sr(): ";
                            for (auto packet : send_buffer) {
                                s += std::to_string(packet->header.seq);
                                if (acked[packet]) {
                                    s += "(Y) ";
                                } else {
                                    s += "(N) ";
                                }
                            }
                            debug(s.c_str());
                        }
                    }
                }
            }
        }
    }
    return 0;
}

void RDTSender::start_timer_go_back_n() {
    std::unique_lock<std::mutex> lck(timer_mtx);
    timer_running = true;
    timer_cv.notify_one();
}

void RDTSender::stop_timer_go_back_n() {
    std::unique_lock<std::mutex> lck(timer_mtx);
    timer_running = false;
    timer_cv.notify_one();
    timer_reset_cv.notify_one();
}

void RDTSender::reset_timer_go_back_n() {
    std::unique_lock<std::mutex> lck(timer_mtx);
    timer_reset_cv.notify_one();
}

int RDTSender::timer_go_back_n() {

    while (true) {
        // Wait for timer to be started
        if (state == FIN_SENT && !timer_running) {
            return 0;
        }

        std::cv_status stat;
        while (!timer_running) {}

        {
        std::unique_lock<std::mutex> lck(timer_mtx);
        stat = timer_reset_cv.wait_for(lck, std::chrono::milliseconds(arq_timeout));
        }

        if (stat == std::cv_status::timeout && timer_running) {
            std::unique_lock<std::mutex> window_lck(window_mtx);
            // Timeout handling
            debug("RDTSender::timer_go_back_n(): Timeout");
            misses++;
            // next_seq = base;
            for (auto packet : send_buffer) {
                udp.send_packet(packet, &target_addr);
            }
        } else {
            // Reset timer
            debug("RDTSender::timer_go_back_n(): Reset timer");
        }
    }
    return 0;
}

int RDTSender::timer_selective_repeat() {
    while (true) {
        std::unique_lock<std::mutex> lck(timer_mtx);
        if (state == FIN_SENT && base == next_seq) {
            return 0;
        }
        if (timer_queue.empty()) {
            // timer_cv.wait(lck);
        } else {
            auto now = std::chrono::system_clock::now();
            auto next_timeout = timer_queue.top().time;

            if (now >= next_timeout) {
                // Timeout handling
                debug("RDTSender::timer_selective_repeat(): Timeout");
                misses++;
                // resend packet
                OrzTCPPacket *packet = timer_queue.top().packet;
                udp.send_packet(packet, &target_addr);
                timer_queue.pop();
                timer_queue.push(PacketTimer(packet, std::chrono::system_clock::now() + std::chrono::milliseconds(arq_timeout)));
            } else {
                timer_reset_cv.wait_until(lck, next_timeout);
            }
        }
    }
    return 0;
}

RDTReciever::RDTReciever(const char *ip, int port, ARQType type)
    : udp(ip, port), type(type) {
    if (type == GO_BACK_N || type == STOP_AND_WAIT) {
        N = 1;
    } else if (type == SELECTIVE_REPEAT) {
        N = 8;
    }
}

RDTReciever::~RDTReciever() {
    
}

int RDTReciever::startup() {
    // waiting for SYN
    OrzTCPPacket packet;
    debug("RDTReciever::startup(): Waiting for SYN");
    while (state == CLOSED) {
        // recv SYN
        OrzTCPPacket recv_packet;
        if (udp.recv_packet(&recv_packet, &target_addr, TIMEOUT) > 0) {
            // check if SYN and not corrupted
            if (recv_packet.header.type & TYPE_SYN
                && checkSum(&recv_packet.header)) {
                    state = SYN_RCVD;
                    // send SYN+ACK
                    OrzTCPHeaderEncode(&packet.header, TYPE_SYN | TYPE_ACK, 0, 0, 0);
                    udp.send_packet(&packet, &target_addr);
                    debug("RDTReciever::startup(): Connection established");
                    state = ESTABLISHED;
                    return 0;
            }
        }
    }
    return -1;
}

int RDTReciever::recv_data(char *data, int len) {
    if (state != ESTABLISHED) {
        err("RDTReciever::recv_data(): Not connected");
    }
    if (type == STOP_AND_WAIT) {
        return recv_data_go_back_n(data, len);
    } else if (type == GO_BACK_N) {
        return recv_data_go_back_n(data, len);
    } else if (type == SELECTIVE_REPEAT) {
        return recv_data_selective_repeat(data, len);
    } else {
        err("RDTReciever::recv_data(): Unknown ARQ type");
    }
    return -1;
}

int RDTReciever::recv_data_go_back_n(char *data, int len) {
    // recv packet
    OrzTCPPacket *packet = reinterpret_cast<OrzTCPPacket *>(new char[BUF_SIZE]);
    bool valid_packet = 0;
    while (!valid_packet) {
        int packet_size = udp.recv_packet(packet, &target_addr, 0);
        if (packet_size > 0) {
            // check if DATA and not corrupted
            if ((packet->header.type == TYPE_DATA)
                && checkSum(&packet->header)
                && packet->header.seq == expected_seq) {
                    if (packet->header.len > len) {
                        err("RDTReciever::recv_data(): Buffer too small");
                    }
                    // send ACK
                    OrzTCPPacket ack_packet;
                    OrzTCPHeaderEncode(&ack_packet.header, TYPE_ACK, 0, packet->header.seq, 0);
                    udp.send_packet(&ack_packet, &target_addr);
                    // copy data
                    memcpy(data, packet->payload, packet->header.len);
                    expected_seq++;
                    int ret = packet->header.len;
                    delete[] reinterpret_cast<char *>(packet);
                    return ret;
            } else if (packet->header.type == TYPE_FIN
                && checkSum(&packet->header)) {
                    // terminate
                    state = FIN_RCVD;
                    OrzTCPPacket ack_packet;
                    OrzTCPHeaderEncode(&ack_packet.header, TYPE_FIN | TYPE_ACK, 0, packet->header.seq, 0);
                    udp.send_packet(&ack_packet, &target_addr);
                    state = FIN_ACKED;
                    return E_PASSIVE_CLOSE;
            } else {
                // send ACK
                OrzTCPPacket ack_packet;
                OrzTCPHeaderEncode(&ack_packet.header, TYPE_ACK, 0, expected_seq - 1, 0);
                udp.send_packet(&ack_packet, &target_addr);
                debug(("RDTReciever::recv_data(): Discard packet " + std::to_string(packet->header.seq)).c_str());
            }
        }
    }
    delete[] reinterpret_cast<char *>(packet);
    return 0;
}

int RDTReciever::recv_data_selective_repeat(char *data, int len) {
    // Check buffer for the expected packet first
    for (auto it = recv_buffer.begin(); it != recv_buffer.end(); it++) {
        if ((*it)->header.seq == expected_seq) {
            // Expected packet is in the buffer
            OrzTCPPacket* buffered_packet = *it;
            recv_buffer.erase(it);

            // Process the buffered packet
            if (buffered_packet->header.len > len) {
                err("RDTReciever::recv_data(): Buffer too small");
                delete[] reinterpret_cast<char *>(buffered_packet);
                return -1;
            }

            // if FIN, terminate
            if (buffered_packet->header.type == TYPE_FIN) {
                state = FIN_RCVD;
                OrzTCPPacket ack_packet;
                OrzTCPHeaderEncode(&ack_packet.header, TYPE_FIN | TYPE_ACK, 0, buffered_packet->header.seq, 0);
                udp.send_packet(&ack_packet, &target_addr);
                state = FIN_ACKED;
                delete[] reinterpret_cast<char *>(buffered_packet);
                return E_PASSIVE_CLOSE;
            }

            memcpy(data, buffered_packet->payload, buffered_packet->header.len);
            int ret = buffered_packet->header.len;
            expected_seq++;

            delete[] reinterpret_cast<char *>(buffered_packet);
            return ret;
        }
    }
    // If not in buffer, receive new packets
    while (true) {
        OrzTCPPacket *packet = reinterpret_cast<OrzTCPPacket *>(new char[BUF_SIZE]);
        int packet_size = udp.recv_packet(packet, &target_addr, 0);

        if (packet_size > 0) {
            // Check if packet is not corrupted
            if (!checkSum(&packet->header)) {
                delete[] reinterpret_cast<char *>(packet);
                continue;
            }

            // If it's the expected sequence number
            if (packet->header.seq == expected_seq) {
                if (packet->header.type == TYPE_DATA) {
                    // Process the received packet
                    if (packet->header.len > len) {
                        err("RDTReciever::recv_data(): Buffer too small");
                        delete[] reinterpret_cast<char *>(packet);
                        continue;
                    }

                    memcpy(data, packet->payload, packet->header.len);
                    int ret = packet->header.len;
                    
                    // Acknowledge the received packet
                    OrzTCPPacket ack_packet;
                    OrzTCPHeaderEncode(&ack_packet.header, TYPE_ACK, 0, expected_seq, 0);
                    udp.send_packet(&ack_packet, &target_addr);

                    expected_seq++;
                    delete[] reinterpret_cast<char *>(packet);
                    return ret;
                } else if (packet->header.type == TYPE_FIN) {
                    // Terminate
                    state = FIN_RCVD;
                    OrzTCPPacket ack_packet;
                    OrzTCPHeaderEncode(&ack_packet.header, TYPE_FIN | TYPE_ACK, 0, packet->header.seq, 0);
                    udp.send_packet(&ack_packet, &target_addr);
                    state = FIN_ACKED;
                    delete[] reinterpret_cast<char *>(packet);
                    return E_PASSIVE_CLOSE;
                } else {
                    // Unexpected packet type
                    delete[] reinterpret_cast<char *>(packet);
                    err("RDTReciever::recv_data(): Unexpected packet type");
                }
            } else if (packet->header.seq > expected_seq) {
                // If packet is out of order, buffer it
                recv_buffer.insert(packet);

                // if its not FIN, send ACK
                if (packet->header.type == TYPE_DATA) {
                    OrzTCPPacket ack_packet;
                    OrzTCPHeaderEncode(&ack_packet.header, TYPE_ACK, 0, packet->header.seq, 0);
                    udp.send_packet(&ack_packet, &target_addr);
                } else if (packet->header.type == TYPE_FIN) {
                    // Terminate later
                } else {
                    // Unexpected packet type
                    delete[] reinterpret_cast<char *>(packet);
                    err("RDTReciever::recv_data(): Unexpected packet type");
                }
            } else {
                // resend ack 
                OrzTCPPacket ack_packet;
                OrzTCPHeaderEncode(&ack_packet.header, TYPE_ACK, 0, packet->header.seq, 0);
                udp.send_packet(&ack_packet, &target_addr);
                delete[] reinterpret_cast<char *>(packet);
            }
        }
    }
}
