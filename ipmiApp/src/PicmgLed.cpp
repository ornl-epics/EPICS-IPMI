/**
 * 
 * 
 * 
 * 
*/

#include "PicmgLed.h"


PicmgLed::PicmgLed(uint8_t deviceAccessAddress, uint8_t channelNumber,
    uint8_t logicalFruDeviceDeviceSlaveAddress, uint8_t led_id, std::string led_color)
: m_deviceAccessAddress(deviceAccessAddress),
m_channelNumber(channelNumber),
m_logicalFruDeviceDeviceSlaveAddress(logicalFruDeviceDeviceSlaveAddress),
m_ledId(led_id),
m_color(led_color)
{

}

PicmgLed::~PicmgLed()
{
}

uint8_t PicmgLed::getDeviceAccessAddress() {
    return this->m_deviceAccessAddress;
}

uint8_t PicmgLed::getChannelNumber() {
    return this->m_channelNumber;
}

uint8_t PicmgLed::getLogicalFruDeviceDeviceSlaveAddress() {
    return this->m_logicalFruDeviceDeviceSlaveAddress;
}

uint8_t PicmgLed::getLedId() {
    return this->m_ledId;
}

std::string PicmgLed::getLedColor() {
    return this->m_color;
}
