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
#include "PicmgLed.h"

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
    void IpmiFruDevLocRec::getPicmgStatusLeds(ipmi_ctx_t ipmi);
    std::vector<std::shared_ptr<IpmiSensorRecComp>> sensor_records;
    std::vector<std::shared_ptr<PicmgLed>> m_StatusLeds;
    static const std::vector<std::string> picmgLedExclusionList;
    static bool isInExclusionList(const std::string &name);

public:
    IpmiFruDevLocRec(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr, uint16_t recid, uint8_t rectype);
    ~IpmiFruDevLocRec();
    std::string report();
    template<typename T>
    void parseAssociations(std::vector<std::shared_ptr<T>> &sensor_list);
    uint8_t get_device_access_address() const;
    uint8_t get_device_slave_address();
    uint8_t get_logical_physical_fru_device() const;
    uint8_t get_channel_number() const;
    std::shared_ptr<IpmiSensorRecComp> get_sensor_by_sensor_number(uint8_t number) const;
    std::shared_ptr<IpmiSensorRecComp> get_sensor_by_sensor_id_string(const std::string &) const;
    std::vector<std::shared_ptr<IpmiSensorRecComp>> &get_sensors();
    std::vector<std::shared_ptr<PicmgLed>> getStatusLeds();
    std::shared_ptr<PicmgLed> getStatusLedById(uint8_t led_id);
    
};

#endif