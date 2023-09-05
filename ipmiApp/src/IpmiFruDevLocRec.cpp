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
        int count = 1;
        for(auto &rec : this->sensor_records) {
            ss << "[" << (unsigned int) this->logical_fru_device_device_slave_address << ":" << count << "]\n";
            ss << rec->to_string();
            count++;
        }
    }
    return ss.str() + "\n";
}

template<typename T>
void IpmiFruDevLocRec::parseAssociations(std::vector<std::shared_ptr<T>> &sensor_list) {

    auto itr = sensor_list.begin();
    while(itr != sensor_list.end()) {
        auto pos = *itr;
        if(itr->get()->get_entity_id() == this->fru_entity_id && itr->get()->get_entity_instance() == this->fru_entity_instance) {
            this->sensor_records.push_back(*itr);
            itr = sensor_list.erase(itr);
        }
        /** TODO: Make a note about how fans and cooling unit are missing from the device relative association record.*/
        else if(this->fru_entity_id == IPMI_ENTITY_ID_COOLING_UNIT_COOLING_DOMAIN && itr->get()->get_sensor_type() == IPMI_SENSOR_TYPE_FAN) {
            if(itr->get()->get_entity_instance() == this->fru_entity_instance)
                this->sensor_records.push_back(*itr);
                itr = sensor_list.erase(itr);
        }

        if(*itr == pos && itr != sensor_list.end()) {
            itr++;
        }
    }
}

template void IpmiFruDevLocRec::parseAssociations(std::vector<std::shared_ptr<IpmiSensorRecComp>> &sensor_list);
template void IpmiFruDevLocRec::parseAssociations(std::vector<std::shared_ptr<IpmiSensorRecFull>> &sensor_list);

uint8_t IpmiFruDevLocRec::get_device_slave_address() {
    return this->logical_fru_device_device_slave_address;
}

std::shared_ptr<IpmiSensorRecComp> IpmiFruDevLocRec::get_sensor_by_sensor_number(uint8_t number) const {
    for(auto &sens : this->sensor_records) {
        if(sens->get_sensor_number() == number) {
            return sens;
        }
    }
    std::stringstream ss;
    ss << "Sensor number \'" << (unsigned) number << "\' not found!\n";
    throw std::invalid_argument(ss.str());
}

std::shared_ptr<IpmiSensorRecComp> IpmiFruDevLocRec::get_sensor_by_sensor_id_string(const std::string &idStr) const {
    for(auto &sens : this->sensor_records) {
        if(sens->get_device_id_string() == idStr) {
            return sens;
        }
    }
    std::stringstream ss;
    ss << "Sensor ID-String \'" << idStr << "\' not found!\n";
    throw std::invalid_argument(ss.str());
}

std::vector<std::shared_ptr<IpmiSensorRecComp>> &IpmiFruDevLocRec::get_sensors() {
    return this->sensor_records;
}