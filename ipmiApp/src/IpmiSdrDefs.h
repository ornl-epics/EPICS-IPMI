/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_IPMISDRDEFS_H_
#define IPMIAPP_SRC_IPMISDRDEFS_H_

#include <map>
#include <freeipmi/freeipmi.h>

static std::map<uint8_t, std::string> sdr_type_itos_map {
	{IPMI_SDR_FORMAT_FULL_SENSOR_RECORD, "IPMI_SDR_FORMAT_FULL_SENSOR_RECORD"},
	{IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD, "IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD"},
	{IPMI_SDR_FORMAT_EVENT_ONLY_RECORD, "IPMI_SDR_FORMAT_EVENT_ONLY_RECORD"},
	{IPMI_SDR_FORMAT_ENTITY_ASSOCIATION_RECORD, "IPMI_SDR_FORMAT_ENTITY_ASSOCIATION_RECORD"},
	{IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD, "IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD"},
	{IPMI_SDR_FORMAT_GENERIC_DEVICE_LOCATOR_RECORD, "IPMI_SDR_FORMAT_GENERIC_DEVICE_LOCATOR_RECORD"},
	{IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD, "IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD"},
	{IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD, "IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD"},
	{IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_CONFIRMATION_RECORD, "IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_CONFIRMATION_RECORD"},
	{IPMI_SDR_FORMAT_BMC_MESSAGE_CHANNEL_INFO_RECORD, "IPMI_SDR_FORMAT_BMC_MESSAGE_CHANNEL_INFO_RECORD"},
	{IPMI_SDR_FORMAT_OEM_RECORD, "IPMI_SDR_FORMAT_OEM_RECORD"}
};

static std::map<uint8_t, std::string> sdr_sensor_owner_id_type_itos_map {
    {IPMI_SDR_SENSOR_OWNER_ID_TYPE_IPMB_SLAVE_ADDRESS, "IPMB_SLAVE_ADDRESS"},
    {IPMI_SDR_SENSOR_OWNER_ID_TYPE_SYSTEM_SOFTWARE_ID, "SYSTEM_SOFTWARE_ID"}
};

static std::map<uint8_t, std::string> sensor_event_reading_type_code_itos_map {
    {IPMI_EVENT_READING_TYPE_CODE_UNSPECIFIED, "Unspecified"},
    {IPMI_EVENT_READING_TYPE_CODE_THRESHOLD, "Threshold"},
    {IPMI_EVENT_READING_TYPE_CODE_TRANSITION_STATE, "Transition State"},
    {IPMI_EVENT_READING_TYPE_CODE_STATE, "State"},
    {IPMI_EVENT_READING_TYPE_CODE_PREDICTIVE_FAILURE, "Predictive Failure"},
    {IPMI_EVENT_READING_TYPE_CODE_LIMIT, "Limit"},
    {IPMI_EVENT_READING_TYPE_CODE_PERFORMANCE, "Performance"},
    {IPMI_EVENT_READING_TYPE_CODE_TRANSITION_SEVERITY, "Transition Severity"},
    {IPMI_EVENT_READING_TYPE_CODE_DEVICE_PRESENT, "Device Present"},
    {IPMI_EVENT_READING_TYPE_CODE_DEVICE_ENABLED, "Device Enabled"},
    {IPMI_EVENT_READING_TYPE_CODE_TRANSITION_AVAILABILITY, "Transition Availability"},
    {IPMI_EVENT_READING_TYPE_CODE_REDUNDANCY, "Redundancy"},
    {IPMI_EVENT_READING_TYPE_CODE_ACPI_POWER_STATE, "ACI Power State"},
    {IPMI_EVENT_READING_TYPE_CODE_SENSOR_SPECIFIC, "Sensor-Specific"},
    {IPMI_EVENT_READING_TYPE_CODE_OEM_MIN, "OEM Min"},
    {IPMI_EVENT_READING_TYPE_CODE_OEM_MAX, "OEM Max"}
};

static std::string get_sensor_event_reading_type_code(uint8_t val) {
    std::string s;

    if(val >= IPMI_EVENT_READING_TYPE_CODE_OEM_MIN && val <= IPMI_EVENT_READING_TYPE_CODE_OEM_MAX) {
        return s + "(OEM)";
    }
    else if(val == IPMI_EVENT_READING_TYPE_CODE_SENSOR_SPECIFIC) {
        return sensor_event_reading_type_code_itos_map[IPMI_EVENT_READING_TYPE_CODE_SENSOR_SPECIFIC];
    }
    else if(val >= IPMI_EVENT_READING_TYPE_CODE_UNSPECIFIED && val <= IPMI_EVENT_READING_TYPE_CODE_ACPI_POWER_STATE) {
        if(IPMI_EVENT_READING_TYPE_CODE_IS_GENERIC(val)) {
            return s + "(Generic/Discrete) " + sensor_event_reading_type_code_itos_map[val];
        }
        else
            return s + sensor_event_reading_type_code_itos_map[val];
    }
    return s + "Unknown";
}

static std::map<uint8_t, std::string> authenticationMap {
    {0, "None"},
    {1, "MD2"},
    {2, "MD5"},
    {4, "Straight-Password-Key"},
    {5, "OEM Prop."},
    {6, "RMCPPLUS"}
};

static std::string getAuthenticationString(uint8_t authType) {
    std::map<uint8_t, std::string>::iterator itr;
    itr = authenticationMap.find(authType);
    if(itr != authenticationMap.end()) {
        return itr->second;
    }
    return "Invalid-Authentication-Type";
}

static std::map<uint8_t, std::string> privilegeLevelMap {
    {0, "Unspecified"},
    {1, "Callback"},
    {2, "User"},
    {3, "Operator"},
    {4, "Admin"},
    {5, "OEM"},
    {15, "No-Access"}
};

static std::string getPrivilegeLevelString(uint8_t level) {
    std::map<uint8_t, std::string>::iterator itr;
    itr = privilegeLevelMap.find(level);
    if(itr != privilegeLevelMap.end()) {
        return itr->second;
    }
    return "Invalid-Level";
}


#endif
