/* common.h
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#pragma once

#include <epicsGuard.h>
#include <epicsMutex.h>

#include <numeric>
#include <string>
#include <vector>

// These only work with C99 and above but that's okay since
// we're compiling C++11 code.
#define LOG_ERROR(...) common::epicsipmi_log(1, __VA_ARGS__)
#define LOG_WARN(...)  common::epicsipmi_log(2, __VA_ARGS__)
#define LOG_INFO(...)  common::epicsipmi_log(3, __VA_ARGS__)
#define LOG_DEBUG(...) common::epicsipmi_log(4, __VA_ARGS__)

namespace common {

void epicsipmi_log(unsigned severity, const std::string& fmt, ...);

std::vector<std::string> split(const std::string& text, char delimiter=' ', unsigned maxSplits=0);

template <template <class...> class Container, class Type, class ... Allocator>
std::string merge(const Container<Type, std::allocator<std::string>>& container, const std::string& delimiter=" ")
{
    auto lambda = [delimiter](const std::string& s1, const std::string& s2) { return s1.empty() ? s2 : s1 + delimiter + s2; };
    return std::accumulate(container.begin(), container.end(), std::string(), lambda);
    return "";
}

template <template<class, class> class Container, class Type, class Allocator>
bool contains(const Container<Type,Allocator>& container, const typename Container<Type,Allocator>::value_type& pattern)
{
    for (auto& e: container) {
        if (e == pattern)
            return true;
    }
    return false;
};

void copy(const std::string& str, char* buf, size_t bufSize);

std::string to_upper(const std::string& s);

template <typename T, size_t S=0>
struct buffer {
    T* data{nullptr};
    const unsigned int max_size{0};
    unsigned int size{0};

    buffer()
        : max_size(S)
    {
        data = new T[S];
    }

    buffer(size_t max_size_)
        : max_size(max_size_)
    {
        // new[] throws bad_alloc exception
        data = new T[max_size];
    }
};

typedef epicsGuard<epicsMutex> ScopedLock;

/**
 * @brief Return local timezone number of seconds from UTC time
 */
int getUtcOffset();

}
