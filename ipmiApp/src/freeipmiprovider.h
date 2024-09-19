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
#include "IpmiSdrManager.h"
#include "IpmiConnectionManager.h"

class FreeIpmiProvider : public Provider
{
    private:

        IpmiConnectionManager *mConnManager{nullptr};
        IpmiSdrManager *mSdrManager{nullptr};

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
        ///std::shared_ptr<IpmiFruDevLocRec> get_fru_by_device_slave_address(const uint8_t slave_address);
        static Entity read_sensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors,
            const std::shared_ptr<IpmiSensorRecComp> record);
        static Entity readPicmgLed(ipmi_ctx_t ipmi, const std::shared_ptr<PicmgLed> picmgLed);
        Entity getEntityValue(const std::shared_ptr<EntityAddrType> entAddrType) override;
        void write_oem_command(const std::shared_ptr<EntityAddrType> entAddrType, Entity &entity);
        void process() override;
        Entity getSensorReading(const std::shared_ptr<EntityAddrType> entAddrType);
        Entity getPicmgLedReading(const std::shared_ptr<EntityAddrType> entAddrType);
        static int compareSdrRecordKeys(ipmi_sdr_ctx_t sdr, const std::shared_ptr<IpmiSensorRecComp> record);
        bool is_valid_oem_cmd(const std::string &vendor_id, const std::string &command);
        
};
