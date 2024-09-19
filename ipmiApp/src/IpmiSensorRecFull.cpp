/**
 * 
 * 
 * 
 * 
*/

#include "IpmiSensorRecFull.h"

IpmiSensorRecFull::IpmiSensorRecFull(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type)
    :IpmiSensorRecComp(sdr, record_id, record_type)
{

}

IpmiSensorRecFull::~IpmiSensorRecFull()
{
}

