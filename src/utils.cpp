#include "utils.hpp"
// #define DEBUG

// print log with green color, time(ms) and [LOG] prefix
void log(const char *msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *parts = std::localtime(&now_c);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;
    printf("\033[32m[%02d:%02d:%02d.%06ld] [LOG] %s\033[0m\n", parts->tm_hour, parts->tm_min, parts->tm_sec, microseconds, msg);
}

// print error with red color, time and [ERR] prefix
void err(const char *msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *parts = std::localtime(&now_c);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;
    printf("\033[31m[%02d:%02d:%02d.%06ld] [ERR] %s\033[0m\n", parts->tm_hour, parts->tm_min, parts->tm_sec, microseconds, msg);
    exit(1);
}

// print debug info with blue color, time and [DBG] prefix
void debug(const char *msg) {
    #ifdef DEBUG
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *parts = std::localtime(&now_c);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;
    printf("[%02d:%02d:%02d.%06ld] [DBG] %s\n", parts->tm_hour, parts->tm_min, parts->tm_sec, microseconds, msg);
    #endif
}

// call function with timeout in milliseconds
template <typename Func, typename... Args>
auto timeout_call(unsigned int milliseconds, Func&& func, Args&&... args) -> std::optional<decltype(func(args...))> {
    auto future = std::async(std::launch::async, std::forward<Func>(func), std::forward<Args>(args)...);
    if (future.wait_for(std::chrono::milliseconds(milliseconds)) == std::future_status::timeout) {
        return {};
    }
    return future.get();
}

// get debug string of packet
// in format "prefix ACK/SYN/FIN/DAT len=xxx seq=xxx ack=xxx checksum=xxx"
std::string get_debug_str(const char *prefix, const OrzTCPPacket *packet) {
    std::string ret = prefix;
    switch (packet->header.type) {
        case TYPE_ACK:
            ret += " ACK";
            break;
        case TYPE_SYN:
            ret += " SYN";
            break;
        case TYPE_FIN:
            ret += " FIN";
            break;
        case TYPE_DATA:
            ret += " DAT";
            break;
        case TYPE_SYN | TYPE_ACK:
            ret += " SYN+ACK";
            break;
        case TYPE_FIN | TYPE_ACK:
            ret += " FIN+ACK";
            break;
        default:
            ret += " UNK";
            break;
    }
    ret += " len=";
    ret += std::to_string(packet->header.len);
    ret += " seq=";
    ret += std::to_string(packet->header.seq);
    ret += " ack=";
    ret += std::to_string(packet->header.ack);
    ret += " sum=";
    ret += std::to_string(packet->header.checksum);
    return ret;
}