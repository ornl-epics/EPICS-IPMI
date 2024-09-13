/** 
 * 
 * 
 * 
 */

#include "IpmiSdrInfo.h"
#include <stdexcept>

IpmiSdrInfo::IpmiSdrInfo(const std::string &connectionid, fiid_obj_t response)
:mConnId(connectionid)
{
    decode(response);
}

IpmiSdrInfo::IpmiSdrInfo(IpmiSdrInfo &&other) {
    
    *this = std::move(other);
}

IpmiSdrInfo::~IpmiSdrInfo() {

}

IpmiSdrInfo &IpmiSdrInfo::operator=(IpmiSdrInfo &&other) {
    
    if(this != &other) {
        mConnId = std::move(other.mConnId);
        mVersion = other.mVersion;
        mRecordCount = other.mRecordCount;
        mFreeSpace = other.mFreeSpace;
        mAdditionTimestamp = other.mAdditionTimestamp;
        mEraseTimestamp = other.mEraseTimestamp;
        mAllocationCommandSupported = other.mAllocationCommandSupported;
        mReserveCommandSupported = other.mReserveCommandSupported;
        mPartialAddCommandSupported = other.mPartialAddCommandSupported;
        mDeleteCommandSupported = other.mDeleteCommandSupported;
        mReserved = other.mReserved;
        mModalNonModalOperationSupported = other.mModalNonModalOperationSupported;
        mOverflowFlag = other.mOverflowFlag;
    }
    return *this;
}

uint8_t IpmiSdrInfo::getVersion() {
    return mVersion;
}

uint16_t IpmiSdrInfo::getRecordCount() {
    return mRecordCount;
}

uint32_t IpmiSdrInfo::getMostRecentAdditionTimestamp() {
    return mAdditionTimestamp;
}

uint32_t IpmiSdrInfo::getMostRecentEraseTimestamp() {
    return mEraseTimestamp;
}

bool IpmiSdrInfo::getAllocationCommandSupported() {
    return mAllocationCommandSupported;
}

bool IpmiSdrInfo::getReserveCommandSupported() {
    return mReserveCommandSupported;
}

bool IpmiSdrInfo::getPartialAddCommandSupported() {
    return mPartialAddCommandSupported;
}

bool IpmiSdrInfo::getDeleteCommandSupported() {
    return mDeleteCommandSupported;
}

uint8_t IpmiSdrInfo::getModalNonModalOperationSupported() {
    return mModalNonModalOperationSupported;
}

bool IpmiSdrInfo::getOverflowFlag() {
    return mOverflowFlag;
}

void IpmiSdrInfo::decode(fiid_obj_t response) {

    int rv = -1;
    uint64_t tval = 0;

    if((rv = fiid_obj_get(response, "sdr_version_major", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'sdr_version_major\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mVersion = ((uint8_t) tval << 4);

    tval = 0;
    if((rv = fiid_obj_get(response, "sdr_version_minor", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'sdr_version_minor\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }
   
    mVersion |= tval;

    tval = 0;
    if((rv = fiid_obj_get(response, "record_count", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'record_count\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }
    
    mRecordCount = (uint16_t) tval;

    tval = 0;
    if((rv = fiid_obj_get(response, "free_space", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'free_space\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mFreeSpace = (uint16_t) tval;

    tval = 0;
    if((rv = fiid_obj_get(response, "most_recent_addition_timestamp", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'most_recent_addition_timestamp\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mAdditionTimestamp = tval;

    tval = 0;
    if((rv = fiid_obj_get(response, "most_recent_erase_timestamp", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'most_recent_erase_timestamp\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mEraseTimestamp = tval;

    tval = 0;
    if((rv = fiid_obj_get(response, "get_sdr_repository_allocation_info_command_supported", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'get_sdr_repository_allocation_info_command_supported\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mAllocationCommandSupported = tval ? true : false;

    tval = 0;
    if((rv = fiid_obj_get(response, "reserve_sdr_repository_command_supported", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'reserve_sdr_repository_command_supported\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mReserveCommandSupported = tval ? true : false;

    tval = 0;
    if((rv = fiid_obj_get(response, "partial_add_sdr_command_supported", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'partial_add_sdr_command_supported\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mPartialAddCommandSupported = tval ? true : false;

    tval = 0;
    if((rv = fiid_obj_get(response, "delete_sdr_command_supported", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'delete_sdr_command_supported\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mDeleteCommandSupported = tval ? true : false;

   tval = 0;
    if((rv = fiid_obj_get(response, "modal_non_modal_sdr_repository_update_operation_supported", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'modal_non_modal_sdr_repository_update_operation_supported\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mModalNonModalOperationSupported = (tval & 0x3);

    tval = 0;
    if((rv = fiid_obj_get(response, "overflow_flag", &tval)) < 1) {
        throw std::runtime_error("Can't decode SDR info field \'overflow_flag\' for connection id: \'" + mConnId +
        "\', Reason: fiid_obj_get() returned \'" + std::to_string(rv) + "\'\n");
    }

    mOverflowFlag = tval ? true : false;
    
}