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
#include <iomanip>

std::map<std::string, uint8_t> IpmiConnectionManager::VADATECH_SITE_TYPES =
{
    {"mch", 0x0A},
    {"amc", 0x07},
    {"cu", 0x04},
    {"pm", 0x0B}
};

const std::map<std::string, std::string> IpmiConnectionManager::mThresholdsMap =
{
    {"lower_critical_threshold", "LOLO"},
    {"lower_non_critical_threshold", "LOW"},
    {"upper_non_critical_threshold", "HIGH"},
    {"upper_critical_threshold", "HIHI"}
};

/** "readable_thresholds.lower_non_critical_threshold" */
const std::string IpmiConnectionManager::mThresholdReadables [] =
{
    "lower_non_critical_threshold",
    "lower_critical_threshold",
    "lower_non_recoverable_threshold",
    "upper_non_critical_threshold",
    "upper_critical_threshold",
    "upper_non_recoverable_threshold"
};

const std::string IpmiConnectionManager::mSensorHysteresisValues [] =
{
    "positive_going_threshold_hysteresis_value",
    "negative_going_threshold_hysteresis_value"
};

std::map<std::list<std::string>, std::map<std::string, IpmiConnectionManager::OEM_HANDLER>> IpmiConnectionManager::oem_cmds = {
    /** Format is {vendor-ids[]}, {cmd-name, callback}*/
    {
        /** List of vendor-ids*/
        {{"vadatech"},{"vt"}}, 
                                {    /** Inner map of OEM commands and handlers.*/
                                    {"reboot", &IpmiConnectionManager::vadatech_reboot_chassis},
                                    {"set-power-state", &IpmiConnectionManager::vadatech_set_power_state}
                                }
    }
};

int IpmiConnectionManager::vadatech_set_power_state(ipmi_ctx_t ctx, const std::vector<std::string> &args, Provider::Entity &entity)
{
    if(args.size() < 2)
    {
        throw std::runtime_error("Can't set_power_state. vadatech set_power_state requires two arguments: "
        "Module type (e.g., AMC, MCH, PM,...) and Site Number (1...16).\n"
        "Example: set_power_state AMC 1\n");
    }
    if(!entity.hasField("VAL"))
    {
        throw std::runtime_error("Can't set_power_state. Missing \'VAL\' field. Device support routine is supposed to set the VAL field.\n");
    }

    const uint8_t SET_CHASSIS_POWER_STATE = 0x9E;
    const uint8_t val = entity.getField<int>("VAL", 0);
    const std::string &SITE_TYPE_NAME = args[0];
    const std::string &SITE_ID_STRING = args[1];
    const uint8_t SITE_ID = std::stoi(SITE_ID_STRING);

    auto site_type_itr = IpmiConnectionManager::VADATECH_SITE_TYPES.find(SITE_TYPE_NAME);
    if(site_type_itr == IpmiConnectionManager::VADATECH_SITE_TYPES.end())
    {
        throw std::runtime_error("Can't set_power_state. Invalid Site Type \'" + SITE_TYPE_NAME + "\'\n");
    }
    const uint8_t SITE_TYPE_VALUE = site_type_itr->second;

    const uint8_t buf_rq [] = {SET_CHASSIS_POWER_STATE, val, SITE_TYPE_VALUE, SITE_ID};
    uint8_t buf_rs [50] = {0};
    int rval = IpmiConnectionManager::send_ipmi_cmd_raw_ipmb(ctx, IPMI_CHANNEL_NUMBER_PRIMARY_IPMB, IpmiConnectionManager::VADATECH_IPMB_ADDRESS,
        IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_OEM_GROUP_RQ, &buf_rq, sizeof(buf_rq), &buf_rs, sizeof(buf_rs));

    /** 
     * Inspect the reply.
     * Based on tests, two bytes are returned:
     * byte[0] is the Command Code that was sent out in the request, SET_CHASSIS_POWER_STATE (0x9E)
     * byte[1] is the Completion Code. 0x00 is a 'Command Completed Normally'
     * See Table 5-, Completion Codes in the IPMI specification.
    */

    if(rval != 2)
    {
        throw std::runtime_error("set_power_state returned an invalid value. "
        "A value of 2 was expected but " + std::to_string(rval) + " was returned.\n");
    }
    else if (buf_rs[0] != SET_CHASSIS_POWER_STATE)
    {
        throw std::runtime_error("set_power_state returned an invalid value. "
        "OEM command of \'" + common::hex_dump(&SET_CHASSIS_POWER_STATE, 0, 1) + "\' expected but \'" + common::hex_dump(&buf_rs[0],0,1) + "\' was returned.\n");
    }
    else if (buf_rs[1] != IPMI_COMP_CODE_COMMAND_SUCCESS)
    {
        throw std::runtime_error("set_power_state returned an invalid Completion Code. "
        "A value of \'0x00\' was expected but \'" + common::hex_dump(&buf_rs[1], 0, 1) + "\' was returned.\n");
    }
    
    return 0;
}

int IpmiConnectionManager::vadatech_reboot_chassis(ipmi_ctx_t ctx, const std::vector<std::string> &args, Provider::Entity &entity)
{
    if(!entity.hasField("VAL"))
    {
        throw std::runtime_error("Can't reboot_chassis. Missing \'VAL\' field. Device support routine is supposed to set the VAL field.\n");
    }

    const uint8_t SET_CHASSIS_POWER_STATE = 0x9E;
    const uint8_t val = entity.getField<int>("VAL", 0);
    
    if(val < 1)
    {
        return 0;
    }
    
    uint8_t buf_rq [] = {SET_CHASSIS_POWER_STATE, 0x00, 0xFF, 0xFF};
    uint8_t buf_rs [50] = {0};

    /** This command returns -1 for the whole chassis reboot.*/
    int rval = IpmiConnectionManager::send_ipmi_cmd_raw_ipmb(ctx, IPMI_CHANNEL_NUMBER_PRIMARY_IPMB, IpmiConnectionManager::VADATECH_IPMB_ADDRESS,
        IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_OEM_GROUP_RQ, &buf_rq, sizeof(buf_rq), &buf_rs, sizeof(buf_rs));

    return 0;
}

int IpmiConnectionManager::send_ipmi_cmd_raw_ipmb(ipmi_ctx_t ctx, uint8_t channel_number,
    uint8_t rs_addr, uint8_t lun, uint8_t net_fn, const void *buf_rq, unsigned int buf_rq_len,
    void *buf_rs, unsigned int buf_rs_len)
{
    int rval = ipmi_cmd_raw_ipmb (ctx, channel_number, rs_addr, lun, net_fn, buf_rq,
                buf_rq_len, buf_rs, buf_rs_len);

    return rval;
}

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
, mCachePath(fs::current_path()/"iocBoot/var/ipmi")
, mCacheFile(mCachePath / (connectionid + "." + hostname + ".cache"))
{
    
    mSdrRepositoryInfoRs = fiid_obj_create(tmpl_cmd_get_sdr_repository_info_rs);
    mSdrRepositoryInfoRq = fiid_obj_create(tmpl_cmd_get_sdr_repository_info_rq);
    mGetSensorThresholdsRq = fiid_obj_create(tmpl_cmd_get_sensor_thresholds_rq);
    mGetSensorThresholdsRs = fiid_obj_create(tmpl_cmd_get_sensor_thresholds_rs);
    mGetSensorHysteresisRq = fiid_obj_create(tmpl_cmd_get_sensor_hysteresis_rq);
    mGetSensorHysteresisRs = fiid_obj_create(tmpl_cmd_get_sensor_hysteresis_rs);

    try
    {
        if(!fs::exists(mCachePath))
        {
            LOG_INFO("IPMI cache directory does not exist! \'" + mCachePath.string() + "\'");
            LOG_INFO("Creating the IPMI cache directory now...");
            fs::create_directory(mCachePath);
        }

        createIpmiContext();
        createSdrContext();
        connect();
        openSdrCache();
        createSensorContext(); /** This has to come after connection is ready to go.*/
    }
    catch(fs::filesystem_error const &fserr)
    {
        LOG_ERROR(fserr.what());
        cleanup();
    }
    catch(const std::exception &e)
    {
        ///std::cerr << e.what() << '\n';
        LOG_ERROR(e.what());
        cleanup();
    }
      
}

IpmiConnectionManager::~IpmiConnectionManager()
{

    if (mIpmiCtx)
    {
        ipmi_ctx_close(mIpmiCtx);
        ipmi_ctx_destroy(mIpmiCtx);
    }
    if (mSdrCtx)
    {
        ipmi_sdr_ctx_destroy(mSdrCtx);
    }
    // if (m_ctx.sensors) {
    //     ipmi_sensor_read_ctx_destroy(m_ctx.sensors);
    // }
    // if (m_ctx.fru) {
    //     ipmi_fru_ctx_destroy(m_ctx.fru);
    // }
}

void IpmiConnectionManager::cleanup()
{

    if (mSensorCtx)
    {
        ipmi_sensor_read_ctx_destroy(mSensorCtx);
    }
    // if (m_ctx.fru) {
    //     ipmi_fru_ctx_destroy(m_ctx.fru);
    // }
    if(mSdrCtx)
    {
        if(mCacheFileIsOpen)
        {
            if(ipmi_sdr_cache_close (mSdrCtx) < 0)
            {
                LOG_INFO("Can't close SDR cache for connection id: \'" +
                mConnId + "\' @ \'" + mHostname + "\' in cleanup method.\n");
            }
        }
        ipmi_sdr_ctx_destroy(mSdrCtx);
    }
    if (mIpmiCtx)
    {
        ipmi_ctx_close(mIpmiCtx);
        ipmi_ctx_destroy(mIpmiCtx);
    }
    mSensorCtx = nullptr;
    mSdrCtx = nullptr;
    mIpmiCtx = nullptr;
    mConnState = ConnectionState::DISCONNECTED;
    mCacheFileIsOpen = false;
    return;
}

uint8_t IpmiConnectionManager::initAuthtype(const std::string &authenticationtype, const std::string &username)
{

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

uint8_t IpmiConnectionManager::initPrivLevel(const std::string &privlegelevel)
{

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

void IpmiConnectionManager::createSdrContext()
{
    if(mSdrCtx)
    {
        ipmi_sdr_ctx_destroy(mSdrCtx);
    }
    mSdrCtx = ipmi_sdr_ctx_create();

    if (!mSdrCtx)
    {
        throw std::runtime_error("Can't create SDR context for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }

}

void IpmiConnectionManager::createSensorContext()
{
    if (mSensorCtx)
    {
        ipmi_sensor_read_ctx_destroy(mSensorCtx);
    }
    mSensorCtx = ipmi_sensor_read_ctx_create(mIpmiCtx);

    if (!mSensorCtx)
    {
        throw std::runtime_error("Can't create sensor-read context for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }
    
    int sensorReadFlags = 0;
    sensorReadFlags |= IPMI_SENSOR_READ_FLAGS_BRIDGE_SENSORS;
    /* Don't error out, if this fails we can still continue */
    if (ipmi_sensor_read_ctx_set_flags(mSensorCtx, sensorReadFlags) < 0)
        LOG_WARN("Can't set sensor-read-context flags for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\' - " + ipmi_sensor_read_ctx_errormsg(mSensorCtx) + "\n");
}

void IpmiConnectionManager::rebuildSdrCache()
{
    
    int rv = -1;
    LOG_INFO("Deleting out of date or invalid SDR cache file \'" + mCacheFile.string() + "\' for connection id: \'" + mConnId + "\'\n");
    if((rv = ipmi_sdr_cache_close (mSdrCtx)) < 0)
    {
        LOG_ERROR("Can't close SDR cache for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }

    if((rv = ipmi_sdr_cache_delete(mSdrCtx, mCacheFile.c_str())) < 0)
    {
        LOG_ERROR("Can't delete SDR cache file for connection id: \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }
    
    openSdrCache();
}

void IpmiConnectionManager::openSdrCache()
{

    int rv = -1;
    /** open creates/opens the file and reads it into memory using mmap. So all
     * of the sdr parse calls come from memory, not the file.
    */
    if ((rv = ipmi_sdr_cache_open(mSdrCtx, mIpmiCtx, mCacheFile.c_str())) < 0)
    {
        
        switch (ipmi_sdr_ctx_errnum(mSdrCtx))
        {
            case IPMI_SDR_ERR_CACHE_OUT_OF_DATE:
            case IPMI_SDR_ERR_CACHE_INVALID:
                LOG_INFO("Deleting out of date or invalid SDR cache file \'" + mCacheFile.string() + "\' for connection id: \'" + mConnId + "\'");
                (void)ipmi_sdr_cache_delete(mSdrCtx, mCacheFile.c_str());
                // fall thru
            case IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST:
                LOG_INFO("Creating new SDR cache file \'" + mCacheFile.string() + "\' for connection id: \'" + mConnId + "\'");
                (void)ipmi_sdr_cache_create(mSdrCtx, mIpmiCtx, mCacheFile.c_str(), IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT, NULL, NULL);
                break;
            default:
                throw std::runtime_error("Can't open SDR cache file \'" + mCacheFile.string() + "\' for connection id: \'" + mConnId + "\' -" 
                + std::string(ipmi_ctx_errormsg(mIpmiCtx)));
        }
        if ((rv = ipmi_sdr_cache_open(mSdrCtx, mIpmiCtx, mCacheFile.c_str())) < 0)
            throw std::runtime_error("Can't open SDR cache file \'" + mCacheFile.string() + "\' for connection id: \'" + mConnId + "\' -" 
            + std::string(ipmi_ctx_errormsg(mIpmiCtx)));
    }

    mCacheFileIsOpen = (rv == 0) ? true : false;
    updateIdleTime();
}

void IpmiConnectionManager::createIpmiContext()
{
    if (mIpmiCtx)
    {
        ipmi_ctx_close(mIpmiCtx);
        ipmi_ctx_destroy(mIpmiCtx);
    }

    mIpmiCtx = ipmi_ctx_create();

    if (!mIpmiCtx)
    {
        throw std::runtime_error("Can't create IPMI context for \'" +
        mConnId + "\' @ \'" + mHostname + "\'\n");
    }
}

void IpmiConnectionManager::connect()
{

    const char* username_ = (mUserName.empty() ? nullptr : mUserName.c_str());
    const char* password_ = (mPassword.empty() ? nullptr : mPassword.c_str());

    int connected = -1;

    if (mProtocol == "lan_2.0")
    {
        connected = ipmi_ctx_open_outofband_2_0(
                        mIpmiCtx, mHostname.c_str(), username_, password_,
                        m_k_g, m_k_g_len, mPrivlevel, mCipherSuiteId,
                        mSessionTimeout, mRetransmissionTimeout, mWorkaroundFlags, mFlags);
    }
    else 
    {
        connected = ipmi_ctx_open_outofband(
                        mIpmiCtx, mHostname.c_str(), username_, password_,
                        mAuthtype, mPrivlevel,
                        mSessionTimeout, mRetransmissionTimeout, mWorkaroundFlags, mFlags);

    }
    
    if (connected < 0)
    {
        std::stringstream ss;
        ss << "Can't Connect to \'" << mConnId << "\' @ \'" << mHostname << "\' ";
        ss << "because of \'" << std::string(ipmi_ctx_errormsg(mIpmiCtx)) << "\'\n";
        throw std::runtime_error(ss.str());
    }

    LOG_INFO("Connected successfully to \'" + mConnId + "\' @ \'"
    + mHostname + "\'\n");

    /** We can set the idle time because I/O was transmitted in open because
     * we passed the ipmi context in.
    */
    updateIdleTime();
    mConnStatus = true;
    mConnState = ConnectionState::CONNECTED;
}

void IpmiConnectionManager::disconnect()
{
    printf("Disconnecting...\n");
    cleanup();
    mDisconnectTime = epicsTime::getCurrent();
}

void IpmiConnectionManager::reconnect()
{

    /*
    * Wait a minute before we try to reconnect.
    * If we have rebooted the chassis, give it time
    * to fully come back and initialize.
    */
    if(epicsTime::getCurrent() > mDisconnectTime+60)
    {
        createIpmiContext();
        createSdrContext();
        connect();
        openSdrCache();
        createSensorContext(); /** This has to come after connection is ready to go.*/
    }
}

ipmi_ctx_t IpmiConnectionManager::getIpmiCtx()
{
    if(!mIpmiCtx)
    {
        createIpmiContext();
    }

    return mIpmiCtx;
}

ipmi_sdr_ctx_t IpmiConnectionManager::getSdrCtx()
{

    if (mConnState != ConnectionState::CONNECTED)
    {
        throw std::runtime_error("Can't return IPMI SDR context for \'" +
        mConnId + "\' @ \'" + mHostname + "\' Device is disconnected.\n");
    }
    if (!mSdrCtx)
    {
        throw std::runtime_error("Can't return IPMI SDR context for \'" +
        mConnId + "\' @ \'" + mHostname + "\' sdr-ctx is null.\n");
    }
    if (!mCacheFileIsOpen)
    {
        throw std::runtime_error("Can't return IPMI SDR context for \'" +
        mConnId + "\' @ \'" + mHostname + "\' sdr-cache-file is not open.\n");
    }

    return mSdrCtx;
}

const std::string &IpmiConnectionManager::getConnectionId() const
{
    return mConnId;
}

const std::string &IpmiConnectionManager::getHostname() const
{
    return mHostname;
}

void IpmiConnectionManager::process()
{

    if(mConnState == ConnectionState::CONNECTED)
    {
        epicsTime now = mIdleTime + ((mSessionTimeout/1000)/2);
        if(epicsTime::getCurrent() > now)
        {
            keepAlive();
        }
    }
    else
    {
        reconnect();
    }
}

bool IpmiConnectionManager::isConnected()
{
    return (mConnState == ConnectionState::CONNECTED);
}

void IpmiConnectionManager::keepAlive()
{

    /** We can read the SDR info from the device to keep the session from timing out.*/
    readSdrInfo();
}

IpmiSdrInfo IpmiConnectionManager::readSdrInfo()
{
    
    int rv = -1;

    /** Return 0 on normal; This calls fiid_obj_clear internally*/
    if((rv = fill_cmd_get_repository_info(mSdrRepositoryInfoRq)) < 0)
    {
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fill_cmd_get_repository_info() returned -1");
    }

    if((rv = fiid_obj_clear(mSdrRepositoryInfoRs)) < 0)
    {
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_clear() returned -1");
    }

    if((rv = ipmi_cmd(mIpmiCtx, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_STORAGE_RQ, mSdrRepositoryInfoRq, mSdrRepositoryInfoRs)) < 0)
    {
        disconnect();
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: ipmi_cmd() returned -1");
    }

    uint64_t compCode = 0;
    rv = fiid_obj_get(mSdrRepositoryInfoRs, "comp_code", &compCode);
    if(rv == 1)
    {
        /** Data was returned*/
        IpmiSdrInfo info = IpmiSdrInfo(mConnId, mSdrRepositoryInfoRs);
        updateIdleTime();
        return info;
    }
    else if(rv == 0)
    {
        /* rv == 0: No data was returned. */
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned 0 (No data was available)");
    }
    else
    {
        /* rf == -1: Error was returned. */
        throw std::runtime_error("Can't read SDR info for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned -1 (Error)");
    }
    
}

void IpmiConnectionManager::updateIdleTime()
{
    mIdleTime = epicsTime::getCurrent();
}

Provider::Entity IpmiConnectionManager::getSensorReading(const std::shared_ptr<IpmiSensorRecComp> record)
{


    try
    {
        return readSensor(record);
    }
    catch(const IpmiException &e)
    {
        /**
         * Trap possible session-timeouts and handle reconnections. 
         * This is indicative of a session timeout/device disconnected.
         * The actual error code/message returned from IPMI will be 16/'internal IPMI error'
         * which isn't very descriptive. But if you dig deeper you find 'session timeout'.
         * But sometimes you get read errors that are okay and so you do not want to
         * disconnect. e.g., Error Code: '5', Error String: 'sensor reading unavailable'
        */
        if(e.getErrorCode() == 16)
        {

            disconnect();

            std::stringstream ss;
            ss << "Could not read sensor for {\n";
            ss << " * Connection-ID: \'" << mConnId << "\'\n";
            ss << " * Hostname: \'" << mHostname << "\'\n";
            ss << " * Entity-Id: \'" << std::to_string(record->get_entity_id()) << "\'\n";
            ss << " * Entity-Instance: \'" << std::to_string(record->get_entity_instance()) << "\'\n";
            ss << " * Sensor-Id-String: \'" << record->get_device_id_string() << "\'\n";
            ss << " * Reason: \'Session Timeout\'" << "\n";
            ss << " * Error Code: \'" << e.getErrorCode() << "\', Error Message: \'" << e.getErrorString() << "\'\n";
            ss << "}\n\n";
            throw std::runtime_error(ss.str());
        }
        else
        {
            //this->disconnect();
            std::stringstream ss;
            ss << "Could not read sensor for {\n";
            ss << " * Connection-ID: \'" << mConnId << "\'\n";
            ss << " * Hostname: \'" << mHostname << "\'\n";
            ss << " * Entity-Id: \'" << std::to_string(record->get_entity_id()) << "\'\n";
            ss << " * Entity-Instance: \'" << std::to_string(record->get_entity_instance()) << "\'\n";
            ss << " * Sensor-Id-String: \'" << record->get_device_id_string() << "\'\n";
            ss << " * Reason: \'" << e.getErrorString() << "\'\n";
            ss << " * Error Code: \'" << e.getErrorCode() << "\', Error Message: \'" << e.getErrorString() << "\'\n";
            ss << "}\n\n";
            throw std::runtime_error(ss.str());
        }
    }
    catch(const std::exception& e)
    {
        std::stringstream ss;
        ss << "Could not read sensor for {\n";
        ss << " * Connection-ID: \'" << mConnId << "\'\n";
        ss << " * Hostname: \'" << mHostname << "\'\n";
        ss << " * Entity-Id: \'" << std::to_string(record->get_entity_id()) << "\'\n";
        ss << " * Entity-Instance: \'" << std::to_string(record->get_entity_instance()) << "\'\n";
        ss << " * Sensor-Id-String: \'" << record->get_device_id_string() << "\'\n";
        ss << " * Reason: " << e.what() << "\n";
        ss << "}\n\n";
        throw std::runtime_error(ss.str());
    }
    
}

void IpmiConnectionManager::write_oem_command(const std::shared_ptr<EntityAddrType> entAddrType, Provider::Entity &entity)
{

    auto [vendorId, command, cmdArgs] = entAddrType->get_oem_command();

    for(auto &key_value : IpmiConnectionManager::oem_cmds)
    {
        auto vendor = std::find(key_value.first.begin(), key_value.first.end(), vendorId);
        if(vendor != key_value.first.end())
        {
            auto cmd = key_value.second.find(command);
            if(cmd != key_value.second.end())
            {
                cmd->second(mIpmiCtx, cmdArgs, entity);
                updateIdleTime();
            }
        }
    }
}

bool IpmiConnectionManager::is_valid_oem_command(const std::string &vendor_id, const std::string &command)
{
     /** Walk thru the map of vendor-ids and vendor-commands.*/
    for(auto &key_value : IpmiConnectionManager::oem_cmds)
    {
        /** See if the vendor name/id/alias is in the supported OEM commands list.
         *  first is a list of names/aliases.
        */
        auto vendor = std::find(key_value.first.begin(), key_value.first.end(), vendor_id);
        if(vendor != key_value.first.end())
        {
            /** See if the vendor command is in the supported OEM commands list.*/
            auto cmd = key_value.second.find(command);
            if(cmd != key_value.second.end())
            {
                return true;
            }
        }
    }
    return false;
}

Provider::Entity IpmiConnectionManager::readSensor(const std::shared_ptr<IpmiSensorRecComp> record)
{
    
    if(mConnState != ConnectionState::CONNECTED)
    {
        std::stringstream ss;
        ss << "Could not read sensor for {\n";
        ss << " * Connection-ID: \'" << mConnId << "\'\n";
        ss << " * Hostname: \'" << mHostname << "\'\n";
        ss << " * Entity-Id: \'" << std::to_string(record->get_entity_id()) << "\'\n";
        ss << " * Entity-Instance: \'" << std::to_string(record->get_entity_instance()) << "\'\n";
        ss << " * Sensor-Id-String: \'" << record->get_device_id_string() << "\'\n";
        ss << " * Reason: Device is disconnected.\n";
        ss << "}\n\n";
        throw std::runtime_error(ss.str());
    }

    if (!mSensorCtx)
    {
        std::stringstream ss;
        ss << "Could not read sensor for {\n";
        ss << " * Connection-ID: \'" << mConnId << "\'\n";
        ss << " * Hostname: \'" << mHostname << "\'\n";
        ss << " * Entity-Id: \'" << std::to_string(record->get_entity_id()) << "\'\n";
        ss << " * Entity-Instance: \'" << std::to_string(record->get_entity_instance()) << "\'\n";
        ss << " * Sensor-Id-String: \'" << record->get_device_id_string() << "\'\n";
        ss << " * Reason: sensor-ctx is null.\n";
        ss << "}\n\n";
        throw std::runtime_error(ss.str());
    }

    Provider::Entity entity;
    uint8_t sharedOffset = 0; // TODO: shared sensors support
    uint8_t readingRaw = 0;
    double* reading = nullptr;
    uint16_t eventMask = 0;
    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &data = record->get_record_data();

    int rv = ipmi_sensor_read(mSensorCtx, data.data, data.size, sharedOffset, &readingRaw, &reading, &eventMask);
    
    if(rv != 1)
    {

        int err_num = ipmi_sensor_read_ctx_errnum (mSensorCtx);
        std::string str_error = ipmi_sensor_read_ctx_strerror (err_num);
        std::string str_errmsg = ipmi_sensor_read_ctx_errormsg (mSensorCtx);

        /** Not sure if ipmi_sensor_read_ctx_strerror() and ipmi_sensor_read_ctx_errormsg()
         * always return the same messages.
         * So, use Lambda to concat strings if they are different...
         */
        auto getErrStr = [&str_error, &str_errmsg]()
        {
            if(str_error.compare(str_errmsg) != 0)
            {
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
    if(IPMI_EVENT_READING_TYPE_CODE_IS_THRESHOLD(record->get_event_reading_type_code()))
    {
        if(reading)
        {
            entity["VAL"] = std::round(*reading * 100.0) / 100.0;
            free(reading);
            getSensorThresholds(entity, record);
            getSensorHysteresis(entity, record);
        }
        else
            entity["VAL"] = (double) eventMask;
    }
    else
    {
        entity["VAL"] = (double) eventMask;
    }
    
    updateIdleTime();
    return entity;

}

void IpmiConnectionManager::getSensorThresholds(Provider::Entity &entity, const std::shared_ptr<IpmiSensorRecComp> record)
{
    
    int rv = (-1);
    if((rv = fiid_obj_clear(mGetSensorThresholdsRq)) < 0)
    {
        throw std::runtime_error("Can't clear get_sensor_threshold_request object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }
    if((rv = fiid_obj_clear(mGetSensorThresholdsRs)) < 0)
    {
        throw std::runtime_error("Can't clear get_sensor_threshold_response object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }

    if((rv = fill_cmd_get_sensor_thresholds (record->get_sensor_number(), mGetSensorThresholdsRq)) < 0)
    {
        throw std::runtime_error("Can't fill get_sensor_threshold_request object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }

    //rv = ipmi_cmd(mIpmiCtx, record->get_sensor_owner_lun(), IPMI_NET_FN_SENSOR_EVENT_RQ, thresh_rq, thresh_rs);
    /** 130 is device-access-addres (65) from Fru Device Locator Record shifted left << 1-bit*/
    uint8_t rs_addr = (record->get_sensor_owner_id() << 1);
    rv = ipmi_cmd_ipmb(mIpmiCtx, record->get_channel_number(), rs_addr, record->get_sensor_owner_lun(),
    IPMI_NET_FN_SENSOR_EVENT_RQ, mGetSensorThresholdsRq, mGetSensorThresholdsRs);
    
    if(rv < 0)
    {
        int errnum = ipmi_ctx_errnum(mIpmiCtx);
        std::string errmsg = ipmi_ctx_errormsg(mIpmiCtx);
        throw IpmiException(errnum, std::move(errmsg));
    }

    uint64_t compCode = 0;
    rv = fiid_obj_get(mGetSensorThresholdsRs, "comp_code", &compCode);

    if(rv < 0)
    {
        throw std::runtime_error("Can't get completion code from get_sensor_threshold_response object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }

    /*
    * See Table 35- Get Sensor Thresholds
    * Byte #2 is a bit mask that indicates which thresholds are
    * readable. Not all are. 
    */

    uint64_t tval = 0;
    int thresh_readable = 0;
    std::list<std::string> threshold_list;
    
    for(int i = 0; i < 6; i++)
    {
        std::string readable = "readable_thresholds." + mThresholdReadables[i];
        if(fiid_obj_get(mGetSensorThresholdsRs, readable.c_str(), &tval) < 0)
        {
            throw std::runtime_error("Can't get \'" + mThresholdReadables[i] +
            "\' from get_sensor_threshold_response object for "
            "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
        }
        thresh_readable |= (tval & 0x01) << i;
        if(tval > 0 && mThresholdsMap.find(mThresholdReadables[i]) != mThresholdsMap.end())
        {
            threshold_list.push_back(mThresholdReadables[i]);
        }
        tval = 0;
    }

    /*
    * See Table 35- Get Sensor Thresholds
    * Just passing the bits back to EPICS in case we need them later.
    */
    entity["THRESHOLDS_READABLE"] = thresh_readable;

    /*
    * There are 6-thresholds in the ipmi standard:
    * + Bit 0 = lower_non_critical_threshold
    * + Bit 1 = lower_critical_threshold
    * + Bit 2 = lower_non_recoverable_threshold
    * + Bit 3 = upper_non_critical_threshold
    * + Bit 4 = upper_critical_threshold
    * + Bit 5 = upper_non_recoverable_threshold
    * 
    * But EPICS only supports four, so we are doing this:
    * + LOLO = lower_critical_threshold
    * + LOW = lower_non_critical_threshold
    * + HIGH = upper_non_critical_threshold
    * + HIHI = upper_critical_threshold
    * 
    */
    for(auto &i : threshold_list)
    {
        if(fiid_obj_get(mGetSensorThresholdsRs, i.c_str(), &tval) < 0)
        {
            throw std::runtime_error("Can't get \'" + i +
            "\' from get_sensor_threshold_response object for "
            "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
        }
        
        auto itr = mThresholdsMap.find(i);
        if(itr != mThresholdsMap.end())
        {
            /** Thresholds are stored in raw values of multiple format types. Have to scale them.*/
            try
            {
                /** Set the precision to 2*/
                std::stringstream dstr;
                dstr << std::fixed << std::setprecision(2) << record->scale_threshold(mSdrCtx, tval);
                double d = 0;
                dstr >> d;
                entity[itr->second.c_str()] = d;
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(std::string(e.what()) + ", for sensor-ID: " + 
                record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
            }
        }
        else
        {
            throw std::runtime_error("Can't find \'" + i + "\' in mThresholdsMap for "
            "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
        }
        tval = 0;
    }
    
}

void IpmiConnectionManager::getSensorHysteresis(Provider::Entity &entity, const std::shared_ptr<IpmiSensorRecComp> record)
{
    int rv = (-1);
    
    if((rv = fiid_obj_clear(mGetSensorHysteresisRq)) < 0)
    {
        throw std::runtime_error("Can't clear get_sensor_hysteresis_request object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }
    
    if((rv = fiid_obj_clear(mGetSensorHysteresisRs)) < 0)
    {
        throw std::runtime_error("Can't clear get_sensor_hysteresis_response object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }

    if((rv = fill_cmd_get_sensor_hysteresis (record->get_sensor_number(), IPMI_SENSOR_HYSTERESIS_MASK, mGetSensorHysteresisRq)) < 0)
    {
        throw std::runtime_error("Can't fill get_sensor_hysteresis_request object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }

    /** 130 is device-access-addres (65) from Fru Device Locator Record shifted left << 1-bit*/
    uint8_t rs_addr = (record->get_sensor_owner_id() << 1);
    rv = ipmi_cmd_ipmb(mIpmiCtx, record->get_channel_number(), rs_addr, record->get_sensor_owner_lun(),
    IPMI_NET_FN_SENSOR_EVENT_RQ, mGetSensorHysteresisRq, mGetSensorHysteresisRs);
    
    if(rv < 0)
    {
        int errnum = ipmi_ctx_errnum(mIpmiCtx);
        std::string errmsg = ipmi_ctx_errormsg(mIpmiCtx);
        throw IpmiException(errnum, std::move(errmsg));
    }

    uint64_t compCode = 0;
    rv = fiid_obj_get(mGetSensorHysteresisRs, "comp_code", &compCode);

    if(rv < 0)
    {
        throw std::runtime_error("Can't get completion code from get_sensor_hysteresis_response object for "
        "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
    }
    /*
    * See table 35, Get Sensor Hysteresis Command
    * There are two values:
    *  + positive_going_threshold_hysteresis_value
    *  + negative_going_threshold_hysteresis_value
    * 
    * But EPICS only supports one hysteresis, HYST, that is applied
    * to both, positive/negative thresholds.
    * 
    * So what do we do if IPMI has different values for positive/negative?
    * In all of the equipment that we are using so far the two positive/negative
    * values are the same.
    * 
    * So I am just going to pick one and use it. positive_going_threshold_hysteresis_value
    * it is!
    * 
    * TODO: Maybe figure out a way to use both and apply them based on the value of the
    * current context.
    */
    uint64_t tval = 0;
    for(auto &hyst_value : mSensorHysteresisValues)
    {
        if(fiid_obj_get(mGetSensorHysteresisRs, hyst_value.c_str(), &tval) < 0)
        {
            throw std::runtime_error("Can't get \'" + hyst_value +
            "\' from get_sensor_hysteresis_response object for "
            "sensor-ID: " + record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
        }
        if(hyst_value.compare("positive_going_threshold_hysteresis_value") == 0)
        {
            try
            {
                ///entity["HYST"] = record->scale_hysteresis(mSdrCtx, tval);
                std::stringstream dstr;
                dstr << std::fixed << std::setprecision(2) << record->scale_threshold(mSdrCtx, tval);
                double d = 0;
                dstr >> d;
                entity["HYST"] = fabs(d);
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(std::string(e.what()) + ", for sensor-ID: " + 
                record->get_entity_id_string() + " for connection id: \'" + mConnId + "\'\n");
            }
        }
        tval = 0;
    }

}

IpmiSdrInfo IpmiConnectionManager::getSdrInfo()
{
    return readSdrInfo();
}