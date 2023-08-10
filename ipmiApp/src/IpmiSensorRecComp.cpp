/**
 * 
 * 
 * 
 * 
*/

#include "IpmiSensorRecComp.h"

IpmiSensorRecComp::IpmiSensorRecComp(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type)
    :IpmiSdrRec(record_id, record_type)
{
    int rv = (-1);

    rv = ipmi_sdr_parse_sensor_owner_id (sdr, NULL, 0, &sensor_owner_id_type, &sensor_owner_id);

    rv = ipmi_sdr_parse_sensor_owner_lun (sdr, NULL, 0, &sensor_owner_lun, &channel_number);

    rv = ipmi_sdr_parse_sensor_number (sdr, NULL, 0, &sensor_number);

    rv = ipmi_sdr_parse_entity_id_instance_type (sdr, NULL, 0, &entity_id, &entity_instance, &entity_instance_type);

    rv = ipmi_sdr_parse_sensor_type (sdr, NULL, 0, &sensor_type);

    rv = ipmi_sdr_parse_event_reading_type_code (sdr, NULL, 0, &event_reading_type_code);

    char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
    rv = ipmi_sdr_parse_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);
    this->device_id_string = id_str;

}

IpmiSensorRecComp::~IpmiSensorRecComp()
{
}

uint8_t IpmiSensorRecComp::get_sensor_owner_id_type() {
    return this->sensor_owner_id_type;
}

uint8_t IpmiSensorRecComp::get_sensor_owner_id() {
    return this->sensor_owner_id;
}

uint8_t IpmiSensorRecComp::get_sensor_owner_lun() {
    return this->sensor_owner_lun;
}

uint8_t IpmiSensorRecComp::get_channel_number() {
    return this->channel_number;
}

uint8_t IpmiSensorRecComp::get_sensor_number() {
    return this->sensor_number;
}

uint8_t IpmiSensorRecComp::get_entity_id() {
    return this->entity_id;
}

uint8_t IpmiSensorRecComp::get_entity_instance() {
    return this->entity_instance;
}

uint8_t IpmiSensorRecComp::get_sensor_type() {
    return this->sensor_type;
}

uint8_t IpmiSensorRecComp::get_event_reading_type_code() {
    return this->event_reading_type_code;
}

