/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_PICMGLED_H_
#define IPMIAPP_SRC_PICMGLED_H_

#include <cstdint>
#include <string>

class PicmgLed {
private:
    uint8_t m_deviceAccessAddress;
    uint8_t m_channelNumber;
    uint8_t m_logicalFruDeviceDeviceSlaveAddress;
    uint8_t m_ledId;
    std::string m_color;
    
public:
    PicmgLed(uint8_t deviceAccessAddress, uint8_t channelNumber,
    uint8_t logicalFruDeviceDeviceSlaveAddress, uint8_t led_id, std::string led_color);
    ~PicmgLed();
    uint8_t getDeviceAccessAddress();
    uint8_t getChannelNumber();
    uint8_t getLogicalFruDeviceDeviceSlaveAddress();
    uint8_t getLedId();
    std::string getLedColor();
};




#endif /** IPMIAPP_SRC_PICMGLED_H_*/