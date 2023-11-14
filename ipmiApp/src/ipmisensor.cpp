/* ipmisensor.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Mar 2019
 */

#include "freeipmiprovider.h"

#include <alarm.h> // from EPICS
#include <cmath>
#include "IpmiSensorRecComp.h"
#include <iostream>

FreeIpmiProvider::Entity FreeIpmiProvider::getSensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors, const SensorAddress& address)
{
    if (ipmi_sdr_cache_first(sdr) < 0)
        throw Provider::process_error("failed to rewind SDR cache - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    do {
        uint8_t recordType;
        if (ipmi_sdr_parse_record_id_and_type(sdr, NULL, 0, NULL, &recordType) < 0)
            continue;

        if (recordType != IPMI_SDR_FORMAT_FULL_SENSOR_RECORD && recordType != IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD)
            continue;

        SdrRecord record;
        record.size = ipmi_sdr_cache_record_read(sdr, record.data, IPMI_SDR_MAX_RECORD_LENGTH);
        if (record.size < 0)
            continue;

        try {
            SensorAddress tmp(sdr, record);
            if (!tmp.compare(address))
                continue;
        } catch (...) {
            continue;
        }

        return getSensor(sdr, sensors, record);
    } while (ipmi_sdr_cache_next(sdr) == 1);

    throw Provider::comm_error("sensor not found");
}

int FreeIpmiProvider::compareSdrRecordKeys(ipmi_sdr_ctx_t sdr, const std::shared_ptr<IpmiSensorRecComp> record) {

    /** Find the record in the SDR that we 'think' our record is pointing at.*/
    if(ipmi_sdr_cache_search_record_id (sdr, record->get_record_id()) < 0)
        return -1;

    /** Get the record-key from the SDR record and see if it matches the key of our record.
     * FYI See Section 33.5 "Reading the SDR Repository" of the IPMI Specification about record-keys
    */
    uint8_t _sensor_owner_id_type = 0;
    uint8_t _sensor_owner_id = 0;
    if(ipmi_sdr_parse_sensor_owner_id (sdr, NULL, 0, &_sensor_owner_id_type, &_sensor_owner_id) < 0)
        return -1;
    
    uint8_t _sensor_owner_lun = 0;
    uint8_t _channel_number = 0;
    if(ipmi_sdr_parse_sensor_owner_lun (sdr, NULL, 0, &_sensor_owner_lun, &_channel_number) < 0)
        return -1;

    uint8_t _sensor_number = 0;
    if(ipmi_sdr_parse_sensor_number (sdr, NULL, 0, &_sensor_number) < 0)
        return -1;
    
    if(record->get_sensor_owner_id() == _sensor_owner_id)
        if(record->get_sensor_owner_lun() == _sensor_owner_lun)
            if(record->get_channel_number() == _channel_number)
                if(record->get_sensor_number() == _sensor_number)
                    return 0;
    return -1;
}

FreeIpmiProvider::Entity FreeIpmiProvider::read_sensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors,
    const std::shared_ptr<IpmiSensorRecComp> record) {
    
    Entity entity;
    uint8_t sharedOffset = 0; // TODO: shared sensors support
    uint8_t readingRaw = 0;
    double* reading = nullptr;
    uint16_t eventMask = 0;
    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &data = record->get_record_data();

    int rv = ipmi_sensor_read(sensors, data.data, data.size, sharedOffset, &readingRaw, &reading, &eventMask);

    if(rv != 1) {

        int err_num = ipmi_sensor_read_ctx_errnum (sensors);
        std::string str_error = ipmi_sensor_read_ctx_strerror (err_num);
        std::string str_errmsg = ipmi_sensor_read_ctx_errormsg (sensors);

        /** Not sure if ipmi_sensor_read_ctx_strerror() and ipmi_sensor_read_ctx_errormsg()
         * always return the same messages.
         * So, use Lambda to concat strings if they are different...
         */
        auto getErrStr = [&str_error, &str_errmsg]() {
            if(str_error.compare(str_errmsg) != 0) {
                return "\'" + str_error + "\' Error Message: \'" + str_errmsg + "\'";
            }
            else
                return "\'" + str_error + "\'";
        };

        throw std::runtime_error (
            "Return Value: \'" + std::to_string(rv) + "\', " + "Error Code: \'" + std::to_string(err_num) + "\', Error String: " + getErrStr()
            );
    }

    /** Only threshold type sensors return a reading-value. The rest of the sensor types
     *  return the event-bit-mask only as the sensor reading-value; which represents an
     *  enumerated state. See section 42 in the IPMI specification.
     *  Note: Threshold sensors return two values: 1) the sensor-reading, 2) the even-mask.
     *  See Table 42-, Generic Event/Reading Type Codes for threshold events.
     *  TODO: Maybe handle threshold events somehow?
    */
    if(IPMI_EVENT_READING_TYPE_CODE_IS_THRESHOLD(record->get_event_reading_type_code())) {
        if(reading) {
            entity["VAL"] = std::round(*reading * 100.0) / 100.0;
            free(reading);
        }
        else
            entity["VAL"] = (double) eventMask;
    }
    else {
        entity["VAL"] = (double) eventMask;
    }

    return entity;
}

FreeIpmiProvider::Entity FreeIpmiProvider::getSensor(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors, const SdrRecord& record)
{
    Entity entity;

    // Determine entity type
    uint8_t recordType;
    if (ipmi_sdr_parse_record_id_and_type(sdr, record.data, record.size, NULL, &recordType) < 0) {
        throw std::runtime_error("Failed to parse SDR record type - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));
    }
    if (recordType != IPMI_SDR_FORMAT_FULL_SENSOR_RECORD && recordType != IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD)
        throw std::runtime_error("SDR record not a sensor, skipping");

    SensorAddress address(sdr, record);
    entity["INP"] = "SENSOR " + address.get();
    entity["EGU"] = getSensorUnits(sdr, record);
    entity["NAME"] = getSensorName(sdr, record);
    entity["DESC"] = getSensorDesc(sdr, record);

    int sharedOffset = 0; // TODO: shared sensors support
    uint8_t readingRaw = 0;
    double* reading = nullptr;
    uint16_t eventMask = 0;
    
    uint8_t event_type = 0;
    int rv = ipmi_sdr_parse_event_reading_type_code (sdr, record.data, record.size, &event_type);
    printf("-> rv: %i, event_type: %u\n", rv, event_type);
    const char * typstr = ipmi_generic_event_reading_type_code_state[0];
    printf("-> type str: %s, max: %i\n", typstr, ipmi_generic_event_reading_type_code_state_max_index);

    uint16_t record_id = 0;
    uint8_t record_type = 0;
    uint16_t record_count;
    ipmi_sdr_cache_record_count (sdr, &record_count);
    printf("-> record_count: %u\n", record_count);
    ///rv = ipmi_sdr_cache_first (sdr);
    
    rv = ipmi_sdr_parse_record_id_and_type (sdr,
                                       record.data,
                                       record.size,
                                       &record_id,
                                       &record_type);
    printf("-> rv: %i, record_id: %u, record_type: %u\n", rv, record_id, record_type);

    rv = ipmi_sdr_cache_first (sdr);

    rv = ipmi_sdr_parse_record_id_and_type (sdr,
                                       record.data,
                                       record.size,
                                       &record_id,
                                       &record_type);
    printf("-> rv: %i, record_id: %u, record_type: %u\n", rv, record_id, record_type);


    if (ipmi_sensor_read(sensors, record.data, record.size, sharedOffset, &readingRaw, &reading, &eventMask) <= 0) {
        entity["SEVR"] = epicsSevInvalid;
        switch (ipmi_sensor_read_ctx_errnum(sensors)) {
            case IPMI_SENSOR_READ_ERR_SENSOR_NON_ANALOG:
            case IPMI_SENSOR_READ_ERR_SENSOR_NON_LINEAR:
                entity["STAT"] = epicsAlarmCalc;
                break;
            case IPMI_SENSOR_READ_ERR_SENSOR_READING_CANNOT_BE_OBTAINED:
            case IPMI_SENSOR_READ_ERR_NODE_BUSY:
                entity["STAT"] = epicsAlarmComm;
                break;
            case IPMI_SENSOR_READ_ERR_SENSOR_IS_SYSTEM_SOFTWARE:
            case IPMI_SENSOR_READ_ERR_SENSOR_READING_UNAVAILABLE:
            case IPMI_SENSOR_READ_ERR_SENSOR_SCANNING_DISABLED:
            case IPMI_SENSOR_READ_ERR_SENSOR_NOT_OWNED_BY_BMC:
            case IPMI_SENSOR_READ_ERR_SENSOR_CANNOT_BE_BRIDGED:
            default:
                entity["STAT"] = epicsAlarmUDF;
                break;
        }

        LOG_DEBUG("Failed to read sensor value (%s) - %s", address.get().c_str(), ipmi_sensor_read_ctx_errormsg(sensors));
    } else {

		printf("-> Reading: %p, Raw: %d, mask: %u\n", reading, readingRaw, eventMask);

        uint8_t readingType;
        if (ipmi_sdr_parse_event_reading_type_code(sdr, record.data, record.size, &readingType) < 0) {
            LOG_DEBUG("Failed to read sensor value type (%s) - %s", address.get().c_str(), ipmi_sdr_ctx_errormsg(sdr));

        } else {

            if (reading) {
                // TODO: readingType == IPMI_EVENT_READING_TYPE_CODE_CLASS_GENERIC_DISCRETE ???
                entity["VAL"] = std::round(*reading * 100.0) / 100.0;
                entity["RVAL"] = readingRaw;
		printf("Reading: %f, Raw: %d\n", *reading, readingRaw);
            } else {
                entity["VAL"] = 0.0;
                entity["SEVR"] = epicsSevInvalid;
                entity["STAT"] = epicsAlarmCalc;
            }

            if (readingType == IPMI_EVENT_READING_TYPE_CODE_CLASS_THRESHOLD) {
                double* lowMinor;
                double* lowAlarm;
                double* lowCritical;
                double* highMinor;
                double* highAlarm;
                double* highCritical;
                if (ipmi_sdr_parse_thresholds(sdr, record.data, record.size,
                                              &lowMinor, &lowAlarm, &lowCritical,
                                              &highMinor, &highAlarm, &highCritical) >= 0) {
                    entity["LOW"] = *lowMinor;
                    entity["LOLO"] = *lowAlarm;
                    entity["HIGH"] = *highMinor;
                    entity["HIHI"] = *highAlarm;
                    free(lowMinor);
                    free(lowAlarm);
                    free(lowCritical);
                    free(highMinor);
                    free(highAlarm);
                    free(highCritical);
                }

                // TODO: create OUT records for driving thresholds
            }
        }
    }

    if (reading)
        free(reading);

    return entity;
}

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getSensors(ipmi_sdr_ctx_t sdr, ipmi_sensor_read_ctx_t sensors)
{
    std::vector<Entity> v;

    auto frus = getFruEntityNameAssoc(sdr);

    if (ipmi_sdr_cache_first(sdr) < 0)
        throw std::runtime_error("failed to rewind SDR cache - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    do {
        uint8_t recordType;
        if (ipmi_sdr_parse_record_id_and_type(sdr, NULL, 0, NULL, &recordType) < 0) {
            LOG_WARN("Failed to parse SDR record type - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
            continue;
        }

        if (recordType != IPMI_SDR_FORMAT_FULL_SENSOR_RECORD && recordType != IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD)
            continue;

        SdrRecord record;
        record.size = ipmi_sdr_cache_record_read(sdr, record.data, IPMI_SDR_MAX_RECORD_LENGTH);
        if (record.size < 0) {
            LOG_DEBUG("Failed to read SDR record - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
            continue;
        }

        // Need entity id for FRU association
        uint8_t entityId;
        uint8_t entityInstance;
        if (ipmi_sdr_parse_entity_id_instance_type(sdr, record.data, record.size, &entityId, &entityInstance, NULL) < 0) {
            LOG_DEBUG("Failed to read SDR entity info - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
            continue;
        }

        Entity sensor;
        try {
            sensor = getSensor(sdr, sensors, record);
        } catch (std::runtime_error e) {
            LOG_DEBUG(std::string(e.what()) + ", skipping");
            continue;
        }

        // Check if we can assign sensor to a device
        auto it = frus.find(std::make_pair(entityId, entityInstance));
        if (it != frus.end())
            sensor["NAME"] = it->second + ":" + sensor.getField<std::string>("NAME", "");

        v.emplace_back(std::move(sensor));
    } while (ipmi_sdr_cache_next(sdr) == 1);

    return v;
}

std::string FreeIpmiProvider::getSensorName(ipmi_sdr_ctx_t sdr, const SdrRecord& record)
{
    uint8_t sensorNum;
    if (ipmi_sdr_parse_sensor_number(sdr, record.data, record.size, &sensorNum) < 0)
        throw std::runtime_error("Failed to parse SDR record sensor number - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    char name[IPMI_SDR_MAX_SENSOR_NAME_LENGTH];
    int nameLen = IPMI_SDR_MAX_SENSOR_NAME_LENGTH;
    if (ipmi_sdr_parse_sensor_name(sdr, record.data, record.size, sensorNum, 0, name, nameLen) < 0)
        throw std::runtime_error("Failed to parse SDR record sensor name - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));
    name[IPMI_SDR_MAX_SENSOR_NAME_LENGTH-1] = 0;
    return name;
}

std::string FreeIpmiProvider::getSensorDesc(ipmi_sdr_ctx_t sdr, const SdrRecord& record)
{
    uint8_t sensorNum;
    if (ipmi_sdr_parse_sensor_number(sdr, record.data, record.size, &sensorNum) < 0) {
        throw std::runtime_error("Failed to parse SDR record sensor number - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));
    }
    char desc[IPMI_SDR_MAX_SENSOR_NAME_LENGTH];
    int descLen = IPMI_SDR_MAX_SENSOR_NAME_LENGTH;
    if (ipmi_sdr_parse_entity_sensor_name(sdr, record.data, record.size, sensorNum, 0, desc, descLen) < 0) {
        throw std::runtime_error("Failed to parse SDR record sensor long name - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));
    }
    desc[IPMI_SDR_MAX_SENSOR_NAME_LENGTH-1] = 0;
    return desc;
}

std::string FreeIpmiProvider::getSensorUnits(ipmi_sdr_ctx_t sdr, const SdrRecord& record)
{
    char units[1024] = {0};
    uint8_t percent = 0;
    uint8_t modifier = 0;
    uint8_t rate = 0;
    uint8_t baseType = 0;
    uint8_t modifierType = 0;
    if (ipmi_sdr_parse_sensor_units(sdr, record.data, record.size, &percent, &modifier, &rate, &baseType, &modifierType) < 0)
        return "";

    if (baseType == 0 || ipmi_sensor_units_string(percent, modifier, rate, baseType, modifierType, units, sizeof(units)-1, 1) <= 0)
        return "";

    return units;
}

/*
 * ===== SensorAddress implementation =====
 *
 * EPICS record link specification for SENSOR entities
 * @ipmi <conn> SENSOR <owner>:<LUN>:<channel>:<sensor num>
 * Example:
 * @ipmi IPMI1 SENSOR 22:0:1:97
 */
FreeIpmiProvider::SensorAddress::SensorAddress(const std::string& address)
{
    auto tokens = common::split(address, ':');
    if (tokens.size() != 4)
        throw Provider::syntax_error("Invalid sensor address");

    try {
        ownerId   = std::stoi(tokens[0]) & 0xFF;
        ownerLun  = std::stoi(tokens[1]) & 0xFF;
        channel   = std::stoi(tokens[2]) & 0xFF;
        sensorNum = std::stoi(tokens[3]) & 0xFF;
    } catch (std::invalid_argument) {
        throw Provider::syntax_error("Invalid sensor address");
    }
}

FreeIpmiProvider::SensorAddress::SensorAddress(ipmi_sdr_ctx_t sdr, const SdrRecord& record)
{
    uint8_t tmp;

    if (ipmi_sdr_parse_sensor_owner_id(sdr, record.data, record.size, &tmp, &ownerId) < 0)
        throw Provider::process_error("Failed to parse sensor owner ID from SDR record");

    if (ipmi_sdr_parse_sensor_owner_lun(sdr, record.data, record.size, &ownerLun, &channel) < 0)
        throw Provider::process_error("Failed to parse sensor owner LUN from SDR record");

    if (ipmi_sdr_parse_sensor_number(sdr, record.data, record.size, &sensorNum) < 0)
        throw Provider::process_error("Failed to parse sensor number from SDR record");
}

std::string FreeIpmiProvider::SensorAddress::get() const
{
    return std::to_string(ownerId) + ":" + std::to_string(ownerLun) + ":" + std::to_string(channel) + ":" + std::to_string(sensorNum);
}

bool FreeIpmiProvider::SensorAddress::compare(const FreeIpmiProvider::SensorAddress& other)
{
    if (other.ownerId != ownerId)
        return false;
    if (other.ownerLun != ownerLun)
        return false;
    if (other.channel != channel)
        return false;
    if (other.sensorNum != sensorNum)
        return false;
    return true;
}
