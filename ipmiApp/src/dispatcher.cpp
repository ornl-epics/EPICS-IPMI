/* dispatcher.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#include "common.h"
#include "freeipmiprovider.h"
#include "print.h"
#include "dispatcher.h"

#include <cstring>
#include <map>
#include <string>
#include <iostream>
#include <sstream>

// EPICS records that we support
#include <aiRecord.h>
#include <stringinRecord.h>

namespace dispatcher {

static std::map<std::string, std::shared_ptr<FreeIpmiProvider>> g_connections; //!< Global map of connections.
static epicsMutex g_mutex; //!< Global mutex to protect g_connections.

/// @brief Split string on whitespaces and place tokens into vector
/// @param tokens 
/// @param link 
static void parse_inout_str(std::vector<std::string> &tokens, const std::string &link) {
    std::stringstream ss(link);
    std::string tok;
    
    while(ss >> tok) {
        tokens.push_back(tok);
    }
}

static std::pair<std::string, std::string> _parseLink(const std::string& link)
{
    auto tokens = common::split(link, ' ', 2);
    if (tokens.size() < 3 || tokens[0] != "ipmi")
        return std::make_pair(std::string(""), std::string(""));

    std::cout << "link: " << link << std::endl;
    std::cout << "t0: " << tokens[0] << ", t1: " << tokens[1] << ", t2: " << tokens[2] << std::endl;
    return std::make_pair(tokens[1], tokens[2]);
}

static std::string _createLink(const std::string& conn_id, const std::string& addr)
{
    return "@ipmi " + conn_id + " " + addr;
}

static std::shared_ptr<FreeIpmiProvider> _getConnection(const std::string& conn_id)
{
    common::ScopedLock lock(g_mutex);
    auto it = g_connections.find(conn_id);
    if (it != g_connections.end())
        return it->second;
    return nullptr;
}

bool connect(const std::string& conn_id, const std::string& hostname,
             const std::string& username, const std::string& password,
             const std::string& authtype, const std::string& protocol,
             const std::string& privlevel)
{
    common::ScopedLock lock(g_mutex);

    if (g_connections.find(conn_id) != g_connections.end())
        return false;

    std::shared_ptr<FreeIpmiProvider> conn;
    try {
        conn.reset(new FreeIpmiProvider(conn_id, hostname, username, password, authtype, protocol, privlevel));
    } catch (std::bad_alloc& e) {
        LOG_ERROR("can't allocate FreeIPMI provider\n");
        return false;
    } catch (std::runtime_error& e) {
        if (username.empty())
            LOG_ERROR("can't connect to %s - %s", hostname.c_str(), e.what());
        else
            LOG_ERROR("can't connect to %s as user %s - %s", hostname.c_str(), username.c_str(), e.what());
        return false;
    }

    g_connections[conn_id] = conn;
    return true;
}

void scan(const std::string& conn_id, const std::vector<EntityType>& types)
{
    g_mutex.lock();
    auto it = g_connections.find(conn_id);
    bool found = (it != g_connections.end());
    g_mutex.unlock();

    if (!found) {
        LOG_ERROR("no such connection " + conn_id);
        return;
    }

    auto conn = it->second;
    for (auto& type: types) {
        try {
            std::vector<Provider::Entity> entities;
            std::string header;
            if (type == EntityType::SENSOR) {
                entities = conn->getSensors();
                header = "Sensors:";
            } else if (type == EntityType::FRU) {
                entities = conn->getFrus();
                header = "FRUs:";
            } else if (type == EntityType::PICMG_LED) {
                entities = conn->getPicmgLeds();
                header = "PICMG LEDs:";
            }
            print::printScanReport(header, entities);
        } catch (std::runtime_error& e) {
            LOG_ERROR(e.what());
        }
    }
}

void printDb(const std::string& conn_id, const std::string& path, const std::string& pv_prefix)
{
    g_mutex.lock();
    auto it = g_connections.find(conn_id);
    bool found = (it != g_connections.end());
    g_mutex.unlock();

    if (!found) {
        LOG_ERROR("no such connection " + conn_id);
        return;
    }
    auto conn = it->second;

    FILE *dbfile = fopen(path.c_str(), "w+");
    if (dbfile == nullptr)
        LOG_ERROR("Failed to open output database file - %s", strerror(errno));

    try {
        auto sensors = conn->getSensors();
        for (auto& sensor: sensors) {
            auto inp = sensor.getField<std::string>("INP", "");
            if (!inp.empty()) {
                sensor["INP"] = _createLink(conn_id, inp);
                print::printRecord(dbfile, pv_prefix, sensor);
            }
        }

        auto frus = conn->getFrus();
        for (auto& fru: frus) {
            auto inp = fru.getField<std::string>("INP", "");
            if (!inp.empty()) {
                fru["INP"] = _createLink(conn_id, inp);
                print::printRecord(dbfile, pv_prefix, fru);
            }
        }

        auto leds = conn->getPicmgLeds();
        for (auto& led: leds) {
            auto inp = led.getField<std::string>("INP", "");
            if (!inp.empty()) {
                led["INP"] = _createLink(conn_id, inp);
                print::printRecord(dbfile, pv_prefix, led);
            }
        }

    } catch (...) {
        // TODO: do we need to log
    }

    fclose(dbfile);
}

void checkLink(const std::string& address) {
    std::vector<std::string> tokens;

    parse_inout_str(tokens, address);

    if(tokens.size() < 3) {
        throw std::invalid_argument("Link field does not have enough parameters. \'" + address + "\'");
    }
    
    /** First find the connection*/
    auto conn = _getConnection(tokens.at(0));
    if(!conn)
        throw std::invalid_argument("Link field can't find device \'@" + tokens.at(0) + "\'");
    
    /** Second find the FRU*/
    std::string s = tokens.at(1).erase(0, 1);
    const IpmiFruDevLocRec &frec = conn->get_fru_by_device_slave_address(std::stoi(s, nullptr, 10));

    s = tokens.at(2).erase(0, 1);
    const IpmiSensorRecComp &recComp = frec.get_sensor_by_sensor_number(std::stoi(s, nullptr, 10));

}

bool scheduleGet(const std::string& address, const std::function<void()>& cb, Provider::Entity& entity)
{
    ///auto conn = _getConnection( _parseLink(address).first );
    std::vector<std::string> tokens;

    ///TODO: Add throw. Strict parsing on number of tokens.
    parse_inout_str(tokens, address);
    
    /** First find the connection*/
    auto conn = _getConnection(tokens.at(0));
    if(!conn)
        return (!!conn);
    /**
    auto addr = _parseLink(address);
    auto conn = _getConnection(addr.first);
    if (!conn)
        return false;
    */

   /** Second find the FRU*/
    std::string s = tokens.at(1).erase(0, 1);
    const IpmiFruDevLocRec &frec = conn->get_fru_by_device_slave_address(std::stoi(s, nullptr, 10));

    s = tokens.at(2).erase(0, 1);
    const IpmiSensorRecComp &recComp = frec.get_sensor_by_sensor_number(std::stoi(s, nullptr, 10));

    ///return conn->schedule( Provider::Task(recComp, std::move(addr.second), cb, entity) );
    return conn->schedule( Provider::Task(recComp, s, cb, entity) );
}

}; // namespace dispatcher
