/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_IPMISDRREC_H_
#define IPMIAPP_SRC_IPMISDRREC_H_

#include <cstdint>
#include <string>

class IpmiSdrRec
{
protected:
    uint16_t record_id;
    uint8_t record_type;
    std::string device_id_string;
public:
    IpmiSdrRec(uint16_t record_id, uint8_t record_type);
    ~IpmiSdrRec();
    uint16_t get_record_id();
    uint8_t get_record_type();
    std::string get_device_id_string();

};

#endif