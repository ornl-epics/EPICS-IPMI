/** 
 * 
 * 
 * 
 */

#include "EntityAddrType.h"
#include <regex>
#include <iostream>

auto isSensor = [](EntityAddrType::Type t) {return t == EntityAddrType::Type::SENSOR;};
auto isPicmgLed = [](EntityAddrType::Type t) {return t == EntityAddrType::Type::PICMG_LED;};

constexpr const char* getEntAddrTypeStr(EntityAddrType::Type t) {
    switch (t)
    {
    case EntityAddrType::Type::SENSOR: return "Sensor";
    case EntityAddrType::Type::PICMG_LED: return "PICMG_LED";
    case EntityAddrType::Type::FRU: return "FRU";
    default:
        throw std::runtime_error("ERROR! getEntAddrTypeStr was called but passed an invalid enumeration \'" + std::to_string((int) t) + "\'");
    }
}

EntityAddrType::EntityAddrType(const std::string &recInOutString)
: mSensorEntityId(0)
, mSensorEntityInstance(0)
, mLogicalFruDeviceSlaveSddress(0)
, mLedId(0)
{

    parseInOutString(recInOutString);

}

EntityAddrType::~EntityAddrType() {
}

const std::string &EntityAddrType::getConnectionId() const {
    return this->mConnectionId;
}

EntityAddrType::Type EntityAddrType::getEntityAddressType() const {
    return this->mAddrType;
}

const std::string EntityAddrType::getEntityAddressTypeAsString() const {
    return getEntAddrTypeStr(this->mAddrType);
}

std::pair<uint8_t, bool> EntityAddrType::getSensorEntityId() const {
    return std::make_pair(this->mSensorEntityId, isSensor(this->mAddrType));
}
std::pair<uint8_t, bool> EntityAddrType::getSensorEntityInstance() const {
    return std::make_pair(this->mSensorEntityInstance, isSensor(this->mAddrType));
}
std::pair<const std::string &, bool> EntityAddrType::getSensorIdString() const {
    return std::make_pair(this->mSensorIdString, isSensor(this->mAddrType));
}

std::pair<uint8_t, bool> EntityAddrType::getPicmgLedFruDeviceSlaveSddress() const {
    return std::make_pair(this->mLogicalFruDeviceSlaveSddress, isPicmgLed(this->mAddrType));
}
std::pair<uint8_t, bool> EntityAddrType::getPicmgLedId() const {
    return std::make_pair(this->mLedId, isPicmgLed(this->mAddrType));
}

const std::string EntityAddrType::getSensorIdAsKey() const {
    return std::to_string(this->mSensorEntityId) + ":"
    + std::to_string(this->mSensorEntityInstance) + ":"
    + this->mSensorIdString;
}

void EntityAddrType::parseInOutString(const std::string &link) {

    /** 
     * We are looking for inout string signatures like the following:
     * Device-Name SENSOR Entity-Id:Entity-Instance 'Sensor-Id-String'
     * Example-1: @vt811 SENSOR 30:97 'CU TEMP1'
     * Example-2: @vt811 SENSOR 29:97 'FAN1'
     * Note: id-strings must be srurrounded in single quotes and can
     * contain spaces. This is slightly annoying,
     * because on a VadaTech device, while reading the SDR, one device
     * id-string come back with a space at the end of the string. EPICS
     * trims this off the inout string automatically. So to make it work
     * you will have to wrap your string with single quotes if they have
     * spaces at the end.
     * E.g., "@vt811 F5 SID 'VT BIOS POST '"
    */

    std::regex re_sensor ("([a-zA-Z0-9]+) ([sS][eE][nN][sS][oO][rR]) *([0-9]+) *: *([0-9]+) *\'(.*)\'");
    std::regex re_picmg_led("([a-zA-Z0-9]+) ([pP][iI][cC][mM][gG]_[lL][eE][dD]) *([0-9]+) *: *([0-9]+)");
    std::smatch re_m;

    /**
     * cid = connection ID
     * type = object type: sensor, led, etc.
     * et = entity type
     * ei = entity instance
     * sid = sensor id-string
    */

    /* Do we have SID? */
    if(std::regex_match(link, re_m, re_sensor)) {
        this->mConnectionId = re_m[1];
        this->mAddrType = Type::SENSOR; ///re_m[2];
        this->mSensorEntityId = (std::stoul(re_m[3]) & 0xFF);
        this->mSensorEntityInstance = (std::stoul(re_m[4]) & 0xFF);

        /** The quotes were only used to keep whitespace characters that
         *  are unknowingly at the end of the strings... Take them off
         *  now and preserve those whitespace characters.
        */
        for(auto &ch : re_m[5].str()) {
            if(ch != '\'')
                this->mSensorIdString.push_back(ch);
        }
    }
    else if(std::regex_match(link, re_m, re_picmg_led)) {
        this->mConnectionId = re_m[1];
        this->mAddrType = Type::PICMG_LED; ///re_m[2];
        this->mLogicalFruDeviceSlaveSddress = (std::stoul(re_m[3]) & 0xFF);
        this->mLedId = (std::stoul(re_m[4]) & 0xFF);
    }
    else {  /* Something is wrong. Throw now! */
        throw std::invalid_argument("Link field does not contain proper arguments. \'" + link + "\'");
    }
}

