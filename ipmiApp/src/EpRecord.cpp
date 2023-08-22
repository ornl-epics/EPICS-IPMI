/** 
 * 
 * 
 * 
 * 
*/

#include "EpRecord.h"
#include "EpAiRecord.h"
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

std::shared_ptr<EpRecord> EpRecord::create(const uint16_t fru_addr, const IpmiSensorRecComp &irecord) {
    
    std::string dev_id_str = irecord.get_device_id_string();
    /** Replace all spaces (' ') with underscores ('_').*/
    std::replace(dev_id_str.begin(), dev_id_str.end(), ' ', '_');

    std::string name = "$(P):FRU";
    name += std::to_string(fru_addr);
    name += "_";
    name += dev_id_str;

    std::string inout = "@<dev> F";
    inout += std::to_string(fru_addr);
    inout += " S";
    inout += std::to_string(irecord.get_sensor_number());

    std::string egu = "";
    if(irecord.get_sensor_base_unit_type_str() != "unspecified") {
        egu += irecord.get_sensor_base_unit_type_str();
    }
    
    return std::make_shared<EpRecord>(EpAiRecord(name, inout, "1 second", egu, "1"));
}