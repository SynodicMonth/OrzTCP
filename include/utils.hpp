#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <cstdio>
#include <chrono>
#include <optional>
#include <thread>
#include <future>
#include "protocol.hpp"

// print log
void log(const char *msg);

// print error
void err(const char *msg);

// print debug info
void debug(const char *msg);

template <typename Func, typename... Args>
auto timeout_call(unsigned int milliseconds, Func&& func, Args&&... args) -> std::optional<decltype(func(args...))>;

std::string get_debug_str(const char *prefix, const OrzTCPPacket *packet);
#endif