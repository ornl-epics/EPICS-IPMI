/* freeipmiprovider.h
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#pragma once

#include "common.h"
#include "provider.h"

#include <epicsTime.h>

#include <string>
#include <vector>
#include <iostream>

#include <freeipmi/freeipmi.h>
#include "IpmiSensorRecFull.h"
#include "IpmiFruDevLocRec.h"

class FreeIpmiProvider : public Provider
{
    private:
        struct {
            ipmi_ctx_t ipmi{nullptr};
            ipmi_sdr_ctx_t sdr{nullptr};
            ipmi_sensor_read_ctx_t sensors{nullptr};
            ipmi_fru_ctx_t fru{nullptr};
        } m_ctx;

        std::vector<std::shared_ptr<IpmiSensorRecFull>> sensRecFullList;
        std::vector<std::shared_ptr<IpmiSensorRecComp>> sensRecCompactList;
        std::vector<std::shared_ptr<IpmiFruDevLocRec>> fruDevLocRecList;
        std::vector<std::shared_ptr<IpmiSensorRecComp>> orphandList;

        std::map<std::string, std::shared_ptr<IpmiSensorRecComp>> m_SidEntityMap;
        std::map<std::string, std::shared_ptr<IpmiSensorRecComp>> m_SnEntityMap;

        std::map<std::shared_ptr<IpmiSensorRecComp>, uint16_t> m_SensToFruMap;

        bool m_sdrCacheIsOpen{false};
        uint8_t m_SdrVersion;
        uint32_t m_SdrAdditionTimestamp;
        uint32_t m_SdrEraseTimestamp;
        uint16_t m_SdrRecordCount;

        int m_sessionTimeout{IPMI_SESSION_TIMEOUT_DEFAULT};
        int m_retransmissionTimeout{IPMI_RETRANSMISSION_TIMEOUT_DEFAULT};
        int m_cipherSuiteId{3};
        int m_k_g_len{0};
        unsigned char* m_k_g{nullptr};
        int m_workaroundFlags{1};
        int m_flags{IPMI_FLAGS_DEFAULT};
        std::string m_ConnectionId;
        std::string m_hostname;
        std::string m_username;
        std::string m_password;
        int m_authType;
        int m_privLevel;
        std::string m_protocol;
        std::string m_sdrCachePath;
        epicsMutex m_apiMutex;          //!< Serializes all external interfaces
        bool m_connected{false};
        epicsTime m_nextReconnect;

        typedef common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> SdrRecord;
        typedef common::buffer<uint8_t, IPMI_FRU_AREA_SIZE_MAX+1> FruArea;

    public:

        /**
         * @brief Instantiate new FreeIpmiProvider object and connect it to IPMI device
         * @param conn_id
         * @param hostname
         * @param username
         * @param password
         * @param authtype
         * @param protocol
         * @param privlevel
         * @exception std::runtime_error when can't connect
         */
        FreeIpmiProvider(const std::string& conn_id, const std::string& hostname,
                         const std::string& username, const std::string& password,
                         const std::string& authtype, const std::string& protocol,
                         const std::string& privlevel);

        /**
         * @brief Destructor
         */
        ~FreeIpmiProvider();

        std::shared_ptr<IpmiSensorRecComp> findSensorByMapKey(std::string key);
        std::shared_ptr<PicmgLed> getPicmgLedByAddress(uint8_t fru_id, uint8_t led_id);
        std::shared_ptr<IpmiFruDevLocRec> get_fru_by_device_slave_address(const uint8_t slave_address);
        static Entity read_sensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors,
            const std::shared_ptr<IpmiSensorRecComp> record);
        static Entity readPicmgLed(ipmi_ctx_t ipmi, const std::shared_ptr<PicmgLed> picmgLed);
        Entity getEntityValue(const std::shared_ptr<EntityAddrType> entAddrType) override;
        Entity getSensorReading(const std::shared_ptr<EntityAddrType> entAddrType);
        Entity getPicmgLedReading(const std::shared_ptr<EntityAddrType> entAddrType);
        static int compareSdrRecordKeys(ipmi_sdr_ctx_t sdr, const std::shared_ptr<IpmiSensorRecComp> record);

    private:

        void destroyContexts();
        void initIpmiContext();
        void initSdrContext();
        void initSensorsContext();
        void initFruContext();
        void insertRecord(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type);
        void insertIntoEntityMap(std::shared_ptr<IpmiSensorRecComp> p);

        /**
         * @brief Tries to (re)connect to IPMI device
         * @return true on success
         */
        void connect();
        void disconnect();

        /**
         * @brief Opens or creates SDR cache, needs file on disk.
         * @return
         */
        void openSdrCache();

        void readSdrCache();

        // *** SENSOR functinality implemented in ipmisensor.cpp file ***

        // *** FRU functionality implemented in ipmifru.cpp file ***

        // These are called from getFru() and are high-level functions that in turn call getFru*Subarea() functions

        // Functions that parse individual FRU subareas and return only VAL field in the entity

        // *** PICMG functionality implemented in ipmipicmg.cpp file ***
        
};
