/**
 * 
 * 
 * 
 * 
*/

#include "IpmiSdrRec.h"

IpmiSdrRec::IpmiSdrRec(uint16_t record_id, uint8_t record_type)
    :record_id(record_id), record_type(record_type)
{

}

IpmiSdrRec::~IpmiSdrRec() {
}

uint16_t IpmiSdrRec::get_record_id() {
    return this->record_id;
}

uint8_t IpmiSdrRec::get_record_type() {
    return this->record_type;
}

std::string IpmiSdrRec::get_device_id_string() {
    return this->device_id_string;
}
