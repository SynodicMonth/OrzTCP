#include "utils.hpp"

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
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *parts = std::localtime(&now_c);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;
    printf("[%02d:%02d:%02d.%06ld] [DBG] %s\n", parts->tm_hour, parts->tm_min, parts->tm_sec, microseconds, msg);
}