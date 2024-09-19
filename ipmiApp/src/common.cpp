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
#include <iomanip>

#include <epicsTime.h>

namespace common {

static unsigned epicsipmiLogLevel = 4;

std::string hex_dump(uint8_t const * const buff, unsigned int const pos, size_t const len)
{

	std::stringstream ss;

	/** loop through the buffer.*/
	for(size_t i = 0; i < len; i++) {
		/** Format it and print it in Hex.*/
		ss << "0x" << std::setfill('0') << std::setw(2) << std::hex << (unsigned int) buff[(i+pos)];
	}

	return ss.str();
}

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

}; // namespace common
