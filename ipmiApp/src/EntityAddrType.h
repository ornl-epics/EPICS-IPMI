/** 
 * 
 * 
 * 
 */

#ifndef IPMIAPP_SRC_ENTITYADDRTYPE_H_
#define IPMIAPP_SRC_ENTITYADDRTYPE_H_

#include <string>
#include <map>
#include <vector>
#include <tuple>

class EntityAddrType {

public:
    enum class Type {
        SENSOR,
        FRU,
        PICMG_LED,
        OEM_CMD
    };

private:
    std::string mConnectionId;
    EntityAddrType::Type mAddrType;

    uint8_t mSensorEntityId;
    uint8_t mSensorEntityInstance;
    std::string mSensorIdString;

    uint8_t mLogicalFruDeviceSlaveSddress;
    uint8_t mLedId;

    struct OEM_Command
    {
        std::string vendorId;
        std::string commandId;
        std::vector<std::string> commandArgs;
    } mOemCmd;
    

    void parseInOutString(const std::string &link);
public:
    EntityAddrType(const std::string &recInOutString);
    ~EntityAddrType();

    const std::string &getConnectionId() const;
    EntityAddrType::Type getEntityAddressType() const;
    const std::string getEntityAddressTypeAsString() const;

    std::pair<uint8_t, bool> getSensorEntityId() const;
    std::pair<uint8_t, bool> getSensorEntityInstance() const;
    std::pair<const std::string &, bool> getSensorIdString() const;
    const std::string getSensorIdAsKey() const;

    std::pair<uint8_t, bool> getPicmgLedFruDeviceSlaveSddress() const;
    std::pair<uint8_t, bool> getPicmgLedId() const;

    std::tuple<const std::string, const std::string, const std::vector<std::string>> get_oem_command() const;

};

#endif /** IPMIAPP_SRC_ENTITYADDRTYPE_H_ */

