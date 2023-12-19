/** 
 * 
 * 
 * 
 */

#ifndef IPMIAPP_SRC_IPMISDRMANAGER_H_
#define IPMIAPP_SRC_IPMISDRMANAGER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <freeipmi/freeipmi.h>
#include <epicsTime.h>
#include <epicsMutex.h>
#include "IpmiSensorRecFull.h"
#include "IpmiFruDevLocRec.h"
#include "IpmiConnectionManager.h"

class IpmiSdrManager
{
private:
    
    IpmiConnectionManager &mConnMgr;
    
    epicsMutex mMutex;
    epicsTime mReadTime;

    uint8_t mVersion;
    uint16_t mRecordCount;
    uint16_t mFreeSpace;
    uint32_t mAdditionTimestamp;
    uint32_t mEraseTimestamp;

    std::vector<std::shared_ptr<IpmiSensorRecFull>> mSensRecFullList;
    std::vector<std::shared_ptr<IpmiSensorRecComp>> mSensRecCompactList;
    std::vector<std::shared_ptr<IpmiFruDevLocRec>> mFruDevLocRecList;
    std::vector<std::shared_ptr<IpmiSensorRecComp>> mOrphandList;

    std::map<std::string, std::shared_ptr<IpmiSensorRecComp>> mSidEntityMap;
    std::map<std::shared_ptr<IpmiSensorRecComp>, uint16_t> mSensToFruMap;
    
    void readSdr();
    void insertRecord(ipmi_sdr_ctx_t psdr, uint16_t record_id, uint8_t record_type);
    void insertIntoEntityMap(std::shared_ptr<IpmiSensorRecComp> prec);
    void clearMaps();

    
public:
    IpmiSdrManager(IpmiConnectionManager &cmngr);
    ~IpmiSdrManager();
    void process();
    std::shared_ptr<IpmiFruDevLocRec> getFruByDeviceSlaveAddress(const uint8_t slave_address);
    std::shared_ptr<IpmiSensorRecComp> findSensorByMapKey(std::string key);
    std::string getHeaderAsString();
    
};

#endif ///IPMIAPP_SRC_IPMISDRMANAGER_H_