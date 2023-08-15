/*
 *
 *
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <freeipmi/freeipmi.h>
#include "EpAiRecord.h"
#include "IpmiSensorRecFull.h"
#include "IpmiFruDevLocRec.h"


std::map<std::string,std::string> cli_args_map;
std::map<std::string, std::string>::const_iterator itr;

uint8_t authType = IPMI_AUTHENTICATION_TYPE_NONE;
uint8_t privLevel = IPMI_PRIVILEGE_LEVEL_ADMIN;

int sessionTimeout = IPMI_SESSION_TIMEOUT_DEFAULT;
int retransmissionTimeout = IPMI_RETRANSMISSION_TIMEOUT_DEFAULT;
int workaroundFlags = IPMI_WORKAROUND_FLAGS_OUTOFBAND_AUTHENTICATION_CAPABILITIES;
int flags = IPMI_FLAGS_DEFAULT;

const char *hostname = NULL;
const char *username = NULL;
const char *password = NULL;

ipmi_ctx_t ipmi{nullptr};

const std::string sdr_cache_path = "/tmp/ipmi_sdr_xxx.cache";
ipmi_sdr_ctx_t sdr{nullptr};

std::vector<uint16_t> mcdlr;
std::vector<std::shared_ptr<IpmiFruDevLocRec>> fruDevLocRecList;
std::vector<uint16_t> evntr;
std::vector<std::shared_ptr<IpmiSensorRecFull>> sensRecFullList;
std::vector<std::shared_ptr<IpmiSensorRecComp>> sensRecCompactList;

void parse_args(int argc, char const *argv[], std::map<std::string,std::string> &m) {

    for(int i = 1; i<argc; i++){
        if(argv[i][0] == '-') {
            /** this is a key, now look ahead for value...*/
            const char *key = argv[i];
            if(i+1 < argc) {
                /** is this a key/value pair?*/
                if(argv[i+1][0] != '-') {
                    m.insert({key, argv[++i]});
                }
                /** Or is this two separate keys/command-line switches...?*/
                else
                    m.insert({key, "null"});
            }
            else {
                m.insert({key, "null"});
            }
        }
    }

}

void ipmi_init() {

    itr = cli_args_map.find("-H");
    if(itr != cli_args_map.end()) {
        hostname = itr->second.c_str();
    }
    else
        throw std::invalid_argument(
            "Error: A hostname/IP-address must be provided! Use the -H switch and provide a valid hostname/IP-address.");

    itr = cli_args_map.find("-u");
    if(itr != cli_args_map.end()) {
        username = itr->second.c_str();
    }
    else
        throw std::invalid_argument(
            "Error: A username must be provided! Use the -u switch and provide a valid username.");

    itr = cli_args_map.find("-p");
    if(itr != cli_args_map.end()) {
        password = itr->second.c_str();
    }
    else
        throw std::invalid_argument(
            "Error: A password must be provided! Use the -p switch and provide a valid password.");
    
    ipmi = ipmi_ctx_create();
    if(ipmi == nullptr) {
        throw std::logic_error(
            "Error: Could not create ipmi context using \'ipmi_ctx_create()\'.");
    }

    sdr = ipmi_sdr_ctx_create();
    if(sdr == nullptr) {
        throw std::logic_error(
            "Error: Could not create sdr context using \'ipmi_sdr_ctx_create()\'.");
    }
}

void ipmi_connect() {
    int rv = ipmi_ctx_open_outofband(
        ipmi, hostname, username, password,
        authType, privLevel,
        sessionTimeout, retransmissionTimeout, workaroundFlags, flags);
    
    if(rv < 0) {
        throw std::logic_error(
            "Error: Could not create an ipmi connection using \'ipmi_ctx_open_outofband())\'.");
    }
}

void ipmi_open_cache() {
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
            throw std::runtime_error("ERROR: Can't open SDR cache - " + std::string(ipmi_ctx_errormsg(ipmi)));
        }

        if (ipmi_sdr_cache_open(sdr, ipmi, sdr_cache_path.c_str()) < 0)
            throw std::runtime_error("ERROR: Can't open SDR cache - " + std::string(ipmi_ctx_errormsg(ipmi)));
    }
}

void ipmi_parse_sdr() {

    uint16_t record_count;
    int rv = ipmi_sdr_cache_record_count (sdr, &record_count);
    printf("**** Rec Count: %u\n", record_count);

    const void *sdr_record = NULL;
    unsigned int sdr_record_len = 0;
    uint16_t record_id = 0;
    uint8_t record_type = 0;

    for(int i = 0; i < record_count; i++, ipmi_sdr_cache_next(sdr)) {
	    rv = ipmi_sdr_parse_record_id_and_type (sdr,
                            sdr_record,
                            sdr_record_len,
                            &record_id,
                            &record_type);

        char id_str[IPMI_SDR_MAX_SENSOR_NAME_LENGTH] = {'\0'};
        rv = ipmi_sdr_parse_device_id_string (sdr, NULL, 0, &id_str[0], IPMI_SDR_MAX_SENSOR_NAME_LENGTH);

        if(record_type == IPMI_SDR_FORMAT_FULL_SENSOR_RECORD) {
            sensRecFullList.push_back(std::make_shared<IpmiSensorRecFull>(sdr, record_id, record_type));
        }

        if(record_type == IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD) {
            sensRecCompactList.push_back(std::make_shared<IpmiSensorRecComp>(sdr, record_id, record_type));
        }

        if(record_type == IPMI_SDR_FORMAT_EVENT_ONLY_RECORD) {
            ///TODO: Handle Event type Records...
            ///evntr.push_back(record_id);
        }

        if(record_type == IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD) {
            ///TODO: Handle Device Relative Entity Association Records...
            ///rv = ipmi_sdr_parse_container_entity (sdr, NULL, 0, &container_entity_id, &container_entity_instance);
        }

        if(record_type == IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
            fruDevLocRecList.push_back(std::make_shared<IpmiFruDevLocRec>(sdr, record_id, record_type));
        }

        if(record_type == IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD) {
            ///TODO: Handle Management Controller Device Locator Records...
            ///mcdlr.push_back(record_id);
            ///rv = ipmi_sdr_parse_entity_id_instance_type (sdr, NULL, 0, &entity_id, &entity_instance, &entity_instance_type);
        }

    }
    ///printf("mcdlr: %i, fruDevLocRecList: %i, evntr: %i, SensRecFullList: %i, SensRecCompactList: %i\n", mcdlr.size(), fruDevLocRecList.size(), evntr.size(), sensRecFullList.size(), sensRecCompactList.size());

}

int main(int argc, char const *argv[]) {
    
    try {
        parse_args(argc, argv, cli_args_map);

        ///FYI: Test-Host (VT811) IP is "192.168.201.141";
        ipmi_init();
        ipmi_connect();
        ipmi_open_cache();
        ipmi_parse_sdr();
        

        
        for(std::shared_ptr<IpmiFruDevLocRec> &fdlr: fruDevLocRecList) {
            fdlr->parse_sensors(sensRecFullList);
            fdlr->parse_sensors(sensRecCompactList);
            std::cout << fdlr->report();
        }
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    
}















