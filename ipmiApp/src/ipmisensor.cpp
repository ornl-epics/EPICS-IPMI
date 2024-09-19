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
#include "IpmiException.h"
#include <iostream>

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
        
        throw IpmiException(err_num, std::move(getErrStr()));
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


