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
#include "IpmiSdrDefs.h"


std::map<std::string,std::string> cli_args_map;
std::map<std::string, std::string>::const_iterator itr;

uint8_t authType = IPMI_AUTHENTICATION_TYPE_NONE;
uint8_t privLevel = IPMI_PRIVILEGE_LEVEL_NO_ACCESS;


int sessionTimeout = IPMI_SESSION_TIMEOUT_DEFAULT;
int retransmissionTimeout = IPMI_RETRANSMISSION_TIMEOUT_DEFAULT;
int workaroundFlags = IPMI_WORKAROUND_FLAGS_OUTOFBAND_AUTHENTICATION_CAPABILITIES;
int flags = IPMI_FLAGS_DEFAULT;

const char *hostname = NULL;
const char *username = NULL;
const char *password = NULL;

ipmi_ctx_t ipmi{nullptr};

const std::string sdr_cache_path = "/tmp/epics_report_sdr.cache";
bool create_report_file = false;
std::string report_file_name;
bool create_epics_db_file = false;
std::string epics_db_file_name;
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

void print_help() {
    std::stringstream ss;
    ss << "\n";
    ss << "Usage: report-sdr [-H hostname or ip address] [-u username] [-p password]\n";
    ss << "[--auth-type [none, plain, md2, md5]] [--privilege-level [admin, operator, user]]\n";
    ss << "[--create-report-file [FILE NAME]] [--create-db-file [FILE NAME]]\n\n";
    ss << "Description\n";
    ss << "    Read the SDR (Sensor Device Repository) and display the sensor records.\n\n";
    ss << "Options:\n";
    ss << "    -H Hostname or IP address (DNS name or 123.456.789.123)\n";
    ss << "    -u Username\n";
    ss << "    -p Password\n";
    ss << "    --auth-type Authentication Type (none, plain, md2, md5)\n";
    ss << "    --privilege-level Privilege Level (admin, operator, user)\n";
    ss << "    --create-report-file Create a file that contains the SDR data\n";
    ss << "    --create-db-file Create an EPICS database file from the SDR data\n\n";
    ss << "Examples:\n";
    ss << "To print the SDR to the console:\n";
    ss << "./report-sdr -H 192.168.201.205 -u \"ADMIN\" -p \"Password0\" --auth-type md5 --privilege-level admin\n\n";
    ss << "To create an EPICS database from the SDR:\n";
    ss << "./report-sdr -H 192.168.201.205 -u \"ADMIN\" -p \"Password0\" --auth-type md5 --privilege-level admin ";
    ss << "--create-db-file /tmp/server.db\n";
    std::cout << ss.str() << std::endl;
    
    exit(EXIT_SUCCESS);
}

void ipmi_init() {

    itr = cli_args_map.find("--help");
    if(itr != cli_args_map.end()) {
        print_help();
    }
    
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
    
    itr = cli_args_map.find("--auth-type");
    if(itr != cli_args_map.end()) {
        if (cli_args_map["--auth-type"] == "none")
            authType = IPMI_AUTHENTICATION_TYPE_NONE;
        else if (cli_args_map["--auth-type"] == "plain" || cli_args_map["--auth-type"] == "straight_password_key")
            authType = IPMI_AUTHENTICATION_TYPE_STRAIGHT_PASSWORD_KEY;
        else if (cli_args_map["--auth-type"] == "md2")
            authType = IPMI_AUTHENTICATION_TYPE_MD2;
        else if (cli_args_map["--auth-type"] == "md5")
            authType = IPMI_AUTHENTICATION_TYPE_MD5;
        else
            throw std::runtime_error("invalid authentication type (choose from none,plain,md2,md5)");
    }

    itr = cli_args_map.find("--privilege-level");
    if(itr != cli_args_map.end()) {
        if (cli_args_map["--privilege-level"] == "admin")
            privLevel = IPMI_PRIVILEGE_LEVEL_ADMIN;
        else if (cli_args_map["--privilege-level"] == "operator")
            privLevel = IPMI_PRIVILEGE_LEVEL_OPERATOR;
        else if (cli_args_map["--privilege-level"] == "user")
            privLevel = IPMI_PRIVILEGE_LEVEL_USER;
        else
            throw std::runtime_error("invalid privilege level (choose from user,operator,admin)");
    }

    itr = cli_args_map.find("--create-report-file");
    if(itr != cli_args_map.end()) {
        create_report_file = true;
        report_file_name = itr->second.c_str();
    }

    itr = cli_args_map.find("--create-db-file");
    if(itr != cli_args_map.end()) {
        create_epics_db_file = true;
        epics_db_file_name = itr->second.c_str();
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
        std::string str_error = ipmi_ctx_strerror (rv);

        /* Get the error message associated with the context. This has been the same as the strerror */
        std::string str_errmsg = ipmi_ctx_errormsg (ipmi);

        /* Not sure if ipmi_ctx_strerror() and ipmi_ctx_errormsg() always return the same messages.
        *  So, use Lambda to concat strings if they are different...
        */
        auto getErrStr = [&str_error, &str_errmsg]() {
            if(str_error.compare(str_errmsg) != 0) {
                return "\'" + str_error + "\' Error Message: \'" + str_errmsg + "\'";
            }
            else
                return "\'" + str_error + "\'";
        };

        throw std::runtime_error (
            "Error: Could not create an ipmi connection using \'ipmi_ctx_open_outofband())\' Using:\n"
            " * Hostname: \'" + std::string(hostname) + "\'\n"
            " * Username: \'" + std::string(username) + "\'\n"
            " * Password: \'" + std::string(password) + "\'\n"
            " * Auth-Type: \'" + getAuthenticationString(authType) + "\'\n"
            " * Privilege Level: \'" + getPrivilegeLevelString(privLevel) + "\'\n"
            " * Sesion Timout: \'" + std::to_string(sessionTimeout) + "\'\n"
            " * Retransmition Timeout: \'" + std::to_string(retransmissionTimeout) + "\'\n"
            " * Workaround Flags: \'" + std::to_string(workaroundFlags) + "\'\n"
            " * Flags: \'" + std::to_string(flags) + "\'\n"
            "Error Code: \'" + std::to_string(rv) + "\' Error String: " + getErrStr());
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
        }

        if(record_type == IPMI_SDR_FORMAT_DEVICE_RELATIVE_ENTITY_ASSOCIATION_RECORD) {
            ///TODO: Handle Device Relative Entity Association Records...
        }

        if(record_type == IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
            fruDevLocRecList.push_back(std::make_shared<IpmiFruDevLocRec>(ipmi, sdr, record_id, record_type));
        }

        if(record_type == IPMI_SDR_FORMAT_MANAGEMENT_CONTROLLER_DEVICE_LOCATOR_RECORD) {
            ///TODO: Handle Management Controller Device Locator Records...
        }
    }
}

void write_db_file() {

    std::ofstream dbfile;

    dbfile.open(epics_db_file_name);

    if(!dbfile.is_open())
        throw std::runtime_error("ERROR! Cannot create EPICS .db file: \'" + epics_db_file_name + "\'");

    std::vector<std::shared_ptr<EpRecord>> eprList;

    for(auto &obj : fruDevLocRecList) {
        /** TODO: Finish PICMG_LED support
         * 
        for(auto &led : obj.get()->getStatusLeds()) {
            std::shared_ptr<EpRecord> epr = EpRecord::create(obj->get_device_slave_address(), led);
            eprList.push_back(epr);
            dbfile << epr->to_string() << std::endl;
        }
        */
        
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

    dbfile.close();
}

void write_report_file() {
    
    std::ofstream reportfile;
    reportfile.open(report_file_name);

    if(!reportfile.is_open())
        throw std::runtime_error("ERROR! Cannot create report file: \'" + report_file_name + "\'");

    reportfile << hostname << " SDR Info {" << std::endl;
    reportfile << " * SDR Record Count: " << record_count << "," << std::endl;
    reportfile << " * SDR Version: " << (unsigned) SdrVersion << "," << std::endl;
    reportfile << " * SDR Addition Timestamp: " << (unsigned) SdrAdditionTimestamp << "," << std::endl;
    reportfile << " * SDR Erase Timestamp: " << (unsigned) SdrEraseTimestamp << std::endl;
    reportfile << "}" << std::endl;

    /* Print the sensors that are not associated with FRUs */
    for(auto &fdlr: fruDevLocRecList) {
        reportfile << fdlr->report();
    }

    int count = 1;
    for(auto &rec : orphandList) {
        reportfile << "[" << count++ << "]" << std::endl;
        reportfile << rec->to_string();
    }

    reportfile.close();
}

int main(int argc, char const *argv[]) {
    
    try {
        parse_args(argc, argv, cli_args_map);

        ///FYI: Test-Host (VT811) IP is "192.168.201.141";

        ipmi_init();
        ipmi_connect();
        ipmi_open_cache();
        ipmi_parse_sdr();

        /* Copy the compact and full sensor list into the orphand list. The orphand list will hold
         * the sensors that are not associated with any FRUs
         */
        std::copy(sensRecFullList.begin(), sensRecFullList.end(), std::back_inserter(orphandList));
        std::copy(sensRecCompactList.begin(), sensRecCompactList.end(), std::back_inserter(orphandList));

        /* Iterate over the FRU Device Locator List and remove sensors from the orphand list that are
         * associated with the FRU. The IpmiFruDevLocRec has its own list of sensor associations.
         */
        for(auto &fdlr: fruDevLocRecList) {
            fdlr->parseAssociations(orphandList);
        }

        if(create_report_file)
            write_report_file();

        if(create_epics_db_file)
            write_db_file();
        
        /* Print the Header information */
        if(!create_epics_db_file && !create_report_file) {
            std::cout << hostname << " SDR Info {" << std::endl;
            std::cout << " * SDR Record Count: " << record_count << "," << std::endl;
            std::cout << " * SDR Version: " << (unsigned) SdrVersion << "," << std::endl;
            std::cout << " * SDR Addition Timestamp: " << (unsigned) SdrAdditionTimestamp << "," << std::endl;
            std::cout << " * SDR Erase Timestamp: " << (unsigned) SdrEraseTimestamp << std::endl;
            std::cout << "}" << std::endl;

            /* Print the sensors that are not associated with FRUs */
            for(auto &fdlr: fruDevLocRecList) {
                std::cout << fdlr->report();
            }
            
            int count = 1;
            for(auto &rec : orphandList) {
                std::cout << "[" << count++ << "]" << std::endl;
                std::cout << rec->to_string();
            }
        }
        
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    
}















