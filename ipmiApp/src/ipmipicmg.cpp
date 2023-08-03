/* ipmisensor.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Mar 2019
 */

#include "freeipmiprovider.h"

#include <alarm.h> // from EPICS
#include <cmath>

#include <freeipmi/fiid/fiid.h>

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

class FiidScoped {
    public:
        fiid_obj_t raw;
        FiidScoped(fiid_template_t tmpl)
        {
            raw = fiid_obj_create(tmpl);
            if (!fiid_obj_valid(raw)) {
                raw = nullptr;
                return;
            }
            fiid_obj_clear(raw);
        }
        ~FiidScoped()
        {
            if (raw != nullptr) {
                fiid_obj_destroy(raw);
            }
        }
        fiid_obj_t operator*()
        {
            return raw;
        }
};

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getPicmgLeds(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr)
{
    if (ipmi_sdr_cache_first(sdr) < 0)
        throw std::runtime_error("failed to rewind SDR cache - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    std::vector<FreeIpmiProvider::Entity> leds;
    do {
        uint8_t recordType;
        if (ipmi_sdr_parse_record_id_and_type(sdr, NULL, 0, NULL, &recordType) < 0) {
            LOG_DEBUG("Failed to parse SDR record type - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
            continue;
        }

        if (recordType != IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD)
            continue;

        SdrRecord record;
        record.size = ipmi_sdr_cache_record_read(sdr, record.data, record.max_size);
        if (record.size < 0) {
            LOG_DEBUG("Failed to read SDR record - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
            continue;
        }

        try {
            FruAddress fruAddress(sdr, record);
            auto fruName = getFruName(sdr, record);
            auto subleds = getPicmgLeds(ipmi, fruAddress, fruName);
            for (auto& led: subleds) {
                leds.emplace_back(std::move(led));
            }
        } catch (std::runtime_error& e) {
            LOG_DEBUG(std::string(e.what()) + ", skipping");
        }
    } while (ipmi_sdr_cache_next(sdr) == 1);

    return leds;
}

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getPicmgLeds(ipmi_ctx_t ipmi, const FruAddress& fruAddress, const std::string& namePrefix)
{
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

    FiidScoped obj_cmd_rq(tmpl_cmd_get_picmg_led_prop_rq);
    FiidScoped obj_cmd_rs(tmpl_cmd_get_picmg_led_prop_rs);

    if (*obj_cmd_rq == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED request");

    if (fiid_obj_set(*obj_cmd_rq, "cmd", PICMG_GET_FRU_LED_PROPERTIES_CMD) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "picmg_id", IPMI_NET_FN_GROUP_EXTENSION_IDENTIFICATION_PICMG) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "fru_device_id", fruAddress.fruId) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");

    if (*obj_cmd_rs == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED response");

//ipmi_ctx_set_flags(ipmi, IPMI_FLAGS_DEBUG_DUMP);
    IpmbBridgeScoped bridge(ipmi, fruAddress.deviceAddr, fruAddress.channel);
    int ret = ipmi_cmd(ipmi, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_PICMG_RQ, *obj_cmd_rq, *obj_cmd_rs);
    if (ret < 0) {
        std::string e(ipmi_ctx_errormsg(ipmi));
        throw std::runtime_error("failed to request PICMG LED properties");
    }
    bridge.close();

    uint64_t compCode;
    if (fiid_obj_get(*obj_cmd_rs, "comp_code", &compCode) < 0)
        throw std::runtime_error("failed to decode PICMG LED properties response");
    if (compCode != 0)
        throw std::runtime_error("failed to decode PICMG LED properties response, invalid comp_code");

    uint64_t statusLeds;
    uint64_t appLeds;
    if (fiid_obj_get(*obj_cmd_rs, "status_leds", &statusLeds) < 0)
        throw std::runtime_error("failed to decode PICMG LED properties response");
    if (fiid_obj_get(*obj_cmd_rs, "app_leds", &appLeds) < 0)
        throw std::runtime_error("failed to decode PICMG LED properties response");

    PicmgLedAddress ledAddr(fruAddress.deviceAddr, fruAddress.channel, fruAddress.fruId, 0);
    std::vector<FreeIpmiProvider::Entity> leds;
    for (int i = 0; i < 4; i++) {
        if (statusLeds & (1 << i)) {
            try {
                ledAddr.ledId = i;
                leds.emplace_back( getPicmgLedFull(ipmi, ledAddr, namePrefix) );
            } catch (...) {
                continue;
            }
        }
    }
    for (int i = 0; i < 252; i++) {
        if (appLeds & (1 << i)) {
            try {
                ledAddr.ledId = i + 4;
                leds.emplace_back( getPicmgLedFull(ipmi, ledAddr, namePrefix) );
            } catch (...) {
                continue;
            }
        }
    }

    return leds;
}

FreeIpmiProvider::Entity FreeIpmiProvider::getPicmgLedFull(ipmi_ctx_t ipmi, const PicmgLedAddress& address, const std::string& namePrefix)
{
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

    FiidScoped obj_cmd_rq(tmpl_cmd_get_picmg_led_cap_rq);
    FiidScoped obj_cmd_rs(tmpl_cmd_get_picmg_led_cap_rs);

    if (*obj_cmd_rq == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "cmd", PICMG_GET_FRU_LED_COLOR_CAPABILITIES_CMD) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "picmg_id", IPMI_NET_FN_GROUP_EXTENSION_IDENTIFICATION_PICMG) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "fru_device_id", address.fruId) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "led_id", address.ledId) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");

    if (*obj_cmd_rs == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED response");

//ipmi_ctx_set_flags(ipmi, IPMI_FLAGS_DEBUG_DUMP);
    IpmbBridgeScoped bridge(ipmi, address.deviceAddr, address.channel);
    int ret = ipmi_cmd(ipmi, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_PICMG_RQ, *obj_cmd_rq, *obj_cmd_rs);
    if (ret < 0)
        throw std::runtime_error("failed to request PICMG LED capabilities");
    bridge.close();

    uint64_t compCode;
    if (fiid_obj_get(*obj_cmd_rs, "comp_code", &compCode) < 0)
        throw std::runtime_error("failed to decode PICMG LED capabilities response");
    if (compCode != 0)
        throw std::runtime_error("failed to decode PICMG LED capabilities response, invalid comp_code");

    uint64_t val;
    if (fiid_obj_get(*obj_cmd_rs, "colors", &val) < 0)
        throw std::runtime_error("failed to decode PICMG LED capabilities response");
    val |= 0x1; // Force 'off' color to be part of the options

    static const std::vector<std::string> colors = {
        "off", "blue", "red", "green", "amber", "orange", "white"
    };
    static const std::vector<std::pair<std::string,std::string>> fields = {
        { "ZRVL", "ZRST" },
        { "ONVL", "ONST" },
        { "TWVL", "TWST" },
        { "THVL", "THST" },
        { "FRVL", "FRST" },
        { "FVVL", "FVST" },
        { "SXVL", "SXST" },
    };

    Entity entity = getPicmgLed(ipmi, address);
    size_t j = 0;
    for (int i = 0; i < 7; i++) {
        if (val & (1 << i)) {
            entity[fields[j].first] = i;
            entity[fields[j].second] = colors[i];
            j++;
        }
    }

    entity["INP"] = "PICMG_LED " + address.get();
    entity["NAME"] = namePrefix + ":LED" + std::to_string(address.ledId);
    entity["DESC"] = (address.ledId < 4 ? "System Light " : "Custom Light ") + std::to_string(address.ledId);
    return entity;
}

FreeIpmiProvider::Entity FreeIpmiProvider::getPicmgLed(ipmi_ctx_t ipmi, const PicmgLedAddress& address)
{
    static fiid_template_t tmpl_cmd_get_picmg_led_get_rq =
    {
        { 8, "cmd", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "picmg_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "fru_device_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "led_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 0, "", 0}
    };

    static fiid_template_t tmpl_cmd_get_picmg_led_get_rs =
    {
        { 8, "cmd", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED | FIID_FIELD_MAKES_PACKET_SUFFICIENT},
        { 8, "comp_code", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED | FIID_FIELD_MAKES_PACKET_SUFFICIENT},
        { 8, "picmg_id", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 1, "state_local_control", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 1, "state_override_control", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 1, "state_lamp_test", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 1, "state_hardware_restrict", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "reserved", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},

        { 8, "local_control_function", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "local_control_duration", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "local_control_color", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "local_control_reserved", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "override_control_function", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "override_control_duration", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "override_control_color", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 4, "override_control_reserved", FIID_FIELD_REQUIRED | FIID_FIELD_LENGTH_FIXED},
        { 8, "lamp_test_duration", FIID_FIELD_OPTIONAL | FIID_FIELD_LENGTH_FIXED},
        { 0, "", 0}
    };

    FiidScoped obj_cmd_rq(tmpl_cmd_get_picmg_led_get_rq);
    FiidScoped obj_cmd_rs(tmpl_cmd_get_picmg_led_get_rs);

    if (*obj_cmd_rq == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "cmd", PICMG_GET_FRU_LED_STATE_CMD) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "picmg_id", IPMI_NET_FN_GROUP_EXTENSION_IDENTIFICATION_PICMG) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "fru_device_id", address.fruId) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");
    if (fiid_obj_set(*obj_cmd_rq, "led_id", address.ledId) < 0)
        throw std::runtime_error("failed to initialize PICMG LED request");

    if (*obj_cmd_rs == nullptr)
        throw std::runtime_error("failed to allocate PICMG LED response");

    IpmbBridgeScoped bridge(ipmi, address.deviceAddr, address.channel);
    int ret  = ipmi_cmd(ipmi, IPMI_BMC_IPMB_LUN_BMC, IPMI_NET_FN_PICMG_RQ, *obj_cmd_rq, *obj_cmd_rs);
    if (ret < 0 && ipmi_ctx_errnum(ipmi) == IPMI_ERR_SESSION_TIMEOUT)
        m_connected = false;
    if (ret < 0)
        throw std::runtime_error("failed to request PICMG LED state");
    bridge.close();

    uint64_t compCode;
    if (fiid_obj_get(*obj_cmd_rs, "comp_code", &compCode) < 0)
        throw std::runtime_error("failed to decode PICMG LED state response");
    if (compCode != 0)
        throw std::runtime_error("failed to decode PICMG LED state response, invalid comp_code");

    int state = 0; // off

    uint64_t val = 0;
    if (fiid_obj_get(*obj_cmd_rs, "state_local_control", &val) < 0)
        throw std::runtime_error("failed to decode PICMG LED state response");
    if (val) {
        val = 0;
        // TODO: val < 255 => off-state blinking
        if (fiid_obj_get(*obj_cmd_rs, "local_control_function", &val) < 0)
            throw std::runtime_error("failed to decode PICMG LED state response");
        if (val > 0) {
            val = 0;
            if (fiid_obj_get(*obj_cmd_rs, "local_control_color", &val) < 0)
                throw std::runtime_error("failed to decode PICMG LED state response");
            state = (val & 0xF);
        }
    }

    val = 0;
    if (fiid_obj_get(*obj_cmd_rs, "state_override_control", &val) < 0)
        throw std::runtime_error("failed to decode PICMG LED state response");
    if (val) {
        val = 0;
        // TODO: val < 255 => off-state blinking
        if (fiid_obj_get(*obj_cmd_rs, "override_control_function", &val) < 0)
            throw std::runtime_error("failed to decode PICMG LED state response");
        if (val > 0) {
            val = 0;
            if (fiid_obj_get(*obj_cmd_rs, "override_control_color", &val) < 0)
                throw std::runtime_error("failed to decode PICMG LED state response");
            state = (val & 0xF);
        }
    }

    // TODO: lamp test

    Entity entity;
    entity["VAL"] = state;
    return entity;
}


/*
 * ===== PicmgLedAddress implementation =====
 *
 * EPICS record link specification for SENSOR entities
 * @ipmi <conn> PICMG_LED <owner>:<channel>:<fru>:<led>
 * Example:
 * @ipmi IPMI1 PICMG_LED 130:5:1
 */
FreeIpmiProvider::PicmgLedAddress::PicmgLedAddress(const std::string& address)
{
    auto tokens = common::split(address, ':');
    if (tokens.size() != 4)
        throw Provider::syntax_error("Invalid PICMG LED address");

    try {
        deviceAddr = std::stoi(tokens[0]) & 0xFF;
        channel    = std::stoi(tokens[1]) & 0xFF;
        fruId      = std::stoi(tokens[2]) & 0xFF;
        ledId      = std::stoi(tokens[3]) & 0xFF;
    } catch (std::invalid_argument) {
        throw Provider::syntax_error("Invalid PICMG LED address");
    }
}

FreeIpmiProvider::PicmgLedAddress::PicmgLedAddress(uint8_t deviceAddr_, uint8_t channel_, uint8_t fruId_, uint8_t ledId_)
    : deviceAddr(deviceAddr_)
    , channel(channel_)
    , fruId(fruId_)
    , ledId(ledId_)
{}

std::string FreeIpmiProvider::PicmgLedAddress::get() const
{
    return std::to_string(deviceAddr) + ":" + std::to_string(channel) + ":" + std::to_string(fruId) + ":" + std::to_string(ledId);
}

bool FreeIpmiProvider::PicmgLedAddress::compare(const FreeIpmiProvider::PicmgLedAddress& other) const
{
    if (other.deviceAddr != deviceAddr)
        return false;
    if (other.channel != channel)
        return false;
    if (other.fruId != fruId)
        return false;
    if (other.ledId != ledId)
        return false;
    return true;
}
