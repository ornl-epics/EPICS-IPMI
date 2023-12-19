/** 
 * 
 * 
 * 
 */

#ifndef IPMIAPP_SRC_IPMISDRINFO_H_
#define IPMIAPP_SRC_IPMISDRINFO_H_

#include <cstdint>
#include <string>
#include <freeipmi/freeipmi.h>


class IpmiSdrInfo
{
private:
    
    std::string mConnId;
    uint8_t mVersion;
    uint16_t mRecordCount;
    uint16_t mFreeSpace;
    uint32_t mAdditionTimestamp;
    uint32_t mEraseTimestamp;
    bool mAllocationCommandSupported{false};
    bool mReserveCommandSupported{false};
    bool mPartialAddCommandSupported{false};
    bool mDeleteCommandSupported{false};
    bool mReserved{false};
    uint8_t mModalNonModalOperationSupported{0};
    bool mOverflowFlag{false};

    void decode(fiid_obj_t response);
    
public:

    IpmiSdrInfo(const std::string &connectionid, fiid_obj_t response);
    IpmiSdrInfo(IpmiSdrInfo &&other);
    ~IpmiSdrInfo();
    IpmiSdrInfo &operator=(IpmiSdrInfo &&other);
    uint8_t getVersion();
    uint16_t getRecordCount();
    uint32_t getMostRecentAdditionTimestamp();
    uint32_t getMostRecentEraseTimestamp();
    bool getAllocationCommandSupported();
    bool getReserveCommandSupported();
    bool getPartialAddCommandSupported();
    bool getDeleteCommandSupported();
    uint8_t getModalNonModalOperationSupported();
    bool getOverflowFlag();
};

#endif ///IPMIAPP_SRC_IPMISDRINFO_H_