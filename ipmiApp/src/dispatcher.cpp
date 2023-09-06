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
#include <regex>

// EPICS records that we support
#include <aiRecord.h>
#include <stringinRecord.h>

namespace dispatcher {

static std::map<std::string, std::shared_ptr<FreeIpmiProvider>> g_connections; //!< Global map of connections.
static epicsMutex g_mutex; //!< Global mutex to protect g_connections.

/// @brief Split string and place tokens into map
/// @param argmap 
/// @param link Field of EPICS record.
static void parse_inout_str(std::map<std::string, std::string> &argMap, const std::string &link) {

    /** 
     * We are looking for inout string signatures like the following:
     * Device-Name SENSOR Entity-Id:Entity-Instance 'Sensor-Id-String'
     * Example-1: @vt811 SENSOR 30:97 'CU TEMP1'
     * Example-2: @vt811 SENSOR 29:97 'FAN1'
     * Note: id-strings must be srurrounded in single quotes and can
     * contain spaces. This is slightly annoying,
     * because on a VadaTech device, while reading the SDR, one device
     * id-string come back with a space at the end of the string. EPICS
     * trims this off the inout string automatically. So to make it work
     * you will have to wrap your string with single quotes if they have
     * spaces at the end.
     * E.g., "@vt811 F5 SID 'VT BIOS POST '"
    */

    std::regex re_sensor ("([a-zA-Z0-9]+) ([sS][eE][nN][sS][oO][rR]) *([0-9]+) *: *([0-9]+) *\'(.*)\'");
    std::smatch re_m;

    /**
     * cid = connection ID
     * type = object type: sensor, led, etc.
     * et = entity type
     * ei = entity instance
     * sid = sensor id-string
    */

    /* Do we have SID? */
    if(std::regex_match(link, re_m, re_sensor)) {
        argMap["cid"] = re_m[1];
        argMap["type"] = re_m[2];
        argMap["et"] = re_m[3];
        argMap["ei"] = re_m[4];

        /** The quotes were only used to keep whitespace characters that
         *  are unknowingly at the end of the strings... Take them off
         *  now and preserve those whitespace characters.
        */
        for(auto &ch : re_m[5].str()) {
            if(ch != '\'')
                argMap["sid"].push_back(ch);
        }
    }
    else {  /* Something is wrong. Throw now! */
        throw std::invalid_argument("Link field does not contain proper arguments. \'" + link + "\'");
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

/** Just veriry that the link field is valid and that we can touch the
 *  objects defined.
*/
void checkLink(const std::string& address) {
    
    std::map<std::string, std::string> argMap;

    parse_inout_str(argMap, address);

    /** First find the connection*/
    auto conn = _getConnection(argMap["cid"]);
    if(!conn)
        throw std::invalid_argument("Link field can't find device \'@" + argMap["cid"] + "\'");

    std::shared_ptr<IpmiSensorRecComp> sp (nullptr);

    std::string key = argMap["et"] + ":" + argMap["ei"] + ":";

    /** Find the sensor in the map by the key created: entity-type:entity-instance:sensor-id-string*/
    if(argMap.find("sid") != argMap.end()) {
        key += argMap["sid"];
        sp = conn->findSensorByMapKey(key);
    }
    else
        throw std::runtime_error("Sensor parameter invalid in link field....");
    
    if(!sp) {
        throw std::runtime_error("Could not find sensor in map by key \'" + key + "\'");
    }
}

bool scheduleGet(const std::string& address, const std::function<void()>& cb, Provider::Entity& entity)
{
    std::map<std::string, std::string> argMap;

    ///TODO: Add throw. Strict parsing on number of tokens.
    parse_inout_str(argMap, address);
    
    /** First find the connection*/
    auto conn = _getConnection(argMap["cid"]);
    if(!conn)
        return (!!conn);

   /** Second find the FRU*/
    std::string s;
    std::shared_ptr<IpmiSensorRecComp> sp (nullptr);

    std::string key = argMap["et"] + ":" + argMap["ei"] + ":";

    /** Find the sensor in the map by the key created: entity-type:entity-instance:sensor-id-string*/
    if(argMap.find("sid") != argMap.end()) {
        key += argMap["sid"];
        sp = conn->findSensorByMapKey(key);
    }
    else    /* We should have already thrown at this point*/
        throw std::runtime_error("Sensor parameter invalid in link field....");
    
    if(!sp) {
        throw std::runtime_error("Could not find sensor in map by key \'" + key + "\'");
    }

    return conn->schedule( Provider::Task(sp, cb, entity) );
}

}; // namespace dispatcher
