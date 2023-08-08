/*
 *
 *
 *
 *
 */

#include <stdio.h>
#include <memory>
#include <list>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <freeipmi/freeipmi.h>
#include "EpAiRecord.h"

std::map<uint8_t, std::string> sdr_type_itos_map {
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

std::map<uint8_t, std::string> sdr_sensor_owner_id_type_itos_map {
    {IPMI_SDR_SENSOR_OWNER_ID_TYPE_IPMB_SLAVE_ADDRESS, "IPMB_SLAVE_ADDRESS"},
    {IPMI_SDR_SENSOR_OWNER_ID_TYPE_SYSTEM_SOFTWARE_ID, "SYSTEM_SOFTWARE_ID"}
};

std::map<uint8_t, std::string> sensor_event_reading_type_code_itos_map {
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

std::string get_sensor_event_reading_type_code(uint8_t val) {
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

class SdrRec
{
protected:
    uint16_t record_id;
    uint8_t record_type;
    std::string device_id_string;
public:
    SdrRec(uint16_t record_id, uint8_t record_type);
    ~SdrRec();
    uint16_t get_record_id();
    uint8_t get_record_type();
    std::string get_device_id_string();

};

SdrRec::SdrRec(uint16_t record_id, uint8_t record_type)
    :record_id(record_id), record_type(record_type)
{

}

SdrRec::~SdrRec() {
}

uint16_t SdrRec::get_record_id() {
    return this->record_id;
}

uint8_t SdrRec::get_record_type() {
    return this->record_type;
}

std::string SdrRec::get_device_id_string() {
    return this->device_id_string;
}

class SensorRecCompact : public SdrRec
{
private:
    uint8_t sensor_owner_id_type;
    uint8_t sensor_owner_id;
    uint8_t sensor_owner_lun;
    uint8_t channel_number;
    uint8_t sensor_number;
    uint8_t entity_id;
    uint8_t entity_instance;
    uint8_t entity_instance_type;
    uint8_t sensor_type;
    uint8_t event_reading_type_code;

public:
    SensorRecCompact(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type);
    ~SensorRecCompact();
    uint8_t get_sensor_owner_id_type();
    uint8_t get_sensor_owner_id();
    uint8_t get_sensor_owner_lun();
    uint8_t get_channel_number();
    uint8_t get_sensor_number();
    uint8_t get_entity_id();
    uint8_t get_entity_instance();
    uint8_t get_sensor_type();
    uint8_t get_event_reading_type_code();
};

SensorRecCompact::SensorRecCompact(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type)
    :SdrRec(record_id, record_type)
{
    int rv = (-1);

    rv = ipmi_sdr_parse_sensor_owner_id (sdr, NULL, 0, &sensor_owner_id_type, &sensor_owner_id);

    rv = ipmi_sdr_parse_sensor_owner_lun (sdr, NULL, 0, &sensor_owner_lun, &channel_number);

    rv = ipmi_sdr_parse_sensor_number (sdr, NULL, 0, &sensor_number);

    rv = ipmi_sdr_parse_entity_id_instance_type (sdr, NULL, 0, &entity_id, &entity_instance, &entity_instance_type);

    rv = ipmi_sdr_parse_sensor_type (sdr, NULL, 0, &sensor_type);

    rv = ipmi_sdr_parse_event_reading_type_code (sdr, NULL, 0, &event_reading_type_code);

    char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
    rv = ipmi_sdr_parse_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);
    this->device_id_string = id_str;

}

SensorRecCompact::~SensorRecCompact()
{
}

uint8_t SensorRecCompact::get_sensor_owner_id_type() {
    return this->sensor_owner_id_type;
}

uint8_t SensorRecCompact::get_sensor_owner_id() {
    return this->sensor_owner_id;
}

uint8_t SensorRecCompact::get_sensor_owner_lun() {
    return this->sensor_owner_lun;
}

uint8_t SensorRecCompact::get_channel_number() {
    return this->channel_number;
}

uint8_t SensorRecCompact::get_sensor_number() {
    return this->sensor_number;
}

uint8_t SensorRecCompact::get_entity_id() {
    return this->entity_id;
}

uint8_t SensorRecCompact::get_entity_instance() {
    return this->entity_instance;
}

uint8_t SensorRecCompact::get_sensor_type() {
    return this->sensor_type;
}

uint8_t SensorRecCompact::get_event_reading_type_code() {
    return this->event_reading_type_code;
}


class SensorRecFull : public SensorRecCompact
{
private:
    

public:
    SensorRecFull(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type);
    ~SensorRecFull();
};

SensorRecFull::SensorRecFull(ipmi_sdr_ctx_t sdr, uint16_t record_id, uint8_t record_type)
    :SensorRecCompact(sdr, record_id, record_type)
{

}

SensorRecFull::~SensorRecFull()
{
}

class FruDevLocatorRec : public SdrRec
{
private:
    uint8_t device_access_address;
    uint8_t logical_fru_device_device_slave_address;
    uint8_t private_bus_id;
    uint8_t lun_for_master_write_read_fru_command;
    uint8_t logical_physical_fru_device;
    uint8_t channel_number;
    uint8_t fru_entity_id;
    uint8_t fru_entity_instance;
    std::vector<std::shared_ptr<SensorRecCompact>> sensor_records;

public:
    FruDevLocatorRec(ipmi_sdr_ctx_t sdr, uint16_t recid, uint8_t rectype);
    ~FruDevLocatorRec();
    std::string report();
    template<typename T>
    void parse_sensors(T &sensor_list);
};

FruDevLocatorRec::FruDevLocatorRec(ipmi_sdr_ctx_t sdr, uint16_t recid, uint8_t rectype)
    :SdrRec(recid, rectype)
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

}

FruDevLocatorRec::~FruDevLocatorRec()
{
}

std::string FruDevLocatorRec::report() {
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

    if(this->sensor_records.size() > 0) {
        ss << "Sensors attched (" << (unsigned int) this->sensor_records.size() << ")\n";
        int count  = 1;
        for(auto &rec : this->sensor_records) {
            ss << "[" << count << "]\n" <<
                " * ID-String: \'" << rec->get_device_id_string() << "\'" <<
                ", \n * Record-Id: " << rec->get_record_id() <<
                ", \n * Sensor-Owner-Id-Type: \'" <<
                sdr_sensor_owner_id_type_itos_map[rec->get_sensor_owner_id_type()] <<
                "\' (" << (unsigned int) rec->get_sensor_owner_id_type() << ")" <<
                ", \n * Sensor-Owner-Id: " << (unsigned int) rec->get_sensor_owner_id() <<
                ", \n * Sensor-Owner-LUN: " << (unsigned int) rec->get_sensor_owner_lun() <<
                ", \n * Channel Number: " << (unsigned int) rec->get_channel_number() <<
                ", \n * Sensor Number: " << (unsigned int) rec->get_sensor_number() <<
                ", \n * Entity-Id: ";
                if(IPMI_ENTITY_ID_VALID(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_ids_pretty[rec->get_entity_id()] << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else if(IPMI_ENTITY_ID_IS_CHASSIS_SPECIFIC(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_id_chassis_specific << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else if(IPMI_ENTITY_ID_IS_BOARD_SET_SPECIFIC(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_id_board_set_specific << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else if(IPMI_ENTITY_ID_IS_OEM_SYSTEM_INTEGRATOR_DEFINED(rec->get_entity_id())) {
                    ss << "\'" << ipmi_entity_id_oem_system_integrator << "\'" <<
                        " (" << (unsigned int) rec->get_entity_id() << ")";
                }
                else {
                    ss << "\'Unknow\' (" << (unsigned int) rec->get_entity_id() << ")";
                }
                ss << ", \n * Entity Instance: " << (unsigned int) rec->get_entity_instance() <<
                ", \n * Sensor Type: ";
                if(IPMI_SENSOR_TYPE_VALID(rec->get_sensor_type())) {
                    ss << "\'" << ipmi_sensor_types[rec->get_sensor_type()] << "\' (" <<
                        (unsigned int) rec->get_sensor_type() << ")\n";
                }
                else
                    ss << "\'OEM\' (" << (unsigned int) rec->get_sensor_type() << ")\n";
                ss << " * Event/Reading Type Code: \'" << get_sensor_event_reading_type_code(
                        (unsigned int) rec->get_event_reading_type_code()) <<
                    "\' (" << (unsigned int) rec->get_event_reading_type_code() << ")\n";
                ss << '\n';
            count++;
        }
    }
    return ss.str() + "\n";
}

template<typename T>
void FruDevLocatorRec::parse_sensors(T &sensor_list) {
    
    for(auto &rec: sensor_list) {
        if(rec->get_entity_id() == this->fru_entity_id) {
            if(rec->get_entity_instance() == this->fru_entity_instance)
                this->sensor_records.push_back(rec);
        }
    }
}

void parse_args(int argc, char const *argv[], std::map<std::string,std::string> &m) {
    
    std::vector<std::string> args;
    for(int x = 1; x<argc; x++){
        args.push_back(argv[x]);
    }

    std::vector<std::string>::iterator itr = args.begin();
    while(itr != args.end()){
        if(*itr == "-h" || *itr == "--host") {
            m["-h"] = *(++itr);
        }
        itr++;
        ///std::cout << arg << std::endl;
    }
    for(auto &x : m) {
        std::cout << "Map->first: " << x.first << ", Map->secod: " << x.second << std::endl;
    }
}

int main(int argc, char const *argv[]) {
    
    EpAiRecord ai = EpAiRecord("$(P):S09_Temp1", "65:0:0:107", "1 second", "F", "1");
    //std::map<std::string,std::string> args;
    //parse_args(argc, argv, args);
    //return;
    const char *hostname = "192.168.201.141";
    const char *username = "";
    const char *password = "";
    const char *authType = IPMI_AUTHENTICATION_TYPE_NONE;
    const char *privLevel = IPMI_PRIVILEGE_LEVEL_ADMIN;
    int sessionTimeout = IPMI_SESSION_TIMEOUT_DEFAULT;
    int retransmissionTimeout = IPMI_RETRANSMISSION_TIMEOUT_DEFAULT;
    int workaroundFlags = IPMI_WORKAROUND_FLAGS_OUTOFBAND_AUTHENTICATION_CAPABILITIES;
    int flags = IPMI_FLAGS_DEFAULT;

    ipmi_ctx_t ipmi{nullptr};
    ipmi = ipmi_ctx_create();

    int connected = ipmi_ctx_open_outofband(
                    ipmi, hostname, username, password,
                    authType, privLevel,
                    sessionTimeout, retransmissionTimeout, workaroundFlags, flags);
    
    printf("connected: %i\n", connected);

    std::string sdr_cache_path = "/tmp/ipmi_sdr_xxx.cache";
    ipmi_sdr_ctx_t sdr{nullptr};
    sdr = ipmi_sdr_ctx_create();

    if (ipmi_sdr_cache_open(sdr, ipmi, sdr_cache_path.c_str()) < 0) {
        switch (ipmi_sdr_ctx_errnum(sdr)) {
        case IPMI_SDR_ERR_CACHE_OUT_OF_DATE:
        case IPMI_SDR_ERR_CACHE_INVALID:
            printf("deleting out of date or invalid SDR cache file %s\n",sdr_cache_path.c_str());
            (void)ipmi_sdr_cache_delete(sdr, sdr_cache_path.c_str());
            // fall thru
        case IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST:
            printf("creating new SDR cache file %s\n", sdr_cache_path.c_str());
            (void)ipmi_sdr_cache_create(sdr, ipmi, sdr_cache_path.c_str(), IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT, nullptr, nullptr);
            break;
        default:
            throw std::runtime_error("can't open SDR cache - " + std::string(ipmi_ctx_errormsg(ipmi)));
        }

        if (ipmi_sdr_cache_open(sdr, ipmi, sdr_cache_path.c_str()) < 0)
            throw std::runtime_error("can't open SDR cache - " + std::string(ipmi_ctx_errormsg(ipmi)));
    }

    uint16_t record_count;
    int rv = ipmi_sdr_cache_record_count (sdr, &record_count);
    printf("**** Rec Count: %u\n", record_count);

    const void *sdr_record = NULL;
    unsigned int sdr_record_len = 0;
    uint16_t record_id = 0;
    uint8_t record_type = 0;

    std::list<uint16_t> mcdlr;
    std::list<std::shared_ptr<FruDevLocatorRec>> fruDevLocRecList;
    std::list<uint16_t> evntr;
    std::list<std::shared_ptr<SensorRecFull>> sensRecFullList;
    std::list<std::shared_ptr<SensorRecCompact>> sensRecCompactList;

    for(int i = 0; i < record_count; i++, ipmi_sdr_cache_next(sdr)) {
	    rv = ipmi_sdr_parse_record_id_and_type (sdr,
                            sdr_record,
                            sdr_record_len,
                            &record_id,
                            &record_type);

        char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
        rv = ipmi_sdr_parse_device_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);

        /** printf("**** rv: %i, record_id: %u, name: \'%s\', record_type: %s(%u)\n",
                rv, record_id, id_str, sdr_type_itos_map[record_type].c_str(), record_type);*/

        try {
		    
		    if(record_type == IPMI_SDR_FORMAT_FULL_SENSOR_RECORD) {
                sensRecFullList.push_back(std::make_shared<SensorRecFull>(sdr, record_id, record_type));
            }

		    if(record_type == IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD) {
                sensRecCompactList.push_back(std::make_shared<SensorRecCompact>(sdr, record_id, record_type));
            }

		    if(record_type == IPMI_SDR_FORMAT_EVENT_ONLY_RECORD) {
                evntr.push_back(record_id);
            }

            uint8_t container_entity_id = 0;
            uint8_t container_entity_instance = 0;
            if(record_type == IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD) {
                rv = ipmi_sdr_parse_container_entity (sdr, NULL, 0, &container_entity_id, &container_entity_instance);
                printf("0000 rv: %i, container_entity_id: %u, container_entity_instance: %u\n",
                        rv, container_entity_id, container_entity_instance);
            }

		    if(record_type == IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
                fruDevLocRecList.push_back(std::make_shared<FruDevLocatorRec>(sdr, record_id, record_type));
            }

            if(record_type == IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD) {
		        mcdlr.push_back(record_id);

                uint8_t entity_id = 0;
                uint8_t entity_instance = 0;
                uint8_t entity_instance_type = 0;

                rv = ipmi_sdr_parse_entity_id_instance_type (sdr, NULL, 0, &entity_id, &entity_instance, &entity_instance_type);
                printf("1111 rv: %i, entity_id: %u, entity_instance: %u, entity_instance_type: %u\n",
                        rv, entity_id, entity_instance, entity_instance_type);
	        }

	    }
	    
	    catch (std::exception &e) {
		    std::cout << e.what() << std::endl;
		    return 0;
	    }

    }

    printf("mcdlr: %i, fruDevLocRecList: %i, evntr: %i, SensRecFullList: %i, SensRecCompactList: %i\n", mcdlr.size(), fruDevLocRecList.size(), evntr.size(), sensRecFullList.size(), sensRecCompactList.size());

    for(std::shared_ptr<FruDevLocatorRec> &fdlr: fruDevLocRecList) {
        fdlr->parse_sensors(sensRecFullList);
        fdlr->parse_sensors(sensRecCompactList);
        std::cout << fdlr->report();

    }

    
}















