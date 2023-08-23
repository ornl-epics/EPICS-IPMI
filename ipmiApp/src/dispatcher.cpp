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
enum LinkOptions {
    NOT_DEFINED,
    SID,    /** Sensor ID String */
    SN      /** Sensor Number */

};

static std::map<std::string, LinkOptions> s_mapLinkOptions = {
    {"null", LinkOptions::NOT_DEFINED},
    {"SID", LinkOptions::SID},
    {"SN", LinkOptions::SN}
    };

/// @brief Split string on whitespaces and place tokens into map
/// @param argmap 
/// @param link 
static void parse_inout_str(std::map<std::string, std::string> &argMap, const std::string &link) {

    /** 
     * We are looking for inout string signatures like the following:
     * <device> F<FRU number> SN <sensor-number>
     * <device> F<FRU number> SID <sensor-id-string>
    */

    std::vector<std::string> tokens;
    std::stringstream ss(link);
    std::string tok;
    
    while(ss >> tok) {
        tokens.push_back(tok);
    }

    /* So, we must have at least 3 tokens to get this party started.*/
    if(tokens.size() < 3) {
        throw std::invalid_argument("Link field does not have enough parameters. \'" + link + "\'");
    }

    /* Connection ID is first. */
    argMap["cid"] = tokens.at(0);

    /* FRU ID is next. */
    argMap["fru"] = tokens.at(1).erase(0,1);

    /* Last is the SENSOR identifier... for now... Later will add something for LEDS
     * Currently there are two different options for identifying sensors:
     * (1) We can identify them by sensor number. E.g., SN 33.
     * (2) We can identify them by sensor id string. E.g., SID VT AMC523 12V
     *  Option (2) can contain spaces. It is annoying but that is the way the vendors
     *  do it.
     */
    switch (s_mapLinkOptions[tokens.at(2)]) {

        /*(1)*/
        case LinkOptions::SN:
            argMap["sn"] = tokens.at(3);
            break;

        /*(2)*/
        case LinkOptions::SID:
            std::vector<std::string>::iterator itr = tokens.begin();
            std::advance(itr, 3);
            while(itr != tokens.end()) {
                argMap["sid"] += *(itr++);
                if(itr != tokens.end()) {
                    argMap["sid"] += " ";
                }
            }
            break;
        
        /* Neither options were found; let's throw! */
        default:
            throw std::invalid_argument("Link field does not contain options after FRU. \'" + link + "\'");
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
    
    /** Second find the FRU*/
    std::shared_ptr<IpmiFruDevLocRec> frec = conn->get_fru_by_device_slave_address(std::stoi(argMap["fru"], nullptr, 10));

    /** Are we identifying this record by sensor number or id string?*/
    if(argMap.find("sn") != argMap.end()) {
        std::shared_ptr<IpmiSensorRecComp> sp = frec->get_sensor_by_sensor_number(std::stoi(argMap["sn"], nullptr, 10));
    }
    else if(argMap.find("sid") != argMap.end()) {
        std::shared_ptr<IpmiSensorRecComp> sp = frec->get_sensor_by_sensor_id_string(argMap["sid"]);
    }
    else    /* We should have already thrown at this point*/
        throw std::runtime_error("Sensor parameter invalid in link field....");
    

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
    std::shared_ptr<IpmiFruDevLocRec> frec = conn->get_fru_by_device_slave_address(std::stoi(argMap["fru"], nullptr, 10));
    std::shared_ptr<IpmiSensorRecComp> sp;

    /** Are we identifying this record by sensor number or id string?*/
    if(argMap.find("sn") != argMap.end()) {
        sp = frec->get_sensor_by_sensor_number(std::stoi(argMap["sn"], nullptr, 10));
    }
    else if(argMap.find("sid") != argMap.end()) {
        sp = frec->get_sensor_by_sensor_id_string(argMap["sid"]);
    }
    else    /* We should have already thrown at this point*/
        throw std::runtime_error("Sensor parameter invalid in link field....");

    return conn->schedule( Provider::Task(sp, s, cb, entity) );
}

}; // namespace dispatcher
