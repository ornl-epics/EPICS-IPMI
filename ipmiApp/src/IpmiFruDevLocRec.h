/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_IPMIFRUDEVLOCREC_H_
#define IPMIAPP_SRC_IPMIFRUDEVLOCREC_H_

#include "IpmiSensorRecComp.h"
#include "IpmiSensorRecFull.h"
#include <vector>
#include <memory>
#include <typeinfo>

class IpmiFruDevLocRec : public IpmiSdrRec
{
private:
    uint8_t device_access_address;
    uint8_t logical_fru_device_device_slave_address;
    uint8_t private_bus_id;
    uint8_t lun_for_master_write_read_fru_command;
    uint8_t logical_physical_fru_device;
    uint8_t channel_number;
    uint8_t fru_entity_id;
    uint8_t fru_entity_instance;
    std::vector<std::shared_ptr<IpmiSensorRecComp>> sensor_records;

public:
    IpmiFruDevLocRec(ipmi_sdr_ctx_t sdr, uint16_t recid, uint8_t rectype);
    ~IpmiFruDevLocRec();
    std::string report();
    template<typename T>
    void parse_sensors(std::vector<std::shared_ptr<T>> &sensor_list);
    uint8_t get_device_slave_address();
    std::shared_ptr<IpmiSensorRecComp> get_sensor_by_sensor_number(uint8_t number) const;
    std::shared_ptr<IpmiSensorRecComp> get_sensor_by_sensor_id_string(const std::string &) const;
    std::vector<std::shared_ptr<IpmiSensorRecComp>> &get_sensors();
};

#endif