/** 
 * 
 * 
 * 
 * 
*/

#include "IpmiSdrManager.h"
#include "common.h"
#include <iostream>
#include <sstream>
#include "IpmiSdrInfo.h"

IpmiSdrManager::IpmiSdrManager(IpmiConnectionManager &cmngr)
: mConnMgr(cmngr)
{
    try
    {
        readSdr();
    }
    catch(const std::exception &e)
    {
        mMutex.unlock();
        LOG_ERROR(e.what());
    }
    
}

IpmiSdrManager::~IpmiSdrManager() {
    
}

void IpmiSdrManager::clearMaps() {

    if(mSensRecFullList.size() > 0)
        mSensRecFullList.clear();

    if(mSensRecCompactList.size() > 0)
        mSensRecCompactList.clear();

    if(mFruDevLocRecList.size() > 0)
        mFruDevLocRecList.clear();

    if(mOrphandList.size() > 0)
        mOrphandList.clear();

    if(mSidEntityMap.size() > 0)
        mSidEntityMap.clear();

    if(mSensToFruMap.size() > 0)
        mSensToFruMap.clear();

}

void IpmiSdrManager::readSdr() {

    mMutex.lock();

    ipmi_sdr_ctx_t sdr = mConnMgr.getSdrCtx();

    /* Get the SDR version. */
    if(ipmi_sdr_cache_sdr_version (sdr, &mVersion) < 0)
        throw std::runtime_error("Cannot read SDR cache version for connection id: \'" + mConnMgr.getConnectionId() + "\'\n");

    /* Get the SDR most recent addition timestamp */
    if(ipmi_sdr_cache_most_recent_addition_timestamp (sdr, &mAdditionTimestamp) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent addition timestamp.");

    /* Get the SDR most recent erase timestamp */
    if(ipmi_sdr_cache_most_recent_erase_timestamp (sdr, &mEraseTimestamp) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent erase timestamp.");

    /* Get the SDR record count */
    if(ipmi_sdr_cache_record_count (sdr, &mRecordCount) < 0)
        throw std::runtime_error("Error! Could not read SDR cache record count.");

    /** This is needed after initialization and we need to rebuild/re read the SDR*/
    clearMaps();

    uint16_t record_id = 0;
    uint8_t record_type = 0;

    /* Iterate through all of the records in the SDR and create sensor lists as needed. */
    for(int i = 0; i < mRecordCount; i++, ipmi_sdr_cache_next(sdr)) {
        if(ipmi_sdr_parse_record_id_and_type (sdr, nullptr, 0, &record_id, &record_type)<0)
            throw std::runtime_error("Could not read record ID and record type in SDR.");

        /* Add this record type to the list. When constructor is called we read more sensor data */
        insertRecord(sdr, record_id, record_type);
    }

    std::copy(mSensRecFullList.begin(), mSensRecFullList.end(), std::back_inserter(mOrphandList));
    std::copy(mSensRecCompactList.begin(), mSensRecCompactList.end(), std::back_inserter(mOrphandList));

    /* Iterate through all of the FRU Device Locator Records and attach sensors to their parent FRUs */
    for(auto &fdlr: mFruDevLocRecList) {
        fdlr.get()->parseAssociations(mOrphandList);
    }

    /* Create a reverse lookup map so the sensor can find its parent FRU. */
    for(auto &fdlr : mFruDevLocRecList) {
        std::vector<std::shared_ptr<IpmiSensorRecComp>> &sensrs = fdlr.get()->get_sensors();
        for(auto &sensr : sensrs) {
            mSensToFruMap.insert({sensr, fdlr.get()->get_device_slave_address()});
        }
    }

    mReadTime = epicsTime::getCurrent();
    mMutex.unlock();
    mSdrState = SDRSTATE::INITIALIZED;
}

int IpmiSdrManager::compSdrHeader()
{
    uint8_t ver = 0;
    uint32_t add_ts = 0;
    uint32_t era_ts = 0;
    uint16_t rec_count = 0;
    int rval = 0;

    ipmi_sdr_ctx_t sdr = mConnMgr.getSdrCtx();

    /* Get the SDR version. */
    if(ipmi_sdr_cache_sdr_version (sdr, &ver) < 0)
        throw std::runtime_error("Cannot read SDR cache version for connection id: \'" + mConnMgr.getConnectionId() + "\'\n");

    /* Get the SDR most recent addition timestamp */
    if(ipmi_sdr_cache_most_recent_addition_timestamp (sdr, &add_ts) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent addition timestamp.");

    /* Get the SDR most recent erase timestamp */
    if(ipmi_sdr_cache_most_recent_erase_timestamp (sdr, &era_ts) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent erase timestamp.");

    /* Get the SDR record count */
    if(ipmi_sdr_cache_record_count (sdr, &rec_count) < 0)
        throw std::runtime_error("Error! Could not read SDR cache record count.");
    
    if(ver != mVersion ||
        rec_count != mRecordCount ||
        add_ts != mAdditionTimestamp ||
        era_ts != mEraseTimestamp)
        {
            rval = 1;
        }
    return rval;
}

void IpmiSdrManager::insertRecord(ipmi_sdr_ctx_t psdr, uint16_t record_id, uint8_t record_type) {

    if(record_type == IPMI_SDR_FORMAT_FULL_SENSOR_RECORD) {
        std::shared_ptr<IpmiSensorRecFull> p;

        /** Add sensor to list */
        p = std::make_shared<IpmiSensorRecFull>(psdr, record_id, record_type);
        mSensRecFullList.push_back(p);

        /** Add sensor to map with entity-id:entity-instance:sensor-number as key */
        insertIntoEntityMap(p);
    }

    /* Add this record type to the list. Compact and Full are so close to the same.
    * When constructor is called we read more sensor data
    */
    if(record_type == IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD) {
        std::shared_ptr<IpmiSensorRecComp> p;

        /** Add sensor to list */
        p = std::make_shared<IpmiSensorRecComp>(psdr, record_id, record_type);
        mSensRecCompactList.push_back(p);

        /** Add sensor to map with entity-id:entity-instance:sensor-number as key */
        insertIntoEntityMap(p);
    }

    if(record_type == IPMI_SDR_FORMAT_EVENT_ONLY_RECORD) {
        ///TODO:
        ;
    }

    if(record_type == IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD) {
        ///TODO:
        ;
    }

    /* Add this record type to the list. This record type is useful for grouping sensor based on
        * FRU to sensor relationships. 
        */
    if(record_type == IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
        mFruDevLocRecList.push_back(std::make_shared<IpmiFruDevLocRec>(mConnMgr.getIpmiCtx(), psdr, record_id, record_type));
    }

    if(record_type == IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD) {
        ///TODO:
        ;
    }
}

void IpmiSdrManager::insertIntoEntityMap(std::shared_ptr<IpmiSensorRecComp> prec) {

    /** Add sensor to map with entity-id:entity-instance:sensor-number as key */
    std::pair<std::map<std::string, std::shared_ptr<IpmiSensorRecComp>>::iterator,bool> rv;

    /** Create the key */
    std::string sidKey = std::to_string(prec->get_entity_id()) + ":" + 
    std::to_string(prec->get_entity_instance()) + ":" + prec->get_device_id_string();

    rv = mSidEntityMap.insert({sidKey, prec});
    if(rv.second == false) {
        
        throw std::runtime_error("Cannot insert SDR record into map for connection id: \'" + mConnMgr.getConnectionId() + "\'. Duplicate Keys Exists: " + sidKey);
    }

}

void IpmiSdrManager::process() {

    /**
     * SDR could change in two places: (cache file or device)
    */

    /**
     * First, check to see if the SDR cache file has changed.
     * This could happen if the equipment reboots and the 
     * IpmiConnectionManager reconnects and detects a file difference.
    */
    
    if(compSdrHeader() != 0)
    {
        readSdr();
        return;
    }
    /**
     * Second, check to see if the SDR in the field device has changed.
     * If so, then we need to rebuild our cache file and local memory
     * objects.
    */
    epicsTime now = mReadTime + 60;
    if(epicsTime::getCurrent() > now) {

        IpmiSdrInfo info = mConnMgr.getSdrInfo();
        mReadTime = epicsTime::getCurrent();

        if(info.getVersion() != mVersion ||
        info.getRecordCount() != mRecordCount ||
        info.getMostRecentAdditionTimestamp() != mAdditionTimestamp ||
        info.getMostRecentEraseTimestamp() != mEraseTimestamp) {

            LOG_INFO("SDR difference detected for device \'" + mConnMgr.getConnectionId() +
            "\' @ \'" + mConnMgr.getHostname() + "\'\n\n");

            std::stringstream ss;
            
            ss << "SDR Difference Summary {\n";
            ss << " * Version: Cache = " << (unsigned) mVersion << ", " << mConnMgr.getConnectionId() << " = " << (unsigned) info.getVersion() << ",\n";
            ss << " * Record Count: Cache = " << mRecordCount << ", " << mConnMgr.getConnectionId() << " = " << info.getRecordCount() << ",\n";
            ss << " * Addition Timestamp: Cache = " << timestampToString(mAdditionTimestamp) << ", "
            << mConnMgr.getConnectionId() << " = " << timestampToString(info.getMostRecentAdditionTimestamp()) << ",\n";

            ss << " * Erase Timestamp: Cache = " << timestampToString(mEraseTimestamp) << ", "
            << mConnMgr.getConnectionId() << " = " << timestampToString(info.getMostRecentEraseTimestamp()) << ",\n";

            ss << "}\n\n";

            std::cout << ss.str();

            LOG_INFO("Rebuilding the SDR cache now for device \'" + mConnMgr.getConnectionId() +
            "\' @ \'" + mConnMgr.getHostname() + "\'\n\n");

            try
            {
                mConnMgr.rebuildSdrCache();

                try
                {
                    readSdr();
                }
                catch(const std::exception& e)
                {
                    mMutex.unlock();
                    LOG_ERROR("Could not read SDR cache after rebuild for \'" + mConnMgr.getConnectionId() + "\' @ \'"
                    + mConnMgr.getHostname() + "\' - " + e.what() + "\n");
                }
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("Could not rebuild SDR cache for \'" + mConnMgr.getConnectionId() + "\' @ \'"
                + mConnMgr.getHostname() + "\' - " + e.what() + "\n");
            }
        }
    }
}

std::shared_ptr<IpmiFruDevLocRec> IpmiSdrManager::getFruByDeviceSlaveAddress(const uint8_t slave_address) {

    std::shared_ptr<IpmiFruDevLocRec> pfru = nullptr;

    mMutex.lock();
    for(auto &fru : mFruDevLocRecList) {
        if(fru->get_device_slave_address() == slave_address) {
            pfru = fru;
            break;
        }
    }
    mMutex.unlock();
    return pfru;
}

std::shared_ptr<IpmiSensorRecComp> IpmiSdrManager::findSensorByMapKey(std::string key) {

    std::map<std::string, std::shared_ptr<IpmiSensorRecComp>>::iterator itr;
    std::shared_ptr<IpmiSensorRecComp> pSens = nullptr;

    mMutex.lock();
    itr = mSidEntityMap.find(key);
    if(itr != mSidEntityMap.end()) {
        pSens = itr->second;
    }

    mMutex.unlock();
    return pSens;
}

std::string IpmiSdrManager::timestampToString(const uint32_t &tstamp) {
    
    epicsTimeStamp etsmp;
    epicsTimeFromTime_t(&etsmp, tstamp);
    char timetxt[40] = {'\0'};
    epicsTimeToStrftime(timetxt, sizeof(timetxt), "[%H:%M:%S %m/%d/%Y]", &etsmp);
    return std::string(timetxt);
}

std::string IpmiSdrManager::getHeaderAsString() {

    std::stringstream ss;

    if(mSdrState == SDRSTATE::UNINITIALIZED) {
        ss << mConnMgr.getConnectionId() << ":" << mConnMgr.getHostname() << " SDR Cache Info {" << std::endl;
        ss << " * SDR Cache State: \'UNINITIALIZED\'" << std::endl;
        ss << " * SDR Record Count: ?," << std::endl;
        ss << " * SDR Version: ?," << std::endl;
        ss << " * SDR Addition Timestamp: ?," << std::endl;
        ss << " * SDR Erase Timestamp: ?" << std::endl;
        ss << "}" << std::endl;
        return ss.str();
    }

    ss << mConnMgr.getConnectionId() << ":" << mConnMgr.getHostname() << " SDR Cache Info {" << std::endl;
    ss << " * SDR Cache State: \'INITIALIZED\'" << std::endl;
    ss << " * SDR Record Count: " << mRecordCount << "," << std::endl;
    ss << " * SDR Version: " << (unsigned) mVersion << "," << std::endl;
    ss << " * SDR Addition Timestamp: " << timestampToString(mAdditionTimestamp) << "," << std::endl;
    ss << " * SDR Erase Timestamp: " << timestampToString(mEraseTimestamp) << std::endl;
    ss << "}" << std::endl;
    
    return ss.str();
}

bool IpmiSdrManager::sdrStateIsInitialized() {
    return (mSdrState == SDRSTATE::INITIALIZED);
}