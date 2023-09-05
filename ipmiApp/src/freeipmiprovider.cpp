/* freeipmiprovider.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#include "freeipmiprovider.h"
#include <iostream>
#include <sstream>

FreeIpmiProvider::FreeIpmiProvider(const std::string& conn_id, const std::string& hostname,
                                   const std::string& username, const std::string& password,
                                   const std::string& authtype, const std::string& protocol,
                                   const std::string& privlevel)
    : Provider(conn_id)
    , m_hostname(hostname)
    , m_username(username)
    , m_password(password)
    , m_protocol(protocol)
    , m_nextReconnect{epicsTime::getCurrent()}
    , m_ConnectionId(conn_id)
{
    if (authtype == "none" || username.empty())
        m_authType = IPMI_AUTHENTICATION_TYPE_NONE;
    else if (authtype == "plain" || authtype == "straight_password_key")
        m_authType = IPMI_AUTHENTICATION_TYPE_STRAIGHT_PASSWORD_KEY;
    else if (authtype == "md2")
        m_authType = IPMI_AUTHENTICATION_TYPE_MD2;
    else if (authtype == "md5")
        m_authType = IPMI_AUTHENTICATION_TYPE_MD5;
    else
        throw std::runtime_error("invalid authentication type (choose from none,plain,md2,md5)");

    if (privlevel == "admin")
        m_privLevel = IPMI_PRIVILEGE_LEVEL_ADMIN;
    else if (privlevel == "operator")
        m_privLevel = IPMI_PRIVILEGE_LEVEL_OPERATOR;
    else if (privlevel == "user")
        m_privLevel = IPMI_PRIVILEGE_LEVEL_USER;
    else
        throw std::runtime_error("invalid privilege level (choose from user,operator,admin)");

    // TODO: parametrize
    m_sdrCachePath = "/tmp/ipmi_sdr_" + conn_id + ".cache";

    // TODO: automatic connection management
    initContexts();
    openSdrCache();
    readSdrCache();
    
}

FreeIpmiProvider::~FreeIpmiProvider()
{
    if (stopThread() == false)
        LOG_WARN("Processing thread did not stop");

    if (m_ctx.ipmi) {
        ipmi_ctx_close(m_ctx.ipmi);
        ipmi_ctx_destroy(m_ctx.ipmi);
    }
    if (m_ctx.sdr) {
        ipmi_sdr_ctx_destroy(m_ctx.sdr);
    }
    if (m_ctx.sensors) {
        ipmi_sensor_read_ctx_destroy(m_ctx.sensors);
    }
    if (m_ctx.fru) {
        ipmi_fru_ctx_destroy(m_ctx.fru);
    }
}

std::shared_ptr<IpmiFruDevLocRec> FreeIpmiProvider::get_fru_by_device_slave_address(const uint8_t slave_address) {
    for(auto &fru : this->fruDevLocRecList) {
        if(fru->get_device_slave_address() == slave_address) {
            return fru;
        }
    }
    std::stringstream ss;
    ss << "FRU address \'" << (unsigned) slave_address << "\' not found!\n";
    throw std::invalid_argument(ss.str());
}

FreeIpmiProvider::Entity FreeIpmiProvider::get_entity_value(std::shared_ptr<IpmiSensorRecComp> sdrRec) {

    /** First check to see if this sensor matches the SDR. The SDR can change underneith us.
     * See Section 33.5 "Reading the SDR Repository" of the IPMI Specification.
    */
    
    if(compareSdrRecordKeys(m_ctx.sdr, *sdrRec) != 0) {
        ///TODO: Dump the current IpmiSensorRecComp objects and reread the SDR
        std::stringstream ss;
        std::map<std::shared_ptr<IpmiSensorRecComp>, uint16_t>::iterator itr;
        itr = this->m_SensToFruMap.find(sdrRec);
        ss << "Connection-ID: \'" << this->m_ConnectionId << "\', ";
        ss << "Hostname: \'" << this->m_hostname << "\', ";
        if(itr != this->m_SensToFruMap.end()) {
            ss << "SDR key for FRU: \'" << itr->second << "\' and Sensor-ID: \'";
        }
        else
            ss << "SDR key for FRU: \'FRU-Id is Unavailable\' and Sensor-ID: \'";
        ss << sdrRec.get()->get_device_id_string() << "\' does not match key in repository." << std::endl;
        throw std::runtime_error(ss.str());
    }
    Entity entity = read_sensor(m_ctx.sdr, m_ctx.sensors, *sdrRec);
    return entity;
}

void FreeIpmiProvider::initContexts() {
    destroyContexts();
    initIpmiContext();
    connect();
    initSdrContext();
    /** Sensor context will fail if connection hasn't been opened yet.*/
    initSensorsContext();
}

void FreeIpmiProvider::destroyContexts() {

    if (m_ctx.sensors)
        ipmi_sensor_read_ctx_destroy(m_ctx.sensors);

    if (m_ctx.fru)
        ipmi_fru_ctx_destroy(m_ctx.fru);

    if (m_ctx.sdr) {
        ipmi_sdr_ctx_destroy(m_ctx.sdr);
    }

    if (m_ctx.ipmi) {
        ipmi_ctx_close(m_ctx.ipmi);
        ipmi_ctx_destroy(m_ctx.ipmi);
    }

}

void FreeIpmiProvider::initIpmiContext() {

    m_ctx.ipmi = ipmi_ctx_create();

    if (!m_ctx.ipmi)
        throw std::runtime_error("can't create IPMI context");
}

void FreeIpmiProvider::initSdrContext() {

    m_ctx.sdr = ipmi_sdr_ctx_create();

    if (!m_ctx.sdr)
        throw std::runtime_error("can't create IPMI SDR context");
}

void FreeIpmiProvider::initSensorsContext() {
    m_ctx.sensors = ipmi_sensor_read_ctx_create(m_ctx.ipmi);
    if (!m_ctx.sensors)
        throw std::runtime_error("can't create IPMI sensor context");
    
    int sensorReadFlags = 0;
    sensorReadFlags |= IPMI_SENSOR_READ_FLAGS_BRIDGE_SENSORS;
    /* Don't error out, if this fails we can still continue */
    if (ipmi_sensor_read_ctx_set_flags(m_ctx.sensors, sensorReadFlags) < 0)
        LOG_WARN("can't set sensor read flags - %s", ipmi_sensor_read_ctx_errormsg(m_ctx.sensors));
}

void FreeIpmiProvider::initFruContext() {
    m_ctx.fru = ipmi_fru_ctx_create(m_ctx.ipmi);
    if (!m_ctx.fru)
        throw std::runtime_error("can't create IPMI FRU context");
}

void FreeIpmiProvider::connect()
{
    const char* username_ = (m_username.empty() ? nullptr : m_username.c_str());
    const char* password_ = (m_password.empty() ? nullptr : m_password.c_str());

    int connected;

    if (m_protocol == "lan_2.0") {
        connected = ipmi_ctx_open_outofband_2_0(
                        m_ctx.ipmi, m_hostname.c_str(), username_, password_,
                        m_k_g, m_k_g_len, m_privLevel, m_cipherSuiteId,
                        m_sessionTimeout, m_retransmissionTimeout, m_workaroundFlags, m_flags);
    } else {
        connected = ipmi_ctx_open_outofband(
                        m_ctx.ipmi, m_hostname.c_str(), username_, password_,
                        m_authType, m_privLevel,
                        m_sessionTimeout, m_retransmissionTimeout, m_workaroundFlags, m_flags);

    }
    if (connected < 0)
        throw std::runtime_error("can't connect - " + std::string(ipmi_ctx_errormsg(m_ctx.ipmi)));

    std::cout << "Connected successfully to \'" << this->m_ConnectionId << "\' at \'"
    << this->m_hostname << "\'" << std::endl;
    m_connected = true;
}

void FreeIpmiProvider::openSdrCache()
{
    if (ipmi_sdr_cache_open(m_ctx.sdr, m_ctx.ipmi, m_sdrCachePath.c_str()) < 0) {
        switch (ipmi_sdr_ctx_errnum(m_ctx.sdr)) {
        case IPMI_SDR_ERR_CACHE_OUT_OF_DATE:
        case IPMI_SDR_ERR_CACHE_INVALID:
            LOG_INFO("deleting out of date or invalid SDR cache file " + m_sdrCachePath);
            (void)ipmi_sdr_cache_delete(m_ctx.sdr, m_sdrCachePath.c_str());
            // fall thru
        case IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST:
            LOG_INFO("creating new SDR cache file " + m_sdrCachePath);
            (void)ipmi_sdr_cache_create(m_ctx.sdr, m_ctx.ipmi, m_sdrCachePath.c_str(), IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT, nullptr, nullptr);
            break;
        default:
            throw std::runtime_error("can't open SDR cache - " + std::string(ipmi_ctx_errormsg(m_ctx.ipmi)));
        }

        if (ipmi_sdr_cache_open(m_ctx.sdr, m_ctx.ipmi, m_sdrCachePath.c_str()) < 0)
            throw std::runtime_error("can't open SDR cache - " + std::string(ipmi_ctx_errormsg(m_ctx.ipmi)));
    }

}

void FreeIpmiProvider::readSdrCache() {

    /* Get the SDR version. */
    if(ipmi_sdr_cache_sdr_version (m_ctx.sdr, &this->m_SdrVersion) < 0)
        throw std::runtime_error("Error! Could not read SDR cache version.");

    /* Get the SDR most recent addition timestamp */
    if(ipmi_sdr_cache_most_recent_addition_timestamp (m_ctx.sdr, &this->m_SdrAdditionTimestamp) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent addition timestamp.");

    /* Get the SDR most recent erase timestamp */
    if(ipmi_sdr_cache_most_recent_erase_timestamp (m_ctx.sdr, &this->m_SdrEraseTimestamp) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent erase timestamp.");

    /* Get the SDR record count */
    if(ipmi_sdr_cache_record_count (m_ctx.sdr, &this->m_SdrRecordCount) < 0)
        throw std::runtime_error("Error! Could not read SDR cache record count.");
    
    std::cout << this->m_ConnectionId << ":" << this->m_hostname << " SDR Info {" << std::endl;
    std::cout << " * SDR Record Count: " << m_SdrRecordCount << "," << std::endl;
    std::cout << " * SDR Version: " << (unsigned) this->m_SdrVersion << "," << std::endl;
    std::cout << " * SDR Addition Timestamp: " << (unsigned) this->m_SdrAdditionTimestamp << "," << std::endl;
    std::cout << " * SDR Erase Timestamp: " << (unsigned) this->m_SdrEraseTimestamp << std::endl;
    std::cout << "}" << std::endl;

    uint16_t record_id = 0;
    uint8_t record_type = 0;

    /* Iterate through all of the records in the SDR and create sensor lists as needed. */
    for(int i = 0; i < this->m_SdrRecordCount; i++, ipmi_sdr_cache_next(m_ctx.sdr)) {
        if(ipmi_sdr_parse_record_id_and_type (m_ctx.sdr, nullptr, 0, &record_id, &record_type)<0)
            throw std::runtime_error("Could not read record ID and record type in SDR.");

        /* Add this record type to the list. When constructor is called we read more sensor data */
        insertRecord(m_ctx.sdr, record_id, record_type);
    }

    std::copy(sensRecFullList.begin(), sensRecFullList.end(), std::back_inserter(orphandList));
    std::copy(sensRecCompactList.begin(), sensRecCompactList.end(), std::back_inserter(orphandList));

    /* Iterate through all of the FRU Device Locator Records and attach sensors to their parent FRUs */
    for(auto &fdlr: this->fruDevLocRecList) {
        fdlr.get()->parseAssociations(orphandList);
    }

    /* Create a reverse lookup map so the sensor can find its parent FRU. */
    for(auto &fdlr : this->fruDevLocRecList) {
        std::vector<std::shared_ptr<IpmiSensorRecComp>> &sensrs = fdlr.get()->get_sensors();
        for(auto &sensr : sensrs) {
            this->m_SensToFruMap.insert({sensr, fdlr.get()->get_device_slave_address()});
        }
    }

}

void FreeIpmiProvider::insertRecord(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type) {

    if(record_type == IPMI_SDR_FORMAT_FULL_SENSOR_RECORD) {
        std::shared_ptr<IpmiSensorRecFull> p;

        /** Add sensor to list */
        p = std::make_shared<IpmiSensorRecFull>(m_ctx.sdr, record_id, record_type);
        this->sensRecFullList.push_back(p);

        /** Add sensor to map with entity-id:entity-instance:sensor-number as key */
        insertIntoEntityMap(p);
    }

    /* Add this record type to the list. Compact and Full are so close to the same.
    * When constructor is called we read more sensor data
    */
    if(record_type == IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD) {
        std::shared_ptr<IpmiSensorRecComp> p;

        /** Add sensor to list */
        p = std::make_shared<IpmiSensorRecComp>(m_ctx.sdr, record_id, record_type);
        this->sensRecCompactList.push_back(p);

        /** Add sensor to map with entity-id:entity-instance:sensor-number as key */
        insertIntoEntityMap(p);
    }

    if(record_type == IPMI_SDR_FORMAT_EVENT_ONLY_RECORD) {
        ///TODO:
        ;
    }

    if(record_type == IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD) {
        ///TODO:
        ;
    }

    /* Add this record type to the list. This record type is useful for grouping sensor based on
        * FRU to sensor relationships. 
        */
    if(record_type == IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
        this->fruDevLocRecList.push_back(std::make_shared<IpmiFruDevLocRec>(m_ctx.sdr, record_id, record_type));
    }

    if(record_type == IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD) {
        ///TODO:
        ;
    }
}

void FreeIpmiProvider::insertIntoEntityMap(std::shared_ptr<IpmiSensorRecComp> p) {
    
    /** Add sensor to map with entity-id:entity-instance:sensor-number as key */
    std::pair<std::map<std::string, std::shared_ptr<IpmiSensorRecComp>>::iterator,bool> rv;

    /** Create the key */
    std::string snKey = std::to_string(p->get_entity_id()) + ":" + 
    std::to_string(p->get_entity_instance()) + ":" + std::to_string(p->get_sensor_number());

    rv = this->m_SnEntityMap.insert({snKey, p});
    if(rv.second == false) {
        throw std::runtime_error("ERROR! Could not insert record into map. Duplicate Keys Exists: " + snKey);
    }

    /** Create the key */
    std::string sidKey = std::to_string(p->get_entity_id()) + ":" + 
    std::to_string(p->get_entity_instance()) + ":" + p->get_device_id_string();

    rv = this->m_SidEntityMap.insert({sidKey, p});
    if(rv.second == false) {
        throw std::runtime_error("ERROR! Could not insert record into map. Duplicate Keys Exists: " + sidKey);
    }
}

std::shared_ptr<IpmiSensorRecComp> FreeIpmiProvider::findSensorByMapKey(std::string key) {

    std::map<std::string, std::shared_ptr<IpmiSensorRecComp>>::iterator itr;

    ///for(auto &x : m_SidEntityMap) {
        ///std::cout << "sid: " << x.first << std::endl;
    ///}

    itr = this->m_SidEntityMap.find(key);
    if(itr != this->m_SidEntityMap.end()) {
        return itr->second;
    }

    itr = this->m_SnEntityMap.find(key);
    if(itr != this->m_SnEntityMap.end()) {
        return itr->second;
    }

    return nullptr;
}

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getSensors()
{
    common::ScopedLock lock(m_apiMutex);
    if (!m_connected)
        connect();
    return getSensors(m_ctx.sdr, m_ctx.sensors);
}

FreeIpmiProvider::Entity FreeIpmiProvider::getEntity(const std::string& address)
{
    common::ScopedLock lock(m_apiMutex);
    if (!m_connected) {
        if ((epicsTime::getCurrent() - m_nextReconnect) < 1.0)
            throw std::runtime_error("Not connected");

        m_nextReconnect = epicsTime::getCurrent() + 1.0;
        connect();
    }

    // First token in address is the entity type, like 'SENSOR', 'FRU' etc.
    // Rest is type specific
    auto tokens = common::split(address, ' ', 1);
    if (tokens.size() != 2)
        throw Provider::syntax_error("Invalid address '" + address + "'");

    auto type = std::move(tokens.at(0));
    auto rest = std::move(tokens.at(1));

    if (type == "SENSOR") {
        return getSensor(m_ctx.sdr, m_ctx.sensors, SensorAddress(rest));
    } else if (type == "FRU") {
        return getFru(m_ctx.ipmi, m_ctx.sdr, m_ctx.fru, FruAddress(rest));
    } else if (type == "PICMG_LED") {
        return getPicmgLed(m_ctx.ipmi, PicmgLedAddress(rest));
    } else {
        throw Provider::syntax_error("Invalid address '" + address + "'");
    }
}

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getFrus()
{
    common::ScopedLock lock(m_apiMutex);
    if (!m_connected)
        connect();
    return getFrus(m_ctx.ipmi, m_ctx.sdr, m_ctx.fru);
}

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getPicmgLeds()
{
    common::ScopedLock lock(m_apiMutex);
    if (!m_connected)
        connect();
    return getPicmgLeds(m_ctx.ipmi, m_ctx.sdr);
}

FreeIpmiProvider::IpmbBridgeScoped::IpmbBridgeScoped(ipmi_ctx_t ipmi_, uint8_t slaveAddress, uint8_t channel)
    : ipmi(ipmi_)
{
    uint8_t channel_;
    uint8_t slaveAddress_;
    if (ipmi_ctx_get_target(ipmi, &channel_, &slaveAddress_) < 0) {
        //throw Provider::process_error("Failed to get IPMI target address - " + std::string(ipmi_ctx_errormsg(ipmi)));
        return;
    }

    if (channel_ == channel && slaveAddress_ == slaveAddress)
        return;

    if (ipmi_ctx_set_target(ipmi, &channel, &slaveAddress) < 0) {
        //throw Provider::process_error("Failed to set IPMI target address - " + std::string(ipmi_ctx_errormsg(ipmi)));
        return;
    }

    bridged = true;
}

FreeIpmiProvider::IpmbBridgeScoped::~IpmbBridgeScoped()
{
    close();
}

void FreeIpmiProvider::IpmbBridgeScoped::close()
{
    if (bridged) {
        if (ipmi_ctx_set_target(ipmi, NULL, NULL) < 0) {
            //throw Provider::process_error("Failed to set IPMI target address - " + std::string(ipmi_ctx_errormsg(ipmi)));
        } else {
            bridged = false;
        }
    }
}
