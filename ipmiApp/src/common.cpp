/* common.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#include "common.h"

#include <algorithm>
#include <iterator>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sstream>

#include <epicsTime.h>

namespace common {

static unsigned epicsipmiLogLevel = 4;

void epicsipmi_log(unsigned severity, const std::string& fmt, ...)
{
    static std::string severities[] = {
        "",
        "ERROR",
        "WARN",
        "INFO",
        "DEBUG"
    };

    if (severity <= epicsipmiLogLevel) {
        std::string msg;

        if (severity > 4)
            severity = 4;

        epicsTimeStamp now;
        if (epicsTimeGetCurrent(&now) == 0) {
            char nowText[40] = {'\0'};
            epicsTimeToStrftime(nowText, sizeof(nowText), "[%Y/%m/%d %H:%M:%S.%03f] ", &now);
            msg += nowText;
        }
        if (fmt.back() != '\n')
            msg += "epicsipmi " + severities[severity] + ": " + fmt + "\n";
        else
            msg += "epicsipmi " + severities[severity] + ": " + fmt;

        va_list args;
        va_start(args, fmt);
        vprintf(msg.c_str(), args);
        va_end(args);
    }
}

std::vector<std::string> split(const std::string& text, char delimiter, unsigned maxSplits)
{
    std::vector<std::string> tokens;

    if (maxSplits == 0)
        maxSplits = -1;
    size_t start = 0;
    while (maxSplits-- > 0 && start != std::string::npos) {
        size_t end = text.find(delimiter, start);
        if (end == std::string::npos)
            break;
        tokens.push_back(std::move(text.substr(start, end-start)));
        start = text.find_first_not_of(delimiter, end);
    }
    if (start != std::string::npos)
        tokens.push_back(std::move(text.substr(start)));
    return tokens;
}

void copy(const std::string& str, char* buf, size_t bufSize)
{
    auto n = str.copy(buf, bufSize);
    buf[std::min(bufSize-1, n)] = 0;
}

std::string to_upper(const std::string& s)
{
    std::string out(s);
    for_each(out.begin(), out.end(), [](char& c) { c = ::toupper((unsigned char)c); });
    return out;
}

int getUtcOffset() {
    time_t now;
    time(&now);
    auto timeinfo = gmtime(&now);
    time_t utc = mktime(timeinfo);
    timeinfo = localtime(&now);
    time_t local = mktime(timeinfo);

    int offsetFromUTC = difftime(utc, local);
    // Adjust for DST, but then we need to update periodically
    if (timeinfo->tm_isdst)
        offsetFromUTC -= 3600;

    return offsetFromUTC;
}

}; // namespace common
