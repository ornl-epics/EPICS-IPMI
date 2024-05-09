/** 
 * 
 * 
 * 
 */

#include <filesystem>
#include <string>
#include <cstdint>
#include <map>
#include <list>
#include <functional>
#include <freeipmi/freeipmi.h>
#include <epicsTime.h>
#include "common.h"
#include "provider.h"
#include "IpmiException.h"
#include "IpmiSdrInfo.h"


#ifndef IPMIAPP_SRC_CONNECTIONMANAGER_H_
#define IPMIAPP_SRC_CONNECTIONMANAGER_H_

namespace fs = std::filesystem;

enum class ConnectionState {
    DISCONNECTED,
    CONNECTED
};

class IpmiConnectionManager
{
private:

    const std::string mConnId;
    const std::string mHostname;
    const std::string mUserName;
    const std::string mPassword;
    const uint8_t mAuthtype;
    const uint8_t mPrivlevel;
    const std::string mProtocol;

    unsigned int mSessionTimeout{IPMI_SESSION_TIMEOUT_DEFAULT};
    unsigned int mRetransmissionTimeout{IPMI_RETRANSMISSION_TIMEOUT_DEFAULT};
    int mCipherSuiteId{3};
    int m_k_g_len{0};
    unsigned char* m_k_g{nullptr};
    unsigned int mWorkaroundFlags{1};
    unsigned int mFlags{IPMI_FLAGS_DEFAULT};

    ipmi_ctx_t mIpmiCtx{nullptr};
    ipmi_sdr_ctx_t mSdrCtx{nullptr};
    ipmi_sensor_read_ctx_t mSensorCtx{nullptr};
    const fs::path mCachePath;
    const fs::path mCacheFile;
    bool mCacheFileIsOpen{false};
    bool mConnStatus{false};
    epicsTime mIdleTime;
    epicsTime mDisconnectTime;
    fiid_obj_t mSdrRepositoryInfoRq{nullptr};
    fiid_obj_t mSdrRepositoryInfoRs{nullptr};
    fiid_obj_t mGetSensorThresholdsRq{nullptr};
    fiid_obj_t mGetSensorThresholdsRs{nullptr};
    fiid_obj_t mGetSensorHysteresisRq{nullptr};
    fiid_obj_t mGetSensorHysteresisRs{nullptr};
    ConnectionState mConnState{ConnectionState::DISCONNECTED};
    static const std::map<std::string, std::string> mThresholdsMap;
    static const std::string mThresholdReadables [];
    static const std::string mSensorHysteresisValues [];
    typedef int (*OEM_HANDLER)(ipmi_ctx_t ctx, const std::vector<std::string> &args, Provider::Entity &value);
    static std::map<std::list<std::string>, std::map<std::string, OEM_HANDLER>> oem_cmds;
    static std::map<std::string, uint8_t> VADATECH_SITE_TYPES;
    static const uint8_t VADATECH_IPMB_ADDRESS {0x82};

    void createIpmiContext();
    void createSdrContext();
    void createSensorContext();
    void openSdrCache();
    void connect();
    void disconnect();
    void reconnect();
    void cleanup();
    void keepAlive();
    IpmiSdrInfo readSdrInfo();
    void updateIdleTime();
    
    uint8_t initAuthtype(const std::string &authenticationtype, const std::string &username);
    uint8_t initPrivLevel(const std::string &privlegelevel);

    Provider::Entity readSensor(const std::shared_ptr<IpmiSensorRecComp> record);
    void getSensorThresholds(Provider::Entity &entity, const std::shared_ptr<IpmiSensorRecComp> record);
    void getSensorHysteresis(Provider::Entity &entity, const std::shared_ptr<IpmiSensorRecComp> record);
    static int vadatech_reboot_chassis(ipmi_ctx_t ctx, const std::vector<std::string> &args, Provider::Entity &entity);
    static int vadatech_set_power_state(ipmi_ctx_t ctx, const std::vector<std::string> &args, Provider::Entity &entity);
    static int send_ipmi_cmd_raw_ipmb(ipmi_ctx_t ctx, uint8_t channel_number, uint8_t rs_addr,
                uint8_t lun, uint8_t net_fn, const void *buf_rq, unsigned int buf_rq_len,
                void *buf_rs, unsigned int buf_rs_len);
    
public:
    IpmiConnectionManager(const std::string &connectionid, const std::string &hostname,
    const std::string &username, const std::string &password,
    const std::string &authtype, const std::string &protocol,
    const std::string &privilegelevel);
    ~IpmiConnectionManager();

    
    ipmi_ctx_t getIpmiCtx();
    ipmi_sdr_ctx_t getSdrCtx();
    const std::string &getConnectionId() const;
    const std::string &getHostname() const;
    void process();
    bool isConnected();

    Provider::Entity getSensorReading(const std::shared_ptr<IpmiSensorRecComp> record);
    ///void write_oem_command(const std::string &connectionId, const std::string vendorId, const std::string command);
    void write_oem_command(const std::shared_ptr<EntityAddrType> entAddrType, Provider::Entity &entity);
    static bool is_valid_oem_command(const std::string &vendor_id, const std::string &command);
    
    IpmiSdrInfo getSdrInfo();
    void rebuildSdrCache();
    
    
};

#endif ///IPMIAPP_SRC_CONNECTIONMANAGER_H_