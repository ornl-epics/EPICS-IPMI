/**
 * 
 * 
 * 
 * 
*/

#include "IpmiFruDevLocRec.h"
#include "IpmiSdrDefs.h"
#include <sstream>
#include <iostream>

IpmiFruDevLocRec::IpmiFruDevLocRec(ipmi_sdr_ctx_t sdr, uint16_t recid, uint8_t rectype)
    :IpmiSdrRec(recid, rectype)
{
    if(rectype != IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
	std::stringstream ss;
	ss << "ERROR: Invalid record type passed to FruDevLocatorRec::FruDevLocatorRec()." <<
	     "Expected \'" << sdr_type_itos_map[IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD] <<
	     "\' but received \'" << sdr_type_itos_map[rectype] << "\'" << std::endl;
        throw std::invalid_argument(ss.str());
    }

    int rv = (-1);
    rv = ipmi_sdr_parse_fru_entity_id_and_instance (sdr, NULL, 0, &this->fru_entity_id, &this->fru_entity_instance);

    rv = ipmi_sdr_parse_fru_device_locator_parameters (sdr, NULL, 0,
                    &this->device_access_address,
                    &this->logical_fru_device_device_slave_address,
                    &this->private_bus_id,
                    &this->lun_for_master_write_read_fru_command,
                    &this->logical_physical_fru_device,
                    &this->channel_number);

    char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
    rv = ipmi_sdr_parse_device_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);
    this->device_id_string = id_str;

}

IpmiFruDevLocRec::~IpmiFruDevLocRec()
{
}

std::string IpmiFruDevLocRec::report() {
    std::stringstream ss;
    ss << "FRU "<< (unsigned int) this->logical_fru_device_device_slave_address <<
        " \'" << this->device_id_string << "\' " << 
        "[" << (unsigned int) this->device_access_address <<
        ", " << (unsigned int) this->private_bus_id <<
        ", " << (unsigned int) this->lun_for_master_write_read_fru_command <<
        ", " << (unsigned int) this->logical_physical_fru_device <<
        ", " << (unsigned int) this->channel_number <<
        ", " << (unsigned int) this->fru_entity_id <<
        ", " << (unsigned int) this->fru_entity_instance << "]\n";

    if(this->sensor_records.size() > 0) {
        ss << "Sensors attched (" << (unsigned int) this->sensor_records.size() << ")\n";
        int count  = 1;
        for(auto &rec : this->sensor_records) {
            ss << "[" << count << "]\n" <<
                " * ID-String: \'" << rec->get_device_id_string() << "\'" <<
                ", \n * Record-Id: " << rec->get_record_id() <<
                ", \n * Sensor-Owner-Id-Type: \'" <<
                sdr_sensor_owner_id_type_itos_map[rec->get_sensor_owner_id_type()] <<
                "\' (" << (unsigned int) rec->get_sensor_owner_id_type() << ")" <<
                ", \n * Sensor-Owner-Id: " << (unsigned int) rec->get_sensor_owner_id() <<
                ", \n * Sensor-Owner-LUN: " << (unsigned int) rec->get_sensor_owner_lun() <<
                ", \n * Channel Number: " << (unsigned int) rec->get_channel_number() <<
                ", \n * Sensor Number: " << (unsigned int) rec->get_sensor_number() <<
                ", \n * Entity-Id: ";
                if(IPMI_ENTITY_ID_VALID(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_ids_pretty[rec->get_entity_id()] << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else if(IPMI_ENTITY_ID_IS_CHASSIS_SPECIFIC(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_id_chassis_specific << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else if(IPMI_ENTITY_ID_IS_BOARD_SET_SPECIFIC(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_id_board_set_specific << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else if(IPMI_ENTITY_ID_IS_OEM_SYSTEM_INTEGRATOR_DEFINED(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_id_oem_system_integrator << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else {
                    ss << "\'Unknow\' (" << (unsigned int) rec->get_entity_id() << ")";
                }
                ss << ", \n * Entity Instance: " << (unsigned int) rec->get_entity_instance() <<
                ", \n * Sensor Type: ";
                if(IPMI_SENSOR_TYPE_VALID(rec->get_sensor_type())) {
                    ss << "\'" << ipmi_sensor_types[rec->get_sensor_type()] << "\' (" <<
                        (unsigned int) rec->get_sensor_type() << ")\n";
                }
                else
                    ss << "\'OEM\' (" << (unsigned int) rec->get_sensor_type() << ")\n";
                ss << " * Event/Reading Type Code: \'" << get_sensor_event_reading_type_code(
                        (unsigned int) rec->get_event_reading_type_code()) <<
                    "\' (" << (unsigned int) rec->get_event_reading_type_code() << ")\n";
                ss << '\n';
            count++;
        }
    }
    return ss.str() + "\n";
}

template<typename T>
void IpmiFruDevLocRec::parse_sensors(std::vector<std::shared_ptr<T>> &sensor_list) {
    for(auto &rec: sensor_list) {
        if(rec->get_entity_id() == this->fru_entity_id) {
            if(rec->get_entity_instance() == this->fru_entity_instance)
                this->sensor_records.push_back(rec);
        }
        /** TODO: Make a note about how fans and cooling unit are missing from the device relative association record.*/
        else if(this->fru_entity_id == IPMI_ENTITY_ID_COOLING_UNIT_COOLING_DOMAIN && rec->get_sensor_type() == IPMI_SENSOR_TYPE_FAN) {
            if(rec->get_entity_instance() == this->fru_entity_instance)
                this->sensor_records.push_back(rec);
        }
    }
}

template void IpmiFruDevLocRec::parse_sensors(std::vector<std::shared_ptr<IpmiSensorRecComp>> &sensor_list);
template void IpmiFruDevLocRec::parse_sensors(std::vector<std::shared_ptr<IpmiSensorRecFull>> &sensor_list);

uint8_t IpmiFruDevLocRec::get_device_slave_address() {
    return this->logical_fru_device_device_slave_address;
}

const IpmiSensorRecComp *IpmiFruDevLocRec::get_sensor_by_sensor_number(uint8_t number) {
    for(auto &sens : this->sensor_records) {
        if(sens->get_sensor_number() == number) {
            return sens.get();
        }
    }
    std::stringstream ss;
    ss << "ERROR: Sensor number \'" << (unsigned) number << "\' not found!\n";
    throw std::invalid_argument(ss.str());
}

std::vector<std::shared_ptr<IpmiSensorRecComp>> &IpmiFruDevLocRec::get_sensors() {
    return this->sensor_records;
}