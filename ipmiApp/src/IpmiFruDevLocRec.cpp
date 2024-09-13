/**
 * 
 * 
 * 
 * 
*/

#include "IpmiFruDevLocRec.h"
#include "IpmiSdrDefs.h"
#include <sstream>
#include <iostream>

#define IPMI_NET_FN_PICMG_RQ IPMI_NET_FN_GROUP_EXTENSION_RQ
#define IPMI_NET_FN_PICMG_RS IPMI_NET_FN_GROUP_EXTENSION_RS

enum {
    PICMG_GET_PICMG_PROPERTIES_CMD             = 0x00,
    PICMG_GET_ADDRESS_INFO_CMD                 = 0x01,
    PICMG_GET_SHELF_ADDRESS_INFO_CMD           = 0x02,
    PICMG_SET_SHELF_ADDRESS_INFO_CMD           = 0x03,
    PICMG_FRU_CONTROL_CMD                      = 0x04,
    PICMG_GET_FRU_LED_PROPERTIES_CMD           = 0x05,
    PICMG_GET_FRU_LED_COLOR_CAPABILITIES_CMD   = 0x06,
    PICMG_SET_FRU_LED_STATE_CMD                = 0x07,
    PICMG_GET_FRU_LED_STATE_CMD                = 0x08,
    PICMG_SET_IPMB_CMD                         = 0x09,
    PICMG_SET_FRU_POLICY_CMD                   = 0x0A,
    PICMG_GET_FRU_POLICY_CMD                   = 0x0B,
    PICMG_FRU_ACTIVATION_CMD                   = 0x0C,
    PICMG_GET_DEVICE_LOCATOR_RECORD_CMD        = 0x0D,
    PICMG_SET_PORT_STATE_CMD                   = 0x0E,
    PICMG_GET_PORT_STATE_CMD                   = 0x0F,
    PICMG_COMPUTE_POWER_PROPERTIES_CMD         = 0x10,
    PICMG_SET_POWER_LEVEL_CMD                  = 0x11,
    PICMG_GET_POWER_LEVEL_CMD                  = 0x12,
    PICMG_RENEGOTIATE_POWER_CMD                = 0x13,
    PICMG_GET_FAN_SPEED_PROPERTIES_CMD         = 0x14,
    PICMG_SET_FAN_LEVEL_CMD                    = 0x15,
    PICMG_GET_FAN_LEVEL_CMD                    = 0x16,
    PICMG_BUSED_RESOURCE_CMD                   = 0x17,
};

/**
 * When calling the two funcions below on the VadaTech equipment with the FRU names
 * in the exclusion list, they will throw errors. Their errors are not bad because
 * we are looking for LEDs on FRUs and the FRUs in the list are not physical hardware
 * with LEDs. And the one named "TELCO ALARM" will actually not return an error
 * right away but will instead timeout which hangs the process for a few seconds
 * and this is annoying to watch during IOC startup. So, this is just a way for now to
 * silence the errors on startup so the IOC engineer doesn't think there is a problem
 * When they see the boot process hang and display errors about bad completion codes.
 * 
 * I can't find another way to exclude asking these guys if they have LEDs as of yet.
 * And what I mean is that I cannot see a way in software to distinguish between a
 * physical FRU device (e.g., AMC523) that has LEDs and a software FRU device 
 * (e.g., TELCO ALARM) that does not have LEDs.
 * 
 * (1) ipmi_cmd(ipmi, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_PICMG_RQ, obj_cmd_rq, obj_cmd_rs);
 * (2) ipmi_ctx_set_target(ipmi, &this->channel_number, &deviceAddr);
 * 
*/
const std::vector<std::string> IpmiFruDevLocRec::picmgLedExclusionList = {
    "SHELF FRU INFO",
    "UTCA CARRIER",
    "SH FRU DEV1",
    "SH FRU DEV2",
    "MCH DA INFO",
    "BMC FRU",
    "TELCO ALARM"
};

bool IpmiFruDevLocRec::isInExclusionList(const std::string &name) {
    for(auto &exName : picmgLedExclusionList) {
        if(name == exName) {return true;};
    }
    return false;
}

IpmiFruDevLocRec::IpmiFruDevLocRec(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr, uint16_t recid, uint8_t rectype)
    :IpmiSdrRec(recid, rectype)
{
    if(rectype != IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
	std::stringstream ss;
	ss << "ERROR: Invalid record type passed to FruDevLocatorRec::FruDevLocatorRec()." <<
	     "Expected \'" << sdr_type_itos_map[IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD] <<
	     "\' but received \'" << sdr_type_itos_map[rectype] << "\'" << std::endl;
        throw std::invalid_argument(ss.str());
    }

    int rv = (-1);
    rv = ipmi_sdr_parse_fru_entity_id_and_instance (sdr, NULL, 0, &this->fru_entity_id, &this->fru_entity_instance);

    rv = ipmi_sdr_parse_fru_device_locator_parameters (sdr, NULL, 0,
                    &this->device_access_address,
                    &this->logical_fru_device_device_slave_address,
                    &this->private_bus_id,
                    &this->lun_for_master_write_read_fru_command,
                    &this->logical_physical_fru_device,
                    &this->channel_number);

    char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
    rv = ipmi_sdr_parse_device_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);
    this->device_id_string = id_str;

    /** Does this FRU device have LEDs? */
    try
    {
        if(! isInExclusionList(this->device_id_string)) {
            getPicmgStatusLeds(ipmi);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << this->device_id_string << " [" << (unsigned) this->logical_fru_device_device_slave_address
        << "] " << e.what() << '\n';
        if (ipmi_ctx_set_target(ipmi, NULL, NULL) < 0) {
            std::cerr << "Failed to reset target IPMI address - " << std::string(ipmi_ctx_errormsg(ipmi));
        }
    }

}

IpmiFruDevLocRec::~IpmiFruDevLocRec()
{
}

std::tuple<fiid_obj_t, fiid_obj_t> getLedPropReqRsp(uint8_t _logical_fru_device_device_slave_address) {

    static fiid_template_t tmpl_cmd_get_picmg_led_prop_rq =
    {
        { 8, "cmd", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "picmg_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "fru_device_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 0, "", 0}
    };

    static fiid_template_t tmpl_cmd_get_picmg_led_prop_rs =
    {
        { 8, "cmd", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED | FIID_FIELD_MAKES_PACKET_SUFFICIENT},
        { 8, "comp_code", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED | FIID_FIELD_MAKES_PACKET_SUFFICIENT},
        { 8, "picmg_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "status_leds", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "led_reserved", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "app_leds", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 0, "", 0}
    };

    /** Create the request object */
    fiid_obj_t obj_cmd_rq = fiid_obj_create(tmpl_cmd_get_picmg_led_prop_rq);

    /** Check if the request object is valid */
    if(!fiid_obj_valid(obj_cmd_rq))
        obj_cmd_rq = nullptr;

    /** Create the response object */
    fiid_obj_t obj_cmd_rs = fiid_obj_create(tmpl_cmd_get_picmg_led_prop_rs);

    /** Check if the response object is valid */
    if(!fiid_obj_valid(obj_cmd_rs))
        obj_cmd_rs = nullptr;

    if (obj_cmd_rq == nullptr)
        throw std::runtime_error("Failed to allocate PICMG LED request");

    if (obj_cmd_rs == nullptr)
        throw std::runtime_error("Failed to allocate PICMG LED response");

    if (fiid_obj_set(obj_cmd_rq, "cmd", PICMG_GET_FRU_LED_PROPERTIES_CMD) < 0)
        throw std::runtime_error("Failed to initialize PICMG LED request for \'cmd\' field");
    if (fiid_obj_set(obj_cmd_rq, "picmg_id", IPMI_NET_FN_GROUP_EXTENSION_IDENTIFICATION_PICMG) < 0)
        throw std::runtime_error("Failed to initialize PICMG LED request for \'picmg_id\' field");
    if (fiid_obj_set(obj_cmd_rq, "fru_device_id", _logical_fru_device_device_slave_address) < 0)
        throw std::runtime_error("Failed to initialize PICMG LED request for \'fru_device_id\' field");
    
    return {obj_cmd_rq, obj_cmd_rs};
}

std::string getLedColorsReqRsp(ipmi_ctx_t ipmi, uint8_t _logical_fru_device_device_slave_address, uint8_t _led_id) {

    static fiid_template_t tmpl_cmd_get_picmg_led_cap_rq =
    {
        { 8, "cmd", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "picmg_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "fru_device_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "led_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 0, "", 0}
    };

    static fiid_template_t tmpl_cmd_get_picmg_led_cap_rs =
    {
        { 8, "cmd", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED | FIID_FIELD_MAKES_PACKET_SUFFICIENT},
        { 8, "comp_code", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED | FIID_FIELD_MAKES_PACKET_SUFFICIENT},
        { 8, "picmg_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "colors", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "local_control_default", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "override_control_default", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "flags", FIID_FIELD_OPTIONAL | FIID_FIELD_LENGTH_FIXED},
        { 0, "", 0}
    };

    /** Create the request object */
    fiid_obj_t obj_cmd_rq = fiid_obj_create(tmpl_cmd_get_picmg_led_cap_rq);

    /** Check if the request object is valid */
    if(!fiid_obj_valid(obj_cmd_rq))
        obj_cmd_rq = nullptr;

    /** Create the response object */
    fiid_obj_t obj_cmd_rs = fiid_obj_create(tmpl_cmd_get_picmg_led_cap_rs);

    /** Check if the response object is valid */
    if(!fiid_obj_valid(obj_cmd_rs))
        obj_cmd_rs = nullptr;

    if (obj_cmd_rq == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED request");
    if (obj_cmd_rs == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED response");
    
    if (fiid_obj_set(obj_cmd_rq, "cmd", PICMG_GET_FRU_LED_COLOR_CAPABILITIES_CMD) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(obj_cmd_rq, "picmg_id", IPMI_NET_FN_GROUP_EXTENSION_IDENTIFICATION_PICMG) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(obj_cmd_rq, "fru_device_id", _logical_fru_device_device_slave_address) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
        
    if (fiid_obj_set(obj_cmd_rq, "led_id", _led_id) < 0)
    throw std::runtime_error("failed to initialize PICMG LED request");

    int ret = ipmi_cmd(ipmi, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_PICMG_RQ, obj_cmd_rq, obj_cmd_rs);
    if (ret < 0)
        throw std::runtime_error("failed to request PICMG LED capabilities");

    uint64_t compCode;
    if (fiid_obj_get(obj_cmd_rs, "comp_code", &compCode) < 0)
        throw std::runtime_error("failed to decode PICMG LED capabilities response");
    if (compCode != 0)
        throw std::runtime_error("failed to decode PICMG LED capabilities response, invalid comp_code");

    uint64_t val;
    if (fiid_obj_get(obj_cmd_rs, "colors", &val) < 0)
        throw std::runtime_error("failed to decode PICMG LED capabilities response");
    ///val |= 0x1; // Force 'off' color to be part of the options

    static const std::vector<std::string> colors = {
        "off", "blue", "red", "green", "amber", "orange", "white"
    };

    int pos = 0;
    for (int i = 0; i < 7; i++) {
        if (val & (1 << i)) {
            pos = i;
        }
    }

    return colors[pos];

}

void IpmiFruDevLocRec::getPicmgStatusLeds(ipmi_ctx_t ipmi) {

    auto [obj_cmd_rq, obj_cmd_rs] = getLedPropReqRsp(this->logical_fru_device_device_slave_address);
    
    /** The device adder is shifted to bit-7 in use for these commands.*/
    uint8_t deviceAddr = (this->device_access_address << 1);
    
    if (ipmi_ctx_set_target(ipmi, &this->channel_number, &deviceAddr) < 0) {
        throw std::runtime_error("Failed to set target IPMI address - " + std::string(ipmi_ctx_errormsg(ipmi)));
    }

    int ret = ipmi_cmd(ipmi, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_PICMG_RQ, obj_cmd_rq, obj_cmd_rs);
    if (ret < 0) {
        throw std::runtime_error("ERROR! IPMI command returned a failure code for object command req/rsp: " + 
        std::string(ipmi_ctx_errormsg(ipmi)));
    }
    
    /** Completion code. 00h is good.*/
    uint64_t compCode;
    if (fiid_obj_get(obj_cmd_rs, "comp_code", &compCode) < 0)
        throw std::runtime_error("Failed to get object \'comp_code\' from response for PICMG LED properties");
    if (compCode != 0)
        throw std::runtime_error("ERROR! Unsuccessfull completion code \'" + std::to_string(compCode) + "\' "
        "returned for get \'comp_code\' command response for PICMG LED properties");

    uint64_t statusLeds;
    uint64_t appLeds;
    if (fiid_obj_get(obj_cmd_rs, "status_leds", &statusLeds) < 0)
        throw std::runtime_error("Failed to get object \'status_leds\' from response for PICMG LED properties");
    if (fiid_obj_get(obj_cmd_rs, "app_leds", &appLeds) < 0)
        throw std::runtime_error("Failed to get object \'app_leds\' from response for PICMG LED properties");

    /** If we have LEDs then get the led colors*/
    if(statusLeds) {
        for(int i=0; i<4; i++) {
            if(statusLeds & (1 << i)) {
                std::string color = getLedColorsReqRsp(ipmi, this->logical_fru_device_device_slave_address, i);
                this->m_StatusLeds.push_back(std::make_shared<PicmgLed>(this->device_access_address,
                this->channel_number, this->logical_fru_device_device_slave_address, i, color));
            }
        }
    }

    /** Do this last maybe? 
     * Reset the target address back to null */
    if (ipmi_ctx_set_target(ipmi, NULL, NULL) < 0) {
            throw std::runtime_error("Failed to reset target IPMI address - " + std::string(ipmi_ctx_errormsg(ipmi)));
    }

}


std::string IpmiFruDevLocRec::report() {
    std::stringstream ss;
    ss << "FRU "<< (unsigned int) this->logical_fru_device_device_slave_address <<
        " \'" << this->device_id_string << "\' " << 
        "[" << (unsigned int) this->device_access_address <<
        ", " << (unsigned int) this->private_bus_id <<
        ", " << (unsigned int) this->lun_for_master_write_read_fru_command <<
        ", " << (unsigned int) this->logical_physical_fru_device <<
        ", " << (unsigned int) this->channel_number <<
        ", " << (unsigned int) this->fru_entity_id <<
        ", " << (unsigned int) this->fru_entity_instance << "]\n";

    if(this->m_StatusLeds.size() > 0) {
        ss << "PICMG_LEDS attached (" << this->m_StatusLeds.size() << ")\n";
        int count = 1;
        for(auto &led : this->m_StatusLeds) {
            ss << "[" << (unsigned int) this->logical_fru_device_device_slave_address << ":" << count << "]\n";
            ss << " * LED Color: \'" << led.get()->getLedColor() << "\',\n";
            ss << " * LED ID/Address: \'" << (unsigned) led.get()->getLedId() << "\'\n\n";
            count++;
        }
    }
    if(this->sensor_records.size() > 0) {
        ss << "Sensors attched (" << (unsigned int) this->sensor_records.size() << ")\n";
        int count = 1;
        for(auto &rec : this->sensor_records) {
            ss << "[" << (unsigned int) this->logical_fru_device_device_slave_address << ":" << count << "]\n";
            ss << rec->to_string();
            count++;
        }
    }
    return ss.str() + "\n";
}

template<typename T>
void IpmiFruDevLocRec::parseAssociations(std::vector<std::shared_ptr<T>> &sensor_list) {

    auto itr = sensor_list.begin();
    while(itr != sensor_list.end()) {
        auto pos = *itr;
        if(itr->get()->get_entity_id() == this->fru_entity_id && itr->get()->get_entity_instance() == this->fru_entity_instance) {
            this->sensor_records.push_back(*itr);
            itr = sensor_list.erase(itr);
        }
        /** TODO: Make a note about how fans and cooling unit are missing from the device relative association record.*/
        else if(this->fru_entity_id == IPMI_ENTITY_ID_COOLING_UNIT_COOLING_DOMAIN && itr->get()->get_sensor_type() == IPMI_SENSOR_TYPE_FAN) {
            if(itr->get()->get_entity_instance() == this->fru_entity_instance) {
                this->sensor_records.push_back(*itr);
                itr = sensor_list.erase(itr);
            }
        }

        if(*itr == pos && itr != sensor_list.end()) {
            itr++;
        }
    }
}

template void IpmiFruDevLocRec::parseAssociations(std::vector<std::shared_ptr<IpmiSensorRecComp>> &sensor_list);
template void IpmiFruDevLocRec::parseAssociations(std::vector<std::shared_ptr<IpmiSensorRecFull>> &sensor_list);

uint8_t IpmiFruDevLocRec::get_device_slave_address() {
    return this->logical_fru_device_device_slave_address;
}

uint8_t IpmiFruDevLocRec::get_channel_number() const {
    return this->channel_number;
}

uint8_t IpmiFruDevLocRec::get_logical_physical_fru_device() const {
    return this->logical_physical_fru_device;
}

uint8_t IpmiFruDevLocRec::get_device_access_address() const {
    return this->device_access_address;
}

std::shared_ptr<IpmiSensorRecComp> IpmiFruDevLocRec::get_sensor_by_sensor_number(uint8_t number) const {
    for(auto &sens : this->sensor_records) {
        if(sens->get_sensor_number() == number) {
            return sens;
        }
    }
    std::stringstream ss;
    ss << "Sensor number \'" << (unsigned) number << "\' not found!\n";
    throw std::invalid_argument(ss.str());
}

std::shared_ptr<IpmiSensorRecComp> IpmiFruDevLocRec::get_sensor_by_sensor_id_string(const std::string &idStr) const {
    for(auto &sens : this->sensor_records) {
        if(sens->get_device_id_string() == idStr) {
            return sens;
        }
    }
    std::stringstream ss;
    ss << "Sensor ID-String \'" << idStr << "\' not found!\n";
    throw std::invalid_argument(ss.str());
}

std::vector<std::shared_ptr<IpmiSensorRecComp>> &IpmiFruDevLocRec::get_sensors() {
    return this->sensor_records;
}

std::vector<std::shared_ptr<PicmgLed>> IpmiFruDevLocRec::getStatusLeds() {
    return this->m_StatusLeds;
}

std::shared_ptr<PicmgLed> IpmiFruDevLocRec::getStatusLedById(uint8_t led_id) {
    for(auto &led : this->m_StatusLeds) {
        if(led.get()->getLedId() == led_id) {
            return led;
        }
    }
    return nullptr;
}