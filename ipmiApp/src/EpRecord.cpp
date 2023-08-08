/** 
 * 
 * 
 * 
 * 
*/

#include "EpRecord.h"

EpRecord::EpRecord(rec_type rtyp, link_type d, std::string name, std::string inout, std::string scanrate)
: rectyp(rtyp), linktype(d)
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
            std::cout << "valid name" << std::endl;
            this->fields[this->fields.size()][field_name] = field_value;
            return 0;
        }
    }
    std::cout << "Invalid name" << std::endl;
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
