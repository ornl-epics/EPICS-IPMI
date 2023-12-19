/** 
 * 
 * 
 * 
 */

#include "IpmiConnectionManager.h"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cmath>
#include <unistd.h>

IpmiConnectionManager::IpmiConnectionManager(const std::string &connectionid, const std::string &hostname,
    const std::string &username, const std::string &password,
    const std::string &authtype, const std::string &protocol,
    const std::string &privilegelevel)
: mConnId(connectionid)
, mHostname(hostname)
, mUserName(username)
, mPassword(password)
, mAuthtype(initAuthtype(authtype, username))
, mPrivlevel(initPrivLevel(privilegelevel))
, mProtocol(protocol)
, mCacheFilePath("/tmp/ipmi_sdr_" + connectionid + ".cache")
{
    
    mSdrRepositoryInfoRs = fiid_obj_create(tmpl_cmd_get_sdr_repository_info_rs);
    mSdrRepositoryInfoRq = fiid_obj_create(tmpl_cmd_get_sdr_repository_info_rq);

    try
    {
        createIpmiContext();
        connect();
    }
    catch(const std::exception &e)
    {
        ///std::cerr << e.what() << '\n';
        LOG_ERROR(e.what());
        cleanup();
    }
      
}

IpmiConnectionManager::~IpmiConnectionManager() {

    if (mIpmiCtx) {
        ipmi_ctx_close(mIpmiCtx);
        ipmi_ctx_destroy(mIpmiCtx);
    }
    if (mSdrCtx) {
        ipmi_sdr_ctx_destroy(mSdrCtx);
    }
    // if (m_ctx.sensors) {
    //     ipmi_sensor_read_ctx_destroy(m_ctx.sensors);
    // }
    // if (m_ctx.fru) {
    //     ipmi_fru_ctx_destroy(m_ctx.fru);
    // }
}

void IpmiConnectionManager::cleanup() {

    if (mSdrCtx) {
        ipmi_sdr_ctx_destroy(mSdrCtx);
    }
    if (mIpmiCtx) {
        ipmi_ctx_close(mIpmiCtx);
        ipmi_ctx_destroy(mIpmiCtx);
    }
    mSdrCtx = nullptr;
    mIpmiCtx = nullptr;
    
}

uint8_t IpmiConnectionManager::initAuthtype(const std::string &authenticationtype, const std::string &username) {

    if (authenticationtype == "none" || username.empty())
        return IPMI_AUTHENTICATION_TYPE_NONE;
    else if (authenticationtype == "plain" || authenticationtype == "straight_password_key")
        return IPMI_AUTHENTICATION_TYPE_STRAIGHT_PASSWORD_KEY;
    else if (authenticationtype == "md2")
        return IPMI_AUTHENTICATION_TYPE_MD2;
    else if (authenticationtype == "md5")
        return IPMI_AUTHENTICATION_TYPE_MD5;
    else
        throw std::runtime_error("Invalid authentication type for \'" +
        mConnId + "\' @ \'" + mHostname + "\' (choose from \'none\', \'plain\', \'md2\' , \'md5\')\n");

}

uint8_t IpmiConnectionManager::initPrivLevel(const std::string &privlegelevel) {

    if (privlegelevel == "admin")
        return IPMI_PRIVILEGE_LEVEL_ADMIN;
    else if (privlegelevel == "operator")
        return IPMI_PRIVILEGE_LEVEL_OPERATOR;
    else if (privlegelevel == "user")
        return IPMI_PRIVILEGE_LEVEL_USER;
    else
        throw std::runtime_error("Invalid privilege level for \'" +
        mConnId + "\' @ \'" + mHostname + "\' (choose from \'user\', \'operator\', \'admin\')\n");
}

void IpmiConnectionManager::createSdrContext() {

    if(!mIpmiCtx) {
        createIpmiContext();
    }

    if(mSdrCtx) {
        ipmi_sdr_ctx_destroy(mSdrCtx);
        mSdrCtx = nullptr;
    }
    
    mSdrCtx = ipmi_sdr_ctx_create();

    if (!mSdrCtx) {
        throw std::runtime_error("Can't create SDR context for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }

    openSdrCache();
}

void IpmiConnectionManager::createSensorContext() {

    if(mSensorCtx) {
        ipmi_sensor_read_ctx_destroy(mSensorCtx);
        mSensorCtx = nullptr;
    }
    
    mSensorCtx = ipmi_sensor_read_ctx_create(mIpmiCtx);
    if (!mSensorCtx) {
        throw std::runtime_error("Can't create sensor-read context for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }
    
    int sensorReadFlags = 0;
    sensorReadFlags |= IPMI_SENSOR_READ_FLAGS_BRIDGE_SENSORS;
    /* Don't error out, if this fails we can still continue */
    if (ipmi_sensor_read_ctx_set_flags(mSensorCtx, sensorReadFlags) < 0)
        LOG_WARN("can't set sensor read flags - %s", ipmi_sensor_read_ctx_errormsg(mSensorCtx));
}

void IpmiConnectionManager::rebuildSdrCache() {
    
    int rv = -1;
    LOG_INFO("Deleting out of date or invalid SDR cache file \'" + mCacheFilePath + "\' for connection id: \'" + mConnId + "\'\n");
    if((rv = ipmi_sdr_cache_close (mSdrCtx)) < 0) {
        throw std::runtime_error("Can't close SDR cache for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }

    if((rv = ipmi_sdr_cache_delete(mSdrCtx, mCacheFilePath.c_str())) < 0) {
        throw std::runtime_error("Can't delete SDR cache file for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }
    
    openSdrCache();
}

void IpmiConnectionManager::openSdrCache() {

    int rv = -1;
    /** open creates/opens the file and reads it into memory using mmap. So all
     * of the sdr parse calls come from memory, not the file.
    */
    if ((rv = ipmi_sdr_cache_open(mSdrCtx, mIpmiCtx, mCacheFilePath.c_str())) < 0) {
        
        switch (ipmi_sdr_ctx_errnum(mSdrCtx)) {
        case IPMI_SDR_ERR_CACHE_OUT_OF_DATE:
        case IPMI_SDR_ERR_CACHE_INVALID:
            LOG_INFO("Deleting out of date or invalid SDR cache file \'" + mCacheFilePath + "\' for connection id: \'" + mConnId + "\'");
            (void)ipmi_sdr_cache_delete(mSdrCtx, mCacheFilePath.c_str());
            // fall thru
        case IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST:
            LOG_INFO("Creating new SDR cache file \'" + mCacheFilePath + "\' for connection id: \'" + mConnId + "\'");
            (void)ipmi_sdr_cache_create(mSdrCtx, mIpmiCtx, mCacheFilePath.c_str(), IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT, nullptr, nullptr);
            break;
        default:
            throw std::runtime_error("Can't open SDR cache file \'" + mCacheFilePath + "\' for connection id: \'" + mConnId + "\' -" 
            + std::string(ipmi_ctx_errormsg(mIpmiCtx)));
        }
        if ((rv = ipmi_sdr_cache_open(mSdrCtx, mIpmiCtx, mCacheFilePath.c_str())) < 0)
            throw std::runtime_error("Can't open SDR cache file \'" + mCacheFilePath + "\' for connection id: \'" + mConnId + "\' -" 
            + std::string(ipmi_ctx_errormsg(mIpmiCtx)));
    }

    mCacheFileIsOpen = (rv == 0) ? true : false;
    mIdleTime = epicsTime::getCurrent(); 
}

void IpmiConnectionManager::createIpmiContext() {

    if (mIpmiCtx) {
        ipmi_ctx_close(mIpmiCtx);
        ipmi_ctx_destroy(mIpmiCtx);
        mIpmiCtx = nullptr;
    }

    mIpmiCtx = ipmi_ctx_create();

    if (!mIpmiCtx) {
        throw std::runtime_error("Can't create IPMI context for \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }
}

void IpmiConnectionManager::connect() {

    if(!mIpmiCtx) {
        createIpmiContext();
    }

    const char* username_ = (mUserName.empty() ? nullptr : mUserName.c_str());
    const char* password_ = (mPassword.empty() ? nullptr : mPassword.c_str());

    int connected = -1;

    if (mProtocol == "lan_2.0") {
        connected = ipmi_ctx_open_outofband_2_0(
                        mIpmiCtx, mHostname.c_str(), username_, password_,
                        m_k_g, m_k_g_len, mPrivlevel, mCipherSuiteId,
                        mSessionTimeout, mRetransmissionTimeout, mWorkaroundFlags, mFlags);
    } else {
        connected = ipmi_ctx_open_outofband(
                        mIpmiCtx, mHostname.c_str(), username_, password_,
                        mAuthtype, mPrivlevel,
                        mSessionTimeout, mRetransmissionTimeout, mWorkaroundFlags, mFlags);

    }
    
    if (connected < 0) {
        std::stringstream ss;
        ss << "Can't Connect to \'" << mConnId << "\' @ \'" << mHostname << "\' ";
        ss << "because of \'" << std::string(ipmi_ctx_errormsg(mIpmiCtx)) << "\'\n";
        throw std::runtime_error(ss.str());
    }

    std::cout << "Connected successfully to \'" << mConnId << "\' @ \'"
    << mHostname << "\'" << std::endl;
    /** We can set the idle time because I/O was transmitted in open because
     * we passed the ipmi context in.
    */
    mIdleTime = epicsTime::getCurrent();
    mConnStatus = true;
}

ipmi_ctx_t IpmiConnectionManager::getIpmiCtx() {
    if(!mIpmiCtx) {
        createIpmiContext();
    }
    mIdleTime = epicsTime::getCurrent();
    return mIpmiCtx;
}

ipmi_sdr_ctx_t IpmiConnectionManager::getSdrCtx() {
    if (!mSdrCtx) {
        createSdrContext();
    }

    mIdleTime = epicsTime::getCurrent();
    return mSdrCtx;
}

const std::string &IpmiConnectionManager::getConnectionId() const {
    return mConnId;
}

const std::string &IpmiConnectionManager::getHostname() const {
    return mHostname;
}

void IpmiConnectionManager::process() {

    epicsTime now = mIdleTime + ((mSessionTimeout/1000)/4);
    if(epicsTime::getCurrent() > now) {
        keepAlive();
    }
}

void IpmiConnectionManager::keepAlive() {

    /** We can read the SDR info from the device to keep the session from timing out.*/
    readSdrInfo();
}

IpmiSdrInfo IpmiConnectionManager::readSdrInfo() {
    
    int rv = -1;

    /** Return 0 on normal; This calls fiid_obj_clear internally*/
    if((rv = fill_cmd_get_repository_info(mSdrRepositoryInfoRq)) < 0) {
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fill_cmd_get_repository_info() returned -1");
    }

    if((rv = fiid_obj_clear(mSdrRepositoryInfoRs)) < 0) {
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_clear() returned -1");
    }

    if((rv = ipmi_cmd(mIpmiCtx, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_STORAGE_RQ, mSdrRepositoryInfoRq, mSdrRepositoryInfoRs)) < 0) {
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: ipmi_cmd() returned -1");
    }

    uint64_t compCode = 0;
    rv = fiid_obj_get(mSdrRepositoryInfoRs, "comp_code", &compCode);
    if(rv == 1) {
        /** Data was returned*/
        IpmiSdrInfo info = IpmiSdrInfo(mConnId, mSdrRepositoryInfoRs);
        mIdleTime = epicsTime::getCurrent();
        return info;
    }
    else if(rv == 0) {
        /* rv == 0: No data was returned. */
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned 0 (No data was available)");
    }
    else {
        /* rf == -1: Error was returned. */
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned -1 (Error)");
    }
    
}

Provider::Entity IpmiConnectionManager::readSensor(const std::shared_ptr<IpmiSensorRecComp> record) {

    if(!mSensorCtx) {
        createSensorContext();
    }
    
    Provider::Entity entity;
    uint8_t sharedOffset = 0; // TODO: shared sensors support
    uint8_t readingRaw = 0;
    double* reading = nullptr;
    uint16_t eventMask = 0;
    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &data = record->get_record_data();

    int rv = ipmi_sensor_read(mSensorCtx, data.data, data.size, sharedOffset, &readingRaw, &reading, &eventMask);
    
    if(rv != 1) {

        int err_num = ipmi_sensor_read_ctx_errnum (mSensorCtx);
        std::string str_error = ipmi_sensor_read_ctx_strerror (err_num);
        std::string str_errmsg = ipmi_sensor_read_ctx_errormsg (mSensorCtx);

        /** Not sure if ipmi_sensor_read_ctx_strerror() and ipmi_sensor_read_ctx_errormsg()
         * always return the same messages.
         * So, use Lambda to concat strings if they are different...
         */
        auto getErrStr = [&str_error, &str_errmsg]() {
            if(str_error.compare(str_errmsg) != 0) {
                return "\'" + str_error + "\' Error Message: \'" + str_errmsg + "\'";
            }
            else
                return "\'" + str_error + "\'";
        };
        
        throw IpmiException(err_num, std::move(getErrStr()));
    }
    
    /** Only threshold type sensors return a reading-value. The rest of the sensor types
     *  return the event-bit-mask only as the sensor reading-value; which represents an
     *  enumerated state. See section 42 in the IPMI specification.
     *  Note: Threshold sensors return two values: 1) the sensor-reading, 2) the even-mask.
     *  See Table 42-, Generic Event/Reading Type Codes for threshold events.
     *  TODO: Maybe handle threshold events somehow?
    */
    if(IPMI_EVENT_READING_TYPE_CODE_IS_THRESHOLD(record->get_event_reading_type_code())) {
        if(reading) {
            entity["VAL"] = std::round(*reading * 100.0) / 100.0;
            free(reading);
        }
        else
            entity["VAL"] = (double) eventMask;
    }
    else {
        entity["VAL"] = (double) eventMask;
    }
    
    mIdleTime = epicsTime::getCurrent();
    return entity;
}

IpmiSdrInfo IpmiConnectionManager::getSdrInfo() {
    return readSdrInfo();
}