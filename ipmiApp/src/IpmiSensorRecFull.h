/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_IPMISENSORRECFULL_H_
#define IPMIAPP_SRC_IPMISENSORRECFULL_H_

#include "IpmiSensorRecComp.h"

class IpmiSensorRecFull : public IpmiSensorRecComp
{
private:
    

public:
    IpmiSensorRecFull(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type);
    ~IpmiSensorRecFull();
};

#endif