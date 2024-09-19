/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_IPMISENSORRECCOMP_H_
#define IPMIAPP_SRC_IPMISENSORRECCOMP_H_

#include "IpmiSdrRec.h"
#include <freeipmi/freeipmi.h>

class IpmiSensorRecComp : public IpmiSdrRec
{
private:
    uint8_t sensor_owner_id_type;
    uint8_t sensor_owner_id;
    uint8_t sensor_owner_lun;
    uint8_t channel_number;
    uint8_t sensor_number;
    uint8_t entity_id;
    uint8_t entity_instance;
    uint8_t entity_instance_type;
    uint8_t sensor_type;
    uint8_t event_reading_type_code;
    uint8_t sensor_units_percentage;
    uint8_t sensor_units_modifier;
    uint8_t sensor_units_rate;
    uint8_t sensor_base_unit_type;
    uint8_t sensor_modifier_unit_type;

public:
    IpmiSensorRecComp(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type);
    ~IpmiSensorRecComp();
    uint8_t get_sensor_owner_id_type() const;
    uint8_t get_sensor_owner_id() const;
    uint8_t get_sensor_owner_lun() const;
    uint8_t get_channel_number() const;
    uint8_t get_sensor_number() const;
    uint8_t get_entity_id() const;
    uint8_t get_entity_instance() const;
    uint8_t get_sensor_type() const;
    uint8_t get_event_reading_type_code() const;
    uint8_t get_sensor_base_unit_type() const;
    std::string get_sensor_base_unit_type_str() const;
    std::string get_entity_id_string() const;

    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &get_record_data() const;

    std::string to_string() const;

};

#endif
