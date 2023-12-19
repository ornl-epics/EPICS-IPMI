/** 
 * 
 * 
 * 
 */

#include <string>
#include <cstdint>
#include <freeipmi/freeipmi.h>
#include <epicsTime.h>
#include "common.h"
#include "provider.h"
#include "IpmiException.h"
#include "IpmiSdrInfo.h"

#ifndef IPMIAPP_SRC_CONNECTIONMANAGER_H_
#define IPMIAPP_SRC_CONNECTIONMANAGER_H_

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
    const std::string mCacheFilePath;
    bool mCacheFileIsOpen{false};
    bool mConnStatus{false};
    epicsTime mIdleTime;
    fiid_obj_t mSdrRepositoryInfoRq{nullptr};
    fiid_obj_t mSdrRepositoryInfoRs{nullptr};

    void createIpmiContext();
    void createSdrContext();
    void createSensorContext();
    void openSdrCache();
    void connect();
    void cleanup();
    void keepAlive();
    IpmiSdrInfo readSdrInfo();
    
    uint8_t initAuthtype(const std::string &authenticationtype, const std::string &username);
    uint8_t initPrivLevel(const std::string &privlegelevel);
    
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

    Provider::Entity readSensor(const std::shared_ptr<IpmiSensorRecComp> record);
    IpmiSdrInfo getSdrInfo();
    void rebuildSdrCache();
    
};

#endif ///IPMIAPP_SRC_CONNECTIONMANAGER_H_