/** 
 * 
 * 
 * 
 * 
*/

#include "EpRecord.h"
#include "EpAiRecord.h"
#include "EpBiRecord.h"
#include <algorithm>
#include <sstream>


EpRecord::EpRecord(rec_type rtyp, link_type d, std::string name, std::string inout, std::string scanrate)
: linktype(d), rectyp(rtyp)
{
    this->fields[0]["NAME"] = name;
    this->fields[1]["DTYP"] = "ipmi";
    if(d == link_type::INP) {
        this->fields[2]["INP"] = inout;
    }
    else if (d == link_type::OUT) {
        this->fields[2]["OUT"] = inout;
    }
    else
        this->fields[2]["???"] = inout;
        
    this->fields[3]["SCAN"] = scanrate;
}

EpRecord::~EpRecord() {

};

int EpRecord::add_field(const std::vector<std::string> &valid_fields, const std::string field_name, const std::string field_value) {
    
    for(auto &vf: valid_fields) {
        if(field_name == vf) {
            this->fields[this->fields.size()][field_name] = field_value;
            return 0;
        }
    }
    return -1;
}

std::string EpRecord::to_string() {
    std::stringstream ss;
    rec_fields::iterator itr = this->fields.begin();
    ss << "record(" << rec_type_str.at(this->rectyp) << ", \"" << (itr++)->second.begin()->second << "\") {\n";
    while(itr != this->fields.end()) {
        ss << " field(" << itr->second.begin()->first << ", \"" << itr->second.begin()->second << "\")\n";
        itr++;
    }
    ss << "}\n\n";
    return ss.str();
}

std::shared_ptr<EpRecord> EpRecord::create(const int fru_addr, std::shared_ptr<IpmiSensorRecComp> irecord) {
    
    std::string dev_id_str = irecord->get_device_id_string();

    /** Replace all spaces (' '), ('.'), and ('-') with underscores ('_').
     * For some reason IPMI devices like to use those characters in their
     * ID-string names.
    */
    std::replace_if(dev_id_str.begin(), dev_id_str.end(), [](char ch) {
        return (ch == '.' || ch == ' ' || ch == '-') ? true : false;
    }, '_');

    std::string name = "$(P):";
    if(fru_addr >= 0) {
        name += "FRU";
        name += std::to_string(fru_addr);
        name += "_";
    }

    name += dev_id_str;

    std::string inout = "@<dev> sensor ";
    inout += std::to_string(irecord->get_entity_id());
    inout += ":";
    inout += std::to_string(irecord->get_entity_instance());
    inout += " \'";
    ///inout += std::to_string(irecord.get_sensor_number());
    inout += irecord->get_device_id_string();
    inout += "\'";

    std::string egu = "";
    if(irecord->get_sensor_base_unit_type_str() != "unspecified") {
        egu += irecord->get_sensor_base_unit_type_str();
    }
    
    return std::make_shared<EpRecord>(EpAiRecord(name, inout, "1 second", egu, "1"));
}

std::shared_ptr<EpRecord> EpRecord::create(const int fru_addr, std::shared_ptr<PicmgLed> picmg_led) {

    std::string name = "$(P):";
    if(fru_addr >= 0) {
        name += "FRU";
        name += std::to_string(fru_addr);
        name += "_";
    }
    
    name += "LED_";
    name += picmg_led.get()->getLedColor();

    std::string inout = "@<dev> PICMG_LED ";
    inout += std::to_string(picmg_led.get()->getLogicalFruDeviceDeviceSlaveAddress());
    inout += ":";
    inout += std::to_string(picmg_led.get()->getLedId());

    std::string egu = "";
    
    return std::make_shared<EpRecord>(EpBiRecord(name, inout, "1 second", "OFF", "ON"));
}