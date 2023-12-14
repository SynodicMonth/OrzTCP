#ifndef RDT_HPP
#define RDT_HPP
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <deque>
#include <queue>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>
#include <map>
#include <thread>
#include <set>
#include "protocol.hpp"
#include "utils.hpp"
#include "udp.hpp"

extern const int E_PASSIVE_CLOSE;

class RDTSender {
public:
    enum ARQType {
        STOP_AND_WAIT,
        GO_BACK_N,
        SELECTIVE_REPEAT
    };
    enum State {
        CLOSED,
        SYN_SENT,
        ESTABLISHED,
        FIN_SENT,
        FIN_ACKED
    };

    RDTSender(const char *ip, int port, ARQType type);
    ~RDTSender();

    int connect(const char *ip, int port);
    int terminate();

    int send_data(const char *data, int len);

    unsigned int misses = 0;
    unsigned int get_packets_sent() {
        return udp.packets_sent;
    }
    unsigned int get_packets_recv() {
        return udp.packets_recv;
    }
    void set_window_size(unsigned int size) {
        N = size;
    }
    void set_timeout(unsigned int timeout) {
        arq_timeout = timeout;
    }
    State get_state() {
        return state;
    }

private:
    const unsigned int TIMEOUT = 1000; // 1000ms
    const unsigned int MAX_RETRIES = 10;
    UDP udp;
    ARQType type;
    struct sockaddr_in target_addr;
    std::atomic<State> state = CLOSED;

    unsigned int base = 1;
    unsigned int next_seq = 1;
    unsigned int N = 8;
    unsigned int arq_timeout = 50; // ms

    std::deque<OrzTCPPacket*> send_buffer;
    std::mutex window_mtx;
    std::mutex timer_mtx;
    std::condition_variable window_cv;
    std::condition_variable timer_cv;
    std::condition_variable timer_reset_cv;
    std::thread timer_thread;
    std::thread recv_thread; 
    std::atomic<bool> timer_running = false;

    // for GBN (stop and wait is a special case of GBN)
    int send_data_go_back_n(const char *data, int len);
    void start_timer_go_back_n();
    void stop_timer_go_back_n();
    void reset_timer_go_back_n();
    int timer_go_back_n();
    int recv_ack_go_back_n();

    // for SR
    struct PacketTimer {
        OrzTCPPacket *packet;
        std::chrono::system_clock::time_point time;

        PacketTimer(OrzTCPPacket *packet, std::chrono::system_clock::time_point time) {
            this->packet = packet;
            this->time = time;
        }

        bool operator>(const PacketTimer &rhs) const {
            return time > rhs.time;
        }
    };
    std::priority_queue<PacketTimer, std::vector<PacketTimer>, std::greater<>> timer_queue;
    std::map<OrzTCPPacket*, bool> acked;
    int send_data_selective_repeat(const char *data, int len);
    int recv_ack_selective_repeat();
    int timer_selective_repeat();
    void update_timer_selective_repeat(OrzTCPPacket *packet);
};

class RDTReciever {
public:
    enum ARQType {
        STOP_AND_WAIT,
        GO_BACK_N,
        SELECTIVE_REPEAT
    };
    enum State {
        CLOSED,
        SYN_RCVD,
        ESTABLISHED,
        FIN_RCVD,
        FIN_ACKED
    };

    RDTReciever(const char *ip, int port, ARQType type);
    ~RDTReciever();

    int startup();
    int close();

    int recv_data(char *data, int len);
    void set_window_size(unsigned int size) {
        N = size;
    }

private:
    const unsigned int TIMEOUT = 1000; // 1000ms
    const unsigned int MAX_RETRIES = 10;
    UDP udp;
    ARQType type;
    struct sockaddr_in target_addr;
    std::atomic<State> state = CLOSED;

    // for GBN
    std::atomic<unsigned int> expected_seq = 1;
    unsigned int N = 1;
    int recv_data_go_back_n(char *data, int len);

    // for SR
    struct PacketComparator {
        bool operator() (const OrzTCPPacket* lhs, const OrzTCPPacket* rhs) const {
            return lhs->header.seq < rhs->header.seq;
        }
    };
    std::set<OrzTCPPacket*, PacketComparator> recv_buffer;
    int recv_data_selective_repeat(char *data, int len);
    
};

#endif