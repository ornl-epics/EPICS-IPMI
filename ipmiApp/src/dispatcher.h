/* dispatcher.h
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#pragma once

#include <functional>
#include <provider.h>
#include <vector>

/**
 * @namespace Dispatcher
 * @brief Manages IPMI objects and distributes EPICS shell interfaces or EPICS records requests.
 */
namespace dispatcher {

enum class EntityType {
    SENSOR,
    FRU,
    PICMG_LED,
};

enum class AuthType {
    NONE,
};

/**
 * @brief Establishes connection with IPMI sub-system.
 * @param connection_id unique connection id
 * @param hostname to connect to
 * @param username credentials to use for connection
 * @param password credentials to use for connection
 * @param auth_type one of 'none', 'plain', 'md2', 'md5'
 * @param protocol to be used
 * @param privlevel privilege level to use for all queries, one of 'user', 'operator', 'admin'
 * @return true on connection success, false otherwise
 *
 * For now only IpmiTool implementation is supported by this function.
 */
bool connect(const std::string& connection_id, const std::string& hostname,
             const std::string& username, const std::string& password,
             const std::string& authtype, const std::string& protocol,
             const std::string& privlevel);

/**
 * @brief Scans for IPMI entity types and prints them to console.
 * @param connection_id
 * @param types valid options are 'sensors', 'fru'
 */
void scan(const std::string& connection_id, const std::vector<EntityType>& types);

/**
 * @brief Scans for IPMI entity types and prints corresponding EPICS records to file.
 * @param connection_id
 * @param filename Full path to filename to be saved
 * @param pv_prefix Prefix to be prepended to record names
 */
void printDb(const std::string& connection_id, const std::string& path, const std::string& pv_prefix);

/**
 * @brief Verify that record link is indeed valid IPMI address
 * @param address to be checked
 * @return true if link points to connected IPMI sub-system, false otherwise
 */
bool checkLink(const std::string& address);

/**
 * @brief Finds existing IPMI sub-system and schedules asynchronous processing.
 * @param rec to process
 * @return true when connection found and task scheduled, false otherwise
 *
 * Starts processing the record. It first verifies that record's link points
 * to an existing IPMI connection, it returns false otherwise. It then schedules
 * asynchronous task to do actual work like retrieving sensor value and metadata
 * or to send new value to IPMI entity. Only when this function returns true
 * it is guaranteed that callbackRequestProcessCallback(rec) will be called
 * eventually (even when no changes were made within connection timeout).
 */
template<typename T>
bool process(T* rec);

bool scheduleGet(const std::string& address, const std::function<void()>& cb, Provider::Entity& entity);

}; // namespace
