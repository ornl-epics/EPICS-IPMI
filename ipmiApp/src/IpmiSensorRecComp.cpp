/**
 * 
 * 
 * 
 * 
*/

#include "IpmiSensorRecComp.h"
#include <sstream>
#include "IpmiSdrDefs.h"


IpmiSensorRecComp::IpmiSensorRecComp(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type)
    :IpmiSdrRec(record_id, record_type)
{
    int rv = (-1);

    this->record_data.size = ipmi_sdr_cache_record_read(sdr, this->record_data.data, IPMI_SDR_MAX_RECORD_LENGTH);

    rv = ipmi_sdr_parse_sensor_owner_id (sdr, NULL, 0, &sensor_owner_id_type, &sensor_owner_id);

    rv = ipmi_sdr_parse_sensor_owner_lun (sdr, NULL, 0, &sensor_owner_lun, &channel_number);

    rv = ipmi_sdr_parse_sensor_number (sdr, NULL, 0, &sensor_number);

    rv = ipmi_sdr_parse_entity_id_instance_type (sdr, NULL, 0, &entity_id, &entity_instance, &entity_instance_type);

    rv = ipmi_sdr_parse_sensor_type (sdr, NULL, 0, &sensor_type);

    rv = ipmi_sdr_parse_event_reading_type_code (sdr, NULL, 0, &event_reading_type_code);

    char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
    rv = ipmi_sdr_parse_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);
    this->device_id_string = id_str;

    rv = ipmi_sdr_parse_sensor_units (sdr, NULL, 0, &sensor_units_percentage, &sensor_units_modifier,
    &sensor_units_rate, &sensor_base_unit_type, &sensor_modifier_unit_type);

    /**printf("%s, [%u, %u, %u, %u, %u]\n", id_str, sensor_units_percentage, sensor_units_modifier,
    sensor_units_rate, sensor_base_unit_type, sensor_modifier_unit_type);*/

}

IpmiSensorRecComp::~IpmiSensorRecComp()
{
}

uint8_t IpmiSensorRecComp::get_sensor_owner_id_type() const {
    return this->sensor_owner_id_type;
}

uint8_t IpmiSensorRecComp::get_sensor_owner_id() const {
    return this->sensor_owner_id;
}

uint8_t IpmiSensorRecComp::get_sensor_owner_lun() const {
    return this->sensor_owner_lun;
}

uint8_t IpmiSensorRecComp::get_channel_number() const {
    return this->channel_number;
}

uint8_t IpmiSensorRecComp::get_sensor_number() const {
    return this->sensor_number;
}

uint8_t IpmiSensorRecComp::get_entity_id() const {
    return this->entity_id;
}

uint8_t IpmiSensorRecComp::get_entity_instance() const {
    return this->entity_instance;
}

uint8_t IpmiSensorRecComp::get_sensor_type() const {
    return this->sensor_type;
}

uint8_t IpmiSensorRecComp::get_event_reading_type_code() const {
    return this->event_reading_type_code;
}

uint8_t IpmiSensorRecComp::get_sensor_base_unit_type() const {
    return this->sensor_base_unit_type;
}

std::string IpmiSensorRecComp::get_sensor_base_unit_type_str() const {
    if(IPMI_SENSOR_UNIT_VALID(this->sensor_base_unit_type)) {
        return std::string(ipmi_sensor_units_abbreviated[this->sensor_base_unit_type]);
    }
    return std::string("Invalid-Type");
}

const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &IpmiSensorRecComp::get_record_data() const {
    return this->record_data;
}

std::string IpmiSensorRecComp::to_string() const {

    std::stringstream ss;
    
    ss << " * ID-String: \'" << this->get_device_id_string() << "\'" <<
        ", \n * Record-Id: " << this->get_record_id() <<
        ", \n * Sensor-Owner-Id-Type: \'" <<
        sdr_sensor_owner_id_type_itos_map[this->get_sensor_owner_id_type()] <<
        "\' (" << (unsigned int) this->get_sensor_owner_id_type() << ")" <<
        ", \n * Sensor-Owner-Id: " << (unsigned int) this->get_sensor_owner_id() <<
        ", \n * Sensor-Owner-LUN: " << (unsigned int) this->get_sensor_owner_lun() <<
        ", \n * Channel Number: " << (unsigned int) this->get_channel_number() <<
        ", \n * Sensor Number: " << (unsigned int) this->get_sensor_number() <<
        ", \n * Entity-Id: ";
        if(IPMI_ENTITY_ID_VALID(this->get_entity_id())) {
            ss << "\'" << ipmi_entity_ids_pretty[this->get_entity_id()] << "\'" <<
                " (" << (unsigned int) this->get_entity_id() << ")";
        }
        else if(IPMI_ENTITY_ID_IS_CHASSIS_SPECIFIC(this->get_entity_id())) {
            ss << "\'" << ipmi_entity_id_chassis_specific << "\'" <<
                " (" << (unsigned int) this->get_entity_id() << ")";
        }
        else if(IPMI_ENTITY_ID_IS_BOARD_SET_SPECIFIC(this->get_entity_id())) {
            ss << "\'" << ipmi_entity_id_board_set_specific << "\'" <<
                " (" << (unsigned int) this->get_entity_id() << ")";
        }
        else if(IPMI_ENTITY_ID_IS_OEM_SYSTEM_INTEGRATOR_DEFINED(this->get_entity_id())) {
            ss << "\'" << ipmi_entity_id_oem_system_integrator << "\'" <<
                " (" << (unsigned int) this->get_entity_id() << ")";
        }
        else {
            ss << "\'Unknow\' (" << (unsigned int) this->get_entity_id() << ")";
        }
        ss << ", \n * Entity Instance: " << (unsigned int) this->get_entity_instance() <<
        ", \n * Sensor Type: ";
        if(IPMI_SENSOR_TYPE_VALID(this->get_sensor_type())) {
            ss << "\'" << ipmi_sensor_types[this->get_sensor_type()] << "\' (" <<
                (unsigned int) this->get_sensor_type() << ")\n";
        }
        else
            ss << "\'OEM\' (" << (unsigned int) this->get_sensor_type() << ")\n";
        ss << " * Event/Reading Type Code: \'" << get_sensor_event_reading_type_code(
                (unsigned int) this->get_event_reading_type_code()) <<
            "\' (" << (unsigned int) this->get_event_reading_type_code() << ")\n";
        ss << '\n';

    return ss.str();
}

std::string IpmiSensorRecComp::get_entity_id_string() const
{
    std::stringstream ss;
    ss << (unsigned) entity_id << ":" << (unsigned) entity_instance <<
    " \'" << device_id_string << "\'" << std::endl;
    return ss.str();
}

