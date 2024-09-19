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
#include "common.h"
#include <freeipmi/freeipmi.h>

class IpmiSdrRec
{
protected:
    uint16_t record_id;
    uint8_t record_type;
    std::string device_id_string;
    common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> record_data;
public:
    IpmiSdrRec(uint16_t record_id, uint8_t record_type);
    ~IpmiSdrRec();
    uint16_t get_record_id() const;
    uint8_t get_record_type() const;
    std::string get_device_id_string() const;
    double scale_threshold(ipmi_sdr_ctx_t sdr, uint64_t rawVal) const;
    double scale_hysteresis(ipmi_sdr_ctx_t sdr, uint64_t rawVal) const;

};

#endif