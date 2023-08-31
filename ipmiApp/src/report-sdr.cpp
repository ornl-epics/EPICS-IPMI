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
#include <fstream>
#include <freeipmi/freeipmi.h>
#include "EpRecord.h"
#include "IpmiSensorRecFull.h"
#include "IpmiFruDevLocRec.h"


std::map<std::string,std::string> cli_args_map;
std::map<std::string, std::string>::const_iterator itr;

uint8_t authType = IPMI_AUTHENTICATION_TYPE_MD5;
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
std::string report_file_name;
std::ofstream reportfile;
std::string epics_db_file_name;
std::ofstream dbfile;
ipmi_sdr_ctx_t sdr{nullptr};

uint16_t record_count = 0;
uint8_t SdrVersion = 0;
uint32_t SdrAdditionTimestamp = 0;
uint32_t SdrEraseTimestamp = 0;

std::vector<uint16_t> mcdlr;
std::vector<std::shared_ptr<IpmiFruDevLocRec>> fruDevLocRecList;
std::vector<uint16_t> evntr;
std::vector<std::shared_ptr<IpmiSensorRecFull>> sensRecFullList;
std::vector<std::shared_ptr<IpmiSensorRecComp>> sensRecCompactList;
std::vector<std::shared_ptr<IpmiSensorRecComp>> orphandList;

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
    
    itr = cli_args_map.find("--create-report-file");
    if(itr != cli_args_map.end()) {
        report_file_name = itr->second.c_str();
        reportfile.open(report_file_name);
    }

    itr = cli_args_map.find("--create-db-file");
    if(itr != cli_args_map.end()) {
        epics_db_file_name = itr->second.c_str();
        dbfile.open(epics_db_file_name);
    }

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

        /* Get the error number from the context. */
        rv = ipmi_ctx_errnum (ipmi);

        /* Match the error number up with an error string. */
        char *str_error = ipmi_ctx_strerror (rv);
        std::cout << str_error << std::endl;

        /* Get the error message associated with the context. This has been the same as the strerror */
        str_error = ipmi_ctx_errormsg (ipmi);
        std::cout << str_error << std::endl;

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

    if(ipmi_sdr_cache_sdr_version (sdr, &SdrVersion) < 0)
        throw std::runtime_error("Error! Could not read SDR cache version.");

    /* Get the SDR most recent addition timestamp */
    if(ipmi_sdr_cache_most_recent_addition_timestamp (sdr, &SdrAdditionTimestamp) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent addition timestamp.");

    /* Get the SDR most recent erase timestamp */
    if(ipmi_sdr_cache_most_recent_erase_timestamp (sdr, &SdrEraseTimestamp) < 0)
        throw std::runtime_error("Error! Could not read SDR cache most recent erase timestamp.");

    /* Get the SDR record count */
    if(ipmi_sdr_cache_record_count (sdr, &record_count) < 0)
        throw std::runtime_error("Error! Could not read SDR cache record count.");

    const void *sdr_record = NULL;
    unsigned int sdr_record_len = 0;
    uint16_t record_id = 0;
    uint8_t record_type = 0;

    for(int i = 0; i < record_count; i++, ipmi_sdr_cache_next(sdr)) {
	    int rv = ipmi_sdr_parse_record_id_and_type (sdr,
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
}

int main(int argc, char const *argv[]) {
    
    try {
        parse_args(argc, argv, cli_args_map);

        ///FYI: Test-Host (VT811) IP is "192.168.201.141";

        ipmi_init();
        ipmi_connect();
        ipmi_open_cache();
        ipmi_parse_sdr();
        
        /* Print the Header information */
        if(reportfile.is_open()) {
            reportfile << hostname << " SDR Info {" << std::endl;
            reportfile << " * SDR Record Count: " << record_count << "," << std::endl;
            reportfile << " * SDR Version: " << (unsigned) SdrVersion << "," << std::endl;
            reportfile << " * SDR Addition Timestamp: " << (unsigned) SdrAdditionTimestamp << "," << std::endl;
            reportfile << " * SDR Erase Timestamp: " << (unsigned) SdrEraseTimestamp << std::endl;
            reportfile << "}" << std::endl;
        }
        else {
            std::cout << hostname << " SDR Info {" << std::endl;
            std::cout << " * SDR Record Count: " << record_count << "," << std::endl;
            std::cout << " * SDR Version: " << (unsigned) SdrVersion << "," << std::endl;
            std::cout << " * SDR Addition Timestamp: " << (unsigned) SdrAdditionTimestamp << "," << std::endl;
            std::cout << " * SDR Erase Timestamp: " << (unsigned) SdrEraseTimestamp << std::endl;
            std::cout << "}" << std::endl;
        }

        std::copy(sensRecFullList.begin(), sensRecFullList.end(), std::back_inserter(orphandList));
        std::copy(sensRecCompactList.begin(), sensRecCompactList.end(), std::back_inserter(orphandList));

        /* Look for FRU associations */
        for(auto &fdlr: fruDevLocRecList) {

            fdlr->parseAssociations(orphandList);
            if(reportfile.is_open()) {
                reportfile << fdlr->report();
            }
            else
                std::cout << fdlr->report();
        }

        std::cout << "Full: " << sensRecFullList.size() << ", Compact: " << 
        sensRecCompactList.size() << ", orphandList: " << orphandList.size() << std::endl;

        /* Print the sensors that are not associated with FRUs */
        for(auto &rec : orphandList) {
            std::cout << rec->to_string();
        }

        if(dbfile.is_open()) {
            std::vector<std::shared_ptr<EpRecord>> eprList;
            for(auto &obj : fruDevLocRecList) {
                
                for(auto &sensor : obj->get_sensors()) {
                    std::shared_ptr<EpRecord> epr = EpRecord::create(obj->get_device_slave_address(), sensor);
                    eprList.push_back(epr);
                    dbfile << epr->to_string() << std::endl;
                }
            }
            
            for(auto &sensor : orphandList) {
                std::shared_ptr<EpRecord> epr = EpRecord::create(-1, sensor);
                eprList.push_back(epr);
                dbfile << epr->to_string() << std::endl;
            }
        }

    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    if(reportfile.is_open()) {
        reportfile.close();
    }
    if(dbfile.is_open()){
        dbfile.close();
    }
}















