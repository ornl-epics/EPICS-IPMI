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

public:
    IpmiSensorRecComp(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type);
    ~IpmiSensorRecComp();
    uint8_t get_sensor_owner_id_type();
    uint8_t get_sensor_owner_id();
    uint8_t get_sensor_owner_lun();
    uint8_t get_channel_number();
    uint8_t get_sensor_number();
    uint8_t get_entity_id();
    uint8_t get_entity_instance();
    uint8_t get_sensor_type();
    uint8_t get_event_reading_type_code();
};

#endif
