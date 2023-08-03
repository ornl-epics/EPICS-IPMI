/* ipmifru.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Mar 2019
 */

#include "freeipmiprovider.h"

Provider::Entity FreeIpmiProvider::getFru(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr, ipmi_fru_ctx_t fru, const FruAddress& address)
{
    // Find FRU Device Locator entry in SDR that matches selected device id
    if (ipmi_sdr_cache_first(sdr) < 0)
        throw std::runtime_error("failed to rewind SDR cache - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    // Iterate through SDR until a record is found
    bool found = false;
    do {
        uint8_t recordType;
        if (ipmi_sdr_parse_record_id_and_type(sdr, NULL, 0, NULL, &recordType) < 0) {
            LOG_WARN("Failed to parse SDR record type - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
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

//        if (isFruLogical(sdr, record) == false)
//            continue;

        try {
            FruAddress tmp(sdr, record);
            if (address.compare(tmp, false, false)) {
                found = true;
                break;
            }
        } catch (Provider::process_error& e) {
            LOG_DEBUG("%s", e.what());
            continue;
        }
    } while (ipmi_sdr_cache_next(sdr) == 1);
    if (!found)
        throw Provider::process_error("FRU not found");

    IpmbBridgeScoped bridge(ipmi, address.deviceAddr, address.channel);

    if (ipmi_fru_open_device_id(fru, address.fruId) < 0)
        throw std::runtime_error("Failed to open FRU device - " + std::string(ipmi_fru_ctx_errormsg(fru)));

    if (ipmi_fru_first(fru) < 0) {
        ipmi_fru_close_device_id(fru);
        throw std::runtime_error("Failed to rewind FRU - " + std::string(ipmi_fru_ctx_errormsg(fru)));
    }

    std::string error;
    std::string subarea;
    do {
        unsigned int areaType = 0;
        FruArea buffer;

        if (ipmi_fru_read_data_area(fru, &areaType, &buffer.size, buffer.data, buffer.max_size-1) < 0)
            continue;
        if (buffer.size == 0)
            continue;

        try {
            switch (areaType) {
            case IPMI_FRU_AREA_TYPE_CHASSIS_INFO_AREA:
                if (address.area == "CHASSIS")
                    subarea = getFruChassisSubarea(fru, buffer, address.subarea);
                break;
            case IPMI_FRU_AREA_TYPE_BOARD_INFO_AREA:
                if (address.area == "BOARD")
                    subarea = getFruBoardSubarea(fru, buffer, address.subarea);
                break;
            case IPMI_FRU_AREA_TYPE_PRODUCT_INFO_AREA:
                if (address.area == "PRODUCT")
                    subarea = getFruProductSubarea(fru, buffer, address.subarea);
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_DC_OUTPUT:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_DC_LOAD:
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_EXTENDED_DC_LOAD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_MANAGEMENT_ACCESS_RECORD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_BASE_COMPATABILITY_RECORD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_EXTENDED_COMPATABILITY_RECORD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_OEM:
                break;
            default:
                break;
            }
        } catch (std::runtime_error e) {
            error = e.what();
            break;
        }

    } while (subarea.empty() && ipmi_fru_next(fru) == 1);

    ipmi_fru_close_device_id(fru);

    if (!error.empty())
        throw Provider::process_error(error);
    if (subarea.empty())
        throw Provider::process_error("FRU area not found");

    Entity entity;
    entity["VAL"] = subarea;
    return entity;
}

std::string FreeIpmiProvider::getFruName(ipmi_sdr_ctx_t sdr, const FreeIpmiProvider::SdrRecord& record)
{
    FruAddress address(sdr, record);
    uint8_t entityId;
    uint8_t entityInstance;
    if (ipmi_sdr_parse_fru_entity_id_and_instance(sdr, record.data, record.size, &entityId, &entityInstance) < 0)
        throw Provider::process_error("Failed to get SDR FRU entity info - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    static const std::vector<std::string> epics_entity_ids = {
        "Ent", "Other", "Ent", "Proc", "Disk", "Periph", "Sys", "Sys", "Mem", "Proc", "PS", "AddIn",
        "Panel", "Panel", "PS", "Drive", "Sys", "Sys", "Proc", "PS", "PS", "PS",
        "Chas", "Chas", "Chas", "Chas", "Disk", "Periph", "Dev", "Fan", "CU", "Cable", "Mem", "Sys",
        "Sys", "OS", "Sys", "Grp", "RemMgmt", "Ext", "Batt", "Proc", "Conn", "Proc", "IO", "Proc",
        "MgmtFw", "IPMI", "PCI", "PCIe", "SCSI", "SATA", "Proc", "RTC", "Ent", "Ent", "Ent", "Ent",
        "Ent", "Ent", "Ent", "Ent", "Ent", "Ent", "Air", "Proc", "Main"
    };

    std::string name = "Ent";
    if (IPMI_ENTITY_ID_VALID(entityId)) {
        if (entityId >= epics_entity_ids.size())
            throw Provider::process_error("Failed to get SDR FRU entity info - lookup table out of range");
        name = epics_entity_ids[entityId];
    } else if (IPMI_ENTITY_ID_IS_CHASSIS_SPECIFIC(entityId)) {
        name = "Chas";
    } else if (IPMI_ENTITY_ID_IS_BOARD_SET_SPECIFIC(entityId)) {
        name = "Board";
    } else if (IPMI_ENTITY_ID_IS_OEM_SYSTEM_INTEGRATOR_DEFINED(entityId)) {
        name = "Oem";
    }

    name += std::to_string(address.deviceAddr) + "_" + std::to_string(address.fruId);
    return name;
}

std::string FreeIpmiProvider::getFruDesc(ipmi_sdr_ctx_t sdr, const FreeIpmiProvider::SdrRecord& record)
{
    char deviceDesc[IPMI_SDR_MAX_DEVICE_ID_STRING_LENGTH+1] = {0};
    if (ipmi_sdr_parse_device_id_string(sdr, record.data, record.size, deviceDesc, sizeof(deviceDesc)-1) < 0)
        throw Provider::process_error("Failed to parse SDR FRU device id - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));
    return deviceDesc;
}

std::vector<FreeIpmiProvider::Entity> FreeIpmiProvider::getFrus(ipmi_ctx_t ipmi, ipmi_sdr_ctx_t sdr, ipmi_fru_ctx_t fru)
{
    std::vector<Entity> entities;

    if (ipmi_sdr_cache_first(sdr) < 0)
        throw std::runtime_error("failed to rewind SDR cache - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

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

        FruAddress address;
        Entity tmpl;
        try {
            address = FruAddress(sdr, record);
            tmpl["NAME"] = getFruName(sdr, record);
            tmpl["DESC"] = getFruDesc(sdr, record);
        } catch (std::runtime_error& e) {
            LOG_DEBUG(std::string(e.what()) + ", skipping");
            continue;
        }

        IpmbBridgeScoped bridge(ipmi, address.deviceAddr, address.channel);
        try {
            auto tmp = getFruAreas(fru, address, tmpl);
            for (auto& e: tmp)
                entities.emplace_back( std::move(e) );
        } catch (std::runtime_error& e) {
            LOG_DEBUG(std::string(e.what()) + ", skipping");
        }

    } while (ipmi_sdr_cache_next(sdr) == 1);

    return entities;
}

std::map<std::pair<uint8_t,uint8_t>,std::string> FreeIpmiProvider::getFruEntityNameAssoc(ipmi_sdr_ctx_t sdr)
{
    std::map<std::pair<uint8_t,uint8_t>,std::string> names;

    if (ipmi_sdr_cache_first(sdr) < 0)
        throw std::runtime_error("failed to rewind SDR cache - " + std::string(ipmi_sdr_ctx_errormsg(sdr)));

    do {
        uint8_t recordType;
        if (ipmi_sdr_parse_record_id_and_type(sdr, NULL, 0, NULL, &recordType) < 0) {
            LOG_DEBUG("Failed to parse SDR record type - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
            continue;
        }

        if (recordType == IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
            SdrRecord record;
            record.size = ipmi_sdr_cache_record_read(sdr, record.data, record.max_size);
            if (record.size < 0) {
                LOG_DEBUG("Failed to read SDR record - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
                continue;
            }

            uint8_t entityId;
            uint8_t entityInstance;
            if (ipmi_sdr_parse_fru_entity_id_and_instance(sdr, record.data, record.size, &entityId, &entityInstance) < 0) {
                LOG_DEBUG("Failed to read SDR entity info - %s, skipping", ipmi_sdr_ctx_errormsg(sdr));
                continue;
            }

            try {
                auto key = std::make_pair(entityId, entityInstance);
                names[key] = getFruName(sdr, record);
            } catch (std::runtime_error& e) {
                LOG_DEBUG(std::string(e.what()) + ", skipping");
                //pass
            }
            continue;
        }
        if (recordType == IPMI_SDR_FORMAT_ENTITY_ASSOCIATION_RECORD) {
            // TODO:
            continue;
        }
    } while (ipmi_sdr_cache_next(sdr) == 1);

    return names;
}

std::vector<Provider::Entity> FreeIpmiProvider::getFruAreas(ipmi_fru_ctx_t fru, const FruAddress& address, const Provider::Entity& tmpl)
{
    if (ipmi_fru_open_device_id(fru, address.fruId) < 0)
        throw std::runtime_error("Failed to open FRU device - " + std::string(ipmi_fru_ctx_errormsg(fru)));

    if (ipmi_fru_first(fru) < 0) {
        ipmi_fru_close_device_id(fru);
        throw std::runtime_error("Failed to rewind FRU - " + std::string(ipmi_fru_ctx_errormsg(fru)));
    }

    std::vector<Entity> entities;
    do {
        unsigned int areaType = 0;
        FruArea buffer;

        if (ipmi_fru_read_data_area(fru, &areaType, &buffer.size, buffer.data, buffer.max_size-1) < 0) {
            // Silently? skip it
            continue;
        }

        if (buffer.size == 0)
            continue;

        try {
            std::vector<Entity> tmp;
            switch (areaType) {
            case IPMI_FRU_AREA_TYPE_CHASSIS_INFO_AREA:
                tmp = getFruChassis(fru, address, tmpl, buffer);
                break;
            case IPMI_FRU_AREA_TYPE_BOARD_INFO_AREA:
                tmp = getFruBoard(fru, address, tmpl, buffer);
                break;
            case IPMI_FRU_AREA_TYPE_PRODUCT_INFO_AREA:
                tmp = getFruProduct(fru, address, tmpl, buffer);
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_DC_OUTPUT:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_DC_LOAD:
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_EXTENDED_DC_LOAD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_MANAGEMENT_ACCESS_RECORD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_BASE_COMPATABILITY_RECORD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_EXTENDED_COMPATABILITY_RECORD:
                break;
            case IPMI_FRU_AREA_TYPE_MULTIRECORD_OEM:
                break;
            default:
                break;
            }

            for (const auto& e: tmp)
                entities.emplace_back( std::move(e) );

        } catch (std::runtime_error e) {
            LOG_DEBUG(std::string(e.what()) + ", skipping");
        }

    } while (ipmi_fru_next(fru) == 1);

    ipmi_fru_close_device_id(fru);

    return entities;
}

std::string FreeIpmiProvider::getFruField(ipmi_fru_ctx_t fru, const ipmi_fru_field_t& field, uint8_t languageCode)
{
    char strbuf[IPMI_FRU_AREA_STRING_MAX + 1] = {0};
    unsigned int strbuflen = IPMI_FRU_AREA_STRING_MAX;

    if (!field.type_length_field_length)
        return "";

    if (ipmi_fru_type_length_field_to_string(fru,
                                             field.type_length_field,
                                             field.type_length_field_length,
                                             languageCode,
                                             strbuf,
                                             &strbuflen) < 0)
        return "";

    return std::string(strbuf);
}

std::vector<Provider::Entity> FreeIpmiProvider::getFruChassis(ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& tmpl, const FruArea& fruArea)
{
    static const size_t IPMI_FRU_CUSTOM_FIELDS = 64;

    std::vector<Entity> entities;

    std::list<std::string> subareas = { "Type", "PartNum", "SerialNum" };
    for (size_t i = 0; i < IPMI_FRU_CUSTOM_FIELDS; i++) {
        subareas.emplace_back( "Field" + std::to_string(i) );
    }

    // We'll modify subarea in each iteration, but we only need it for the address string
    FruAddress addr = address;
    addr.area = "CHASSIS";

    // Yes this is not the most efficient way of doing it,
    // but getFruChassis() is invoked rarely so we trade performance for cleaner code.
    for (auto& subarea: subareas) {
        try {
            auto SUBAREA = common::to_upper(subarea);
            auto value = getFruChassisSubarea(fru, fruArea, SUBAREA);
            if (!value.empty()) {
                addr.subarea = SUBAREA;

                Entity entity = tmpl;
                entity["VAL"] = value;
                entity["NAME"] = tmpl.getField<std::string>("NAME", "") + ":Chas:" + subarea;
                entity["DESC"] = tmpl.getField<std::string>("DESC", "") + " Chassis " + subarea;
                entity["INP"] = "FRU " + addr.get();
                entities.emplace_back( std::move(entity) );
            }
        } catch (...) {
            // pass
        }
    }
    return entities;
}

std::string FreeIpmiProvider::getFruChassisSubarea(ipmi_fru_ctx_t fru, const FruArea& area, const std::string& subarea)
{
    static const size_t IPMI_FRU_CUSTOM_FIELDS = 64;
    uint8_t type;
    ipmi_fru_field_t partNum;
    ipmi_fru_field_t serialNum;
    ipmi_fru_field_t customFields[IPMI_FRU_CUSTOM_FIELDS];

    if (ipmi_fru_chassis_info_area(fru, area.data, area.size, &type, &partNum, &serialNum, customFields, IPMI_FRU_CUSTOM_FIELDS) < 0)
        throw Provider::process_error("Failed to parse FRU chassis info - " + std::string(ipmi_fru_ctx_errormsg(fru)));
    if (!IPMI_FRU_CHASSIS_TYPE_VALID(type))
        type = IPMI_FRU_CHASSIS_TYPE_UNKNOWN;

    Entity entity;
    if (subarea == "TYPE")      return ipmi_fru_chassis_types[type];
    if (subarea == "PARTNUM")   return getFruField(fru, partNum,   IPMI_FRU_LANGUAGE_CODE_ENGLISH);
    if (subarea == "SERIALNUM") return getFruField(fru, serialNum, IPMI_FRU_LANGUAGE_CODE_ENGLISH);
    if (subarea.find("FIELD") == 0) {
        std::string tmp = subarea;
        tmp.erase(0, 5);
        size_t i = std::stol(tmp);
        if (i < IPMI_FRU_CUSTOM_FIELDS)
            return getFruField(fru, customFields[i], IPMI_FRU_LANGUAGE_CODE_ENGLISH);
    }
    throw Provider::syntax_error("Invalid FRU chassis area " + subarea);
}

std::vector<Provider::Entity> FreeIpmiProvider::getFruBoard(ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& tmpl, const FruArea& fruArea)
{
    static const size_t IPMI_FRU_CUSTOM_FIELDS = 64;

    std::vector<Entity> entities;

    std::list<std::string> subareas = { "DateTime", "Manufacturer", "Product", "PartNum", "SerialNum", "FileId" };
    for (size_t i = 0; i < IPMI_FRU_CUSTOM_FIELDS; i++) {
        subareas.emplace_back( "Field" + std::to_string(i) );
    }

    // We'll modify subarea in each iteration, but we only need it for the address string
    FruAddress addr = address;
    addr.area = "BOARD";

    // Yes this is not the most efficient way of doing it,
    // but getFruChassis() is invoked rarely so we trade performance for cleaner code.
    for (auto& subarea: subareas) {
        try {
            auto SUBAREA = common::to_upper(subarea);
            auto value = getFruBoardSubarea(fru, fruArea, SUBAREA);
            if (!value.empty()) {
                addr.subarea = SUBAREA;

                Entity entity = tmpl;
                entity["VAL"] = value;
                entity["NAME"] = tmpl.getField<std::string>("NAME", "") +  + ":Board:" + subarea;
                entity["DESC"] = tmpl.getField<std::string>("DESC", "") +  + " Board " + subarea;
                entity["INP"] = "FRU " + addr.get();
                entities.emplace_back( std::move(entity) );
            }
        } catch (...) {
            // pass
        }
    }
    return entities;
}

std::string FreeIpmiProvider::getFruBoardSubarea(ipmi_fru_ctx_t fru, const FruArea& area, const std::string& subarea)
{
    static const size_t IPMI_FRU_CUSTOM_FIELDS = 64;
    static const size_t IPMI_FRU_BOARD_STR_BUFLEN = 1024;

    uint8_t langCode;
    uint32_t dateTime;
    ipmi_fru_field_t manufacturer;
    ipmi_fru_field_t product;
    ipmi_fru_field_t serialNum;
    ipmi_fru_field_t partNum;
    ipmi_fru_field_t fruFileId;
    ipmi_fru_field_t customFields[IPMI_FRU_CUSTOM_FIELDS];

    if (ipmi_fru_board_info_area(fru, area.data, area.size, &langCode, &dateTime, &manufacturer, &product, &serialNum, &partNum, &fruFileId, customFields, IPMI_FRU_CUSTOM_FIELDS) < 0)
        throw Provider::process_error("Failed to parse FRU chassis info - " + std::string(ipmi_fru_ctx_errormsg(fru)));

    if (subarea == "MANUFACTURER") return getFruField(fru, manufacturer, langCode);
    if (subarea == "PRODUCT")      return getFruField(fru, product,      langCode);
    if (subarea == "SERIALNUM")    return getFruField(fru, serialNum,    langCode);
    if (subarea == "PARTNUM")      return getFruField(fru, partNum,      langCode);
    if (subarea == "FILEID")       return getFruField(fru, fruFileId,    langCode);
    if (subarea == "DATETIME") {
        char buf[IPMI_FRU_BOARD_STR_BUFLEN + 1] = { 0 };
        int flags = IPMI_TIMESTAMP_FLAG_UTC_TO_LOCALTIME | IPMI_TIMESTAMP_FLAG_DEFAULT;
        if (dateTime != IPMI_FRU_MFG_DATE_TIME_UNSPECIFIED)
            return "unspecified";
        if (ipmi_timestamp_string(dateTime, common::getUtcOffset(), flags, "%D - %T", buf, sizeof(buf)-1) < 0)
            return "invalid";
        return buf;
    }
    if (subarea.find("FIELD") == 0) {
        std::string tmp = subarea;
        tmp.erase(0, 5);
        size_t i = std::stol(tmp);
        if (i < IPMI_FRU_CUSTOM_FIELDS)
            return getFruField(fru, customFields[i], langCode);
    }
    throw Provider::syntax_error("Invalid FRU board area " + subarea);
}

std::vector<Provider::Entity> FreeIpmiProvider::getFruProduct(ipmi_fru_ctx_t fru, const FruAddress& address, const Entity& tmpl, const FruArea& fruArea)
{
    static const size_t IPMI_FRU_CUSTOM_FIELDS = 64;

    std::vector<Entity> entities;

    std::list<std::string> subareas = { "Manufacturer", "Product", "Model", "Version", "AssetTag", "SerialNum", "FileId" };
    for (size_t i = 0; i < IPMI_FRU_CUSTOM_FIELDS; i++) {
        subareas.emplace_back( "Field" + std::to_string(i) );
    }

    // We'll modify subarea in each iteration, but we only need it for the address string
    FruAddress addr = address;
    addr.area = "PRODUCT";

    // Yes this is not the most efficient way of doing it,
    // but getFruChassis() is invoked rarely so we trade performance for cleaner code.
    for (auto& subarea: subareas) {
        try {
            auto SUBAREA = common::to_upper(subarea);
            auto value = getFruProductSubarea(fru, fruArea, SUBAREA);
            if (!value.empty()) {
                addr.subarea = SUBAREA;

                Entity entity = tmpl;
                entity["VAL"] = value;
                entity["NAME"] = tmpl.getField<std::string>("NAME", "") +  + ":Prod:" + subarea;
                entity["DESC"] = tmpl.getField<std::string>("DESC", "") +  + " Product " + subarea;
                entity["INP"] = "FRU " + addr.get();
                entities.emplace_back( std::move(entity) );
            }
        } catch (...) {
            // pass
        }

    }
    return entities;
}

std::string FreeIpmiProvider::getFruProductSubarea(ipmi_fru_ctx_t fru, const FruArea& area, const std::string& subarea)
{
    static const size_t IPMI_FRU_CUSTOM_FIELDS = 64;

    uint8_t langCode;
    ipmi_fru_field_t manufacturer;
    ipmi_fru_field_t product;
    ipmi_fru_field_t model;
    ipmi_fru_field_t version;
    ipmi_fru_field_t serialNum;
    ipmi_fru_field_t assetTag;
    ipmi_fru_field_t fruFileId;
    ipmi_fru_field_t customFields[IPMI_FRU_CUSTOM_FIELDS];

    if (ipmi_fru_product_info_area(fru, area.data, area.size, &langCode, &manufacturer, &product, &model, &version, &serialNum, &assetTag, &fruFileId, customFields, IPMI_FRU_CUSTOM_FIELDS) < 0)
        throw Provider::process_error("Failed to parse FRU chassis info - " + std::string(ipmi_fru_ctx_errormsg(fru)));

    if (subarea == "MANUFACTURER") return getFruField(fru, manufacturer, langCode);
    if (subarea == "PRODUCT")      return getFruField(fru, product,      langCode);
    if (subarea == "MODEL")        return getFruField(fru, model,        langCode);
    if (subarea == "VERSION")      return getFruField(fru, version,      langCode);
    if (subarea == "SERIALNUM")    return getFruField(fru, serialNum,    langCode);
    if (subarea == "ASSETTAG")     return getFruField(fru, assetTag,     langCode);
    if (subarea == "FILEID")       return getFruField(fru, fruFileId,    langCode);
    if (subarea.find("FIELD") == 0) {
        std::string tmp = subarea;
        tmp.erase(0, 5);
        size_t i = std::stol(tmp);
        if (i < IPMI_FRU_CUSTOM_FIELDS)
            return getFruField(fru, customFields[i], langCode);
    }
    throw Provider::syntax_error("Invalid FRU product area " + subarea);
}

bool FreeIpmiProvider::isFruLogical(ipmi_sdr_ctx_t sdr, const SdrRecord& record)
{
    uint8_t device_type;
    uint8_t device_type_modifier;
    if (ipmi_sdr_parse_device_type(sdr, record.data, record.size, &device_type, &device_type_modifier) < 0) {
        LOG_DEBUG("Failed to parse FRU device type");
        return false;
    }

    // Copied from ipmi-fru/ipmi-fru.c:565: _is_logical_fru()
    if ((device_type == IPMI_DEVICE_TYPE_EEPROM_24C01_OR_EQUIVALENT
         && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C01_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C02_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C02_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C04_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C04_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C08_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C08_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C16_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C16_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C17_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C17_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C32_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C32_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_EEPROM_24C64_OR_EQUIVALENT
            && device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_EEPROM_24C64_OR_EQUIVALENT_IPMI_FRU_INVENTORY)
        || (device_type == IPMI_DEVICE_TYPE_FRU_INVENTORY_DEVICE_BEHIND_MANAGEMENT_CONTROLLER
            && (device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_FRU_INVENTORY_DEVICE_BEHIND_MANAGEMENT_CONTROLLER_IPMI_FRU_INVENTORY_BACKWARDS_COMPATABILITY
        || device_type_modifier == IPMI_DEVICE_TYPE_MODIFIER_FRU_INVENTORY_DEVICE_BEHIND_MANAGEMENT_CONTROLLER_IPMI_FRU_INVENTORY)))
        return true;
    return false;
}

/*
 * ===== FruAddress implementation =====
 *
 * EPICS record link specification for FRU entities
 * @ipmi <conn> FRU <device_addr>:<device_id>:<lun>:<channel> <area> <subarea>
 * Example:
 * @ipmi IPMI1 FRU 32:12:1:7 CHASSIS SERIALNUM
 */
FreeIpmiProvider::FruAddress::FruAddress(const std::string& address)
{
    auto sections = common::split(address, ' ', 3);
    if (sections.size() != 3)
        throw Provider::syntax_error("Invalid FRU address");
    auto addrspec = common::split(sections[0], ':');
    if (addrspec.size() != 4)
        throw Provider::syntax_error("Invalid FRU address");

    try {
        deviceAddr = std::stoi(addrspec[0]) & 0xFF;
        fruId      = std::stoi(addrspec[1]) & 0xFF;
        lun        = std::stoi(addrspec[2]) & 0xFF;
        channel    = std::stoi(addrspec[3]) & 0xFF;
        area    = sections[1];
        subarea = sections[2];
    } catch (std::invalid_argument &) {
        throw Provider::syntax_error("Invalid FRU address");
    }
}

FreeIpmiProvider::FruAddress::FruAddress(ipmi_sdr_ctx_t sdr, const SdrRecord& record)
{
    uint8_t logical;
    if (ipmi_sdr_parse_fru_device_locator_parameters(sdr,
                                                     record.data, record.size,
                                                     &deviceAddr,
                                                     &fruId,
                                                     NULL, //&privateBusId,
                                                     &lun,
                                                     &logical,
                                                     &channel) < 0)
        throw Provider::process_error("Failed to parse FRU address from FRU Device Locator");
    if (logical != 1)
        throw Provider::process_error("FRU logical type not supported");

    // stored in 7-bit form
    deviceAddr <<= 1;
}

std::string FreeIpmiProvider::FruAddress::get() const
{
    std::string addrspec;
    addrspec += std::to_string(deviceAddr) + ":";
    addrspec += std::to_string(fruId) + ":";
    addrspec += std::to_string(lun) + ":";
    addrspec += std::to_string(channel);
    return addrspec + " " + area + " " + subarea;
}

bool FreeIpmiProvider::FruAddress::compare(const FruAddress& other, bool checkArea, bool checkSubarea) const
{
    if (deviceAddr != other.deviceAddr)
        return false;
    if (fruId != other.fruId)
        return false;
    if (channel != other.channel)
        return false;
    if (lun != other.lun)
        return false;
    if (checkArea && area != other.area)
        return false;
    if (checkSubarea && subarea != other.subarea)
        return false;
    return true;
}
