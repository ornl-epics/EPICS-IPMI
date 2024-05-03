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
#include <tuple>

// EPICS records that we support
#include <aiRecord.h>
#include <stringinRecord.h>

namespace dispatcher {

static std::map<std::string, std::shared_ptr<FreeIpmiProvider>> g_connections; //!< Global map of connections.
static epicsMutex g_mutex; //!< Global mutex to protect g_connections.

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

std::shared_ptr<FreeIpmiProvider> checkEntityAddressType(const std::shared_ptr<EntityAddrType> entAddrType)
{
    /** First verify that the Entity Address and Type object is good to go.*/
    if(!entAddrType)
    {
        throw std::runtime_error("EntityAddrType object derrived from record link field is null.");
    }

    /** Second find/verify the connection. */
    auto conn = _getConnection(entAddrType->getConnectionId());
    if(!conn)
        throw std::invalid_argument("Link field can't find device \'@" + entAddrType->getConnectionId() + "\'"); 

    /** What type is the Entity Address? */
    const EntityAddrType::Type addressType = entAddrType->getEntityAddressType();

    switch (addressType)
    {
        /*
        * Note: opening and closing braces are needed for each case block that initializes
        * variables. Otherwise you have to move the variable declarations for all cases outside
        * of the switch block.
        */
        case EntityAddrType::Type::SENSOR:
        {
            std::shared_ptr<IpmiSensorRecComp> sp (nullptr);
            const std::string key = entAddrType->getSensorIdAsKey();
            sp = conn->findSensorByMapKey(key);
            if(!sp)
            {
                throw std::runtime_error("Could not find sensor in map by key \'" + key + "\'");
            }

            break;
        }
        case EntityAddrType::Type::PICMG_LED:
        {
            std::shared_ptr<PicmgLed> led = nullptr;
            std::pair<uint8_t, bool> fruId = entAddrType->getPicmgLedFruDeviceSlaveSddress();
            std::pair<uint8_t, bool> ledId = {0,false};
            if(fruId.second)
            {
                ledId = entAddrType->getPicmgLedId();
                if(ledId.second)
                {
                    led = conn->getPicmgLedByAddress(fruId.first, ledId.first);
                }
            }
            
            if(!led)
            {
                throw std::runtime_error("Could not find PICMG_LED by FRU-ID \'" + std::to_string(fruId.first) +
                "\' and LED-ID \'" + std::to_string(ledId.first) + "\' in record link field.");
            }
        
            break;
        }
        case EntityAddrType::Type::OEM_CMD:
        {
            auto [vid, vcmd, args] = entAddrType->get_oem_command();
            if(!conn->is_valid_oem_cmd(vid, vcmd))
            {
                throw std::runtime_error("Invalid OEM ID: \'" + vid +"\', and Command: \'" + vcmd + "\'\n");
            }
            break;
        }

        default:
            throw std::runtime_error("Could not find sensor in map by key \'" + entAddrType->getEntityAddressTypeAsString() + "\'");
            break;
    }

    return conn;
}

/** Just veriry that the link field is valid and that we can touch the
 *  objects defined.
**/
///void checkLink(const std::string& address) {
void checkLink(const std::shared_ptr<EntityAddrType> entAddrType) {
    
    /** First verify that the Entity Address and Type object is good to go.*/
    checkEntityAddressType(entAddrType);
}

///bool scheduleGet(const std::string& address, const std::function<void()>& cb, Provider::Entity& entity)
bool scheduleGet(const std::shared_ptr<EntityAddrType> entAddrType, const std::function<void()>& cb, Provider::Entity& entity)
{
    /** First verify that the Entity Address and Type object is good to go.*/
    auto conn = checkEntityAddressType(entAddrType);
    return conn->schedule( Provider::Task(entAddrType, cb, entity) );
    
}

bool scheduleWrite(const std::shared_ptr<EntityAddrType> entAddrType, const std::function<void()>& cb, Provider::Entity& entity)
{
    /** First verify that the Entity Address and Type object is good to go.*/
    auto conn = checkEntityAddressType(entAddrType);
    return conn->schedule( Provider::Task(entAddrType, cb, entity) );
}

}; // namespace dispatcher
