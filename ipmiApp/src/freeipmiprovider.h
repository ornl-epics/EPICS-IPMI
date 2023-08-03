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

#include <freeipmi/freeipmi.h>

class FreeIpmiProvider : public Provider
{
    private:
        struct {
            ipmi_ctx_t ipmi{nullptr};
            ipmi_sdr_ctx_t sdr{nullptr};
            ipmi_sensor_read_ctx_t sensors{nullptr};
            ipmi_fru_ctx_t fru{nullptr};
        } m_ctx;

        int m_sessionTimeout{IPMI_SESSION_TIMEOUT_DEFAULT};
        int m_retransmissionTimeout{IPMI_RETRANSMISSION_TIMEOUT_DEFAULT};
        int m_cipherSuiteId{3};
        int m_k_g_len{0};
        unsigned char* m_k_g{nullptr};
        int m_workaroundFlags{1};
        int m_flags{IPMI_FLAGS_DEFAULT};
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

        struct SensorAddress {
            uint8_t ownerId{0};
            uint8_t ownerLun{0};
            uint8_t channel;
            uint8_t sensorNum{0};
            SensorAddress() {};
            SensorAddress(const std::string& address);
            SensorAddress(ipmi_sdr_ctx_t sdr, const SdrRecord& record);
            std::string get() const;
            bool compare(const SensorAddress& other);
        };

        struct FruAddress {
            // Supports only FRUs that can be accessed via read/write command to mgmt ctrl
            uint8_t deviceAddr;
            uint8_t fruId;
            uint8_t lun;
            uint8_t channel;
            std::string area;
            std::string subarea;

            FruAddress() {};
            FruAddress(const std::string& address);
            FruAddress(ipmi_sdr_ctx_t sdr, const SdrRecord& record);
            std::string get() const;
            bool compare(const FruAddress& other, bool checkArea=true, bool checkSubarea=true) const;
        };

        struct PicmgLedAddress {
            uint8_t deviceAddr;
            uint8_t channel;
            uint8_t fruId;
            uint8_t ledId; // ids assigned according to PICMG 3.0 Table 3-28

            PicmgLedAddress() {};
            PicmgLedAddress(const std::string& address);
            PicmgLedAddress(uint8_t deviceAddr, uint8_t channel, uint8_t fruId, uint8_t ledId);
            std::string get() const;
            bool compare(const PicmgLedAddress& other) const;
        };

        /**
         * @brief Establishes and managed IPMB bridge if necessary depending on the address.
         */
        class IpmbBridgeScoped {
        private:
                bool bridged{false};
                ipmi_ctx_t ipmi{nullptr};
            public:
                IpmbBridgeScoped(ipmi_ctx_t ipmi, uint8_t slaveAddress, uint8_t channel);
                ~IpmbBridgeScoped();
                void close();
        };

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

        /**
         * @brief Scans for all sensors in the connected IPMI device.
         * @return A list of sensors
         */
        std::vector<Entity> getSensors() override;

        /**
         * @brief Scans for all FRUs in the connected IPMI device.
         * @return A list of FRUs
         */
        std::vector<Entity> getFrus() override;

        /**
         * @brief Scans for all PICMG LEDs in the connected IPMI device.
         * @return A list of LEDs
         */
        std::vector<Entity> getPicmgLeds() override;

    private:
        /**
         * @brief Tries to (re)connect to IPMI device
         * @return true on success
         */
        void connect();

        /**
         * @brief Opens or creates SDR cache, needs file on disk.
         * @return
         */
        void openSdrCache();

        /**
         * @brief Based on the address, determine IPMI entity type and retrieve its current value.
         * @param address FreeIPMI implementation specific address
         * @return current value
         */
        Entity getEntity(const std::string& address) override;

        // *** SENSOR functinality implemented in ipmisensor.cpp file ***

        static Entity getSensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors, const SensorAddress& address);
        static Entity getSensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors, const SdrRecord& record);
        static std::vector<Entity> getSensors(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors);
        static std::string getSensorName(ipmi_sdr_ctx_t sdr, const SdrRecord& record);
        static std::string getSensorDesc(ipmi_sdr_ctx_t sdr, const SdrRecord& record);
        static std::string getSensorUnits(ipmi_sdr_ctx_t sdr, const SdrRecord& record);

        // *** FRU functionality implemented in ipmifru.cpp file ***

        static Entity getFru(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr, ipmi_fru_ctx_t fru, const FruAddress& address);
        static std::vector<Entity> getFrus(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr, ipmi_fru_ctx_t fru);
        static std::map<std::pair<uint8_t,uint8_t>,std::string> getFruEntityNameAssoc(ipmi_sdr_ctx_t sdr);
        static std::vector<Entity> getFruAreas(ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& tmpl);
        static std::string getFruField(ipmi_fru_ctx_t fru, const ipmi_fru_field_t& field, uint8_t languageCode);
        static std::string getFruName(ipmi_sdr_ctx_t sdr, const SdrRecord& record);
        static std::string getFruDesc(ipmi_sdr_ctx_t sdr, const SdrRecord& record);
        static bool isFruLogical(ipmi_sdr_ctx_t sdr, const SdrRecord& record);

        // These are called from getFru() and are high-level functions that in turn call getFru*Subarea() functions
        static std::vector<Entity> getFruChassis(ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& deviceName, const FruArea& fruArea);
        static std::vector<Entity> getFruBoard(  ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& deviceName, const FruArea& fruArea);
        static std::vector<Entity> getFruProduct(ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& deviceName, const FruArea& fruArea);

        // Functions that parse individual FRU subareas and return only VAL field in the entity
        static std::string getFruChassisSubarea(ipmi_fru_ctx_t fru, const FruArea& area, const std::string& subarea);
        static std::string getFruBoardSubarea(  ipmi_fru_ctx_t fru, const FruArea& area, const std::string& subarea);
        static std::string getFruProductSubarea(ipmi_fru_ctx_t fru, const FruArea& area, const std::string& subarea);

        // *** PICMG functionality implemented in ipmipicmg.cpp file ***
        std::vector<FreeIpmiProvider::Entity> getPicmgLeds(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr);
        std::vector<FreeIpmiProvider::Entity> getPicmgLeds(ipmi_ctx_t ipmi, const FruAddress& address, const std::string& namePrefix);
        FreeIpmiProvider::Entity getPicmgLedFull(ipmi_ctx_t ipmi, const PicmgLedAddress& address, const std::string& namePrefix);
        FreeIpmiProvider::Entity getPicmgLed(ipmi_ctx_t ipmi, const PicmgLedAddress& address);
};
