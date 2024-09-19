/* print.h
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#pragma once

#include "provider.h"

#include <string>
#include <vector>

namespace print {

void printScanReport(const std::string& header, const std::vector<Provider::Entity>& entities);

void printRecord(FILE* dbfile, const std::string& prefix, const Provider::Entity& entity);

}; // namespace print
