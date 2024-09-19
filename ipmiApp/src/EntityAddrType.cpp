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
auto isOemCmd = [](EntityAddrType::Type t) {return t == EntityAddrType::Type::OEM_CMD;};

constexpr const char* getEntAddrTypeStr(EntityAddrType::Type t)
{
    switch (t)
    {
        case EntityAddrType::Type::SENSOR: return "Sensor";
        case EntityAddrType::Type::PICMG_LED: return "PICMG_LED";
        case EntityAddrType::Type::FRU: return "FRU";
        case EntityAddrType::Type::OEM_CMD: return "OEM_CMD";
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

EntityAddrType::~EntityAddrType()
{
}

const std::string &EntityAddrType::getConnectionId() const
{
    return mConnectionId;
}

EntityAddrType::Type EntityAddrType::getEntityAddressType() const
{
    return mAddrType;
}

const std::string EntityAddrType::getEntityAddressTypeAsString() const
{
    return getEntAddrTypeStr(mAddrType);
}

std::pair<uint8_t, bool> EntityAddrType::getSensorEntityId() const
{
    return std::make_pair(mSensorEntityId, isSensor(mAddrType));
}

std::pair<uint8_t, bool> EntityAddrType::getSensorEntityInstance() const
{
    return std::make_pair(mSensorEntityInstance, isSensor(mAddrType));
}

std::pair<const std::string &, bool> EntityAddrType::getSensorIdString() const
{
    return std::make_pair(mSensorIdString, isSensor(mAddrType));
}

std::pair<uint8_t, bool> EntityAddrType::getPicmgLedFruDeviceSlaveSddress() const
{
    return std::make_pair(mLogicalFruDeviceSlaveSddress, isPicmgLed(mAddrType));
}

std::pair<uint8_t, bool> EntityAddrType::getPicmgLedId() const
{
    return std::make_pair(mLedId, isPicmgLed(mAddrType));
}

const std::string EntityAddrType::getSensorIdAsKey() const
{
    return std::to_string(mSensorEntityId) + ":"
    + std::to_string(mSensorEntityInstance) + ":"
    + mSensorIdString;
}

std::tuple<const std::string, const std::string, const std::vector<std::string>> EntityAddrType::get_oem_command() const
{
    return std::make_tuple(mOemCmd.vendorId, mOemCmd.commandId, mOemCmd.commandArgs);
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
    //std::regex re_oem_cmd ("^([a-zA-Z0-9]+)\\s+OEM_CMD\\s+([a-zA-Z0-9]+)\\s+([a-zA-Z0-9]+)\\s*$");
    std::regex re_oem_cmd ("^([a-zA-Z0-9]+)\\s+OEM_CMD\\s+([a-zA-Z0-9]+)\\s+([a-zA-Z0-9-]+)(?: ([a-zA-Z0-9][a-zA-Z0-9-_\\s]+))?$");
    std::smatch re_m;

    /**
     * cid = connection ID
     * type = object type: sensor, led, etc.
     * et = entity type
     * ei = entity instance
     * sid = sensor id-string
    */

    /* Do we have SID? */
    if(std::regex_match(link, re_m, re_sensor))
    {
        mConnectionId = re_m[1];
        mAddrType = Type::SENSOR; ///re_m[2];
        mSensorEntityId = (std::stoul(re_m[3]) & 0xFF);
        mSensorEntityInstance = (std::stoul(re_m[4]) & 0xFF);

        /** The quotes were only used to keep whitespace characters that
         *  are unknowingly at the end of the strings... Take them off
         *  now and preserve those whitespace characters.
        */
        for(auto &ch : re_m[5].str())
        {
            if(ch != '\'')
                mSensorIdString.push_back(ch);
        }
    }
    else if(std::regex_match(link, re_m, re_picmg_led))
    {
        mConnectionId = re_m[1];
        mAddrType = Type::PICMG_LED; ///re_m[2];
        mLogicalFruDeviceSlaveSddress = (std::stoul(re_m[3]) & 0xFF);
        mLedId = (std::stoul(re_m[4]) & 0xFF);
    }
    else if (std::regex_match(link, re_m, re_oem_cmd))
    {
        mAddrType = Type::OEM_CMD;
        mConnectionId = re_m[1].str();
        mOemCmd.vendorId = re_m[2].str();
        mOemCmd.commandId = re_m[3].str();

        /* Make'em lowercase.*/
        std::transform(mOemCmd.vendorId.begin(), mOemCmd.vendorId.end(), mOemCmd.vendorId.begin(), ::tolower);
        std::transform(mOemCmd.commandId.begin(), mOemCmd.commandId.end(), mOemCmd.commandId.begin(), ::tolower);
        
        /** Optional command arguments*/
        if(!re_m[4].str().empty())
        {
            /** split the list of arguments based on whitespace*/
            int strstart = 0;
            int strend = 0;
            while((strstart = re_m[4].str().find_first_not_of( ' ',strend)) != std::string::npos)
            {
                strend = re_m[4].str().find(' ', strstart);
                mOemCmd.commandArgs.push_back(re_m[4].str().substr(strstart, strend-strstart));
            }
            for(auto &str : mOemCmd.commandArgs)
            {
                /** Make these lower too*/
                std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            }
        }
    }
        
    
    else {  /* Something is wrong. Throw now! */
        throw std::invalid_argument("Link field does not contain proper arguments. \'" + link + "\'");
    }
}

