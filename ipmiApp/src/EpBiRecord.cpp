/**
 * 
 * 
 * 
 * 
*/

#include "EpBiRecord.h"

const std::vector<std::string> EpBiRecord::validfields = {
    "DESC",
    "ZNAM",
    "ONAM",
    "ZSV",
    "OSV"
};

EpBiRecord::EpBiRecord(std::string name, std::string inout, std::string scanrate,
    std::string znam, std::string onam)
: EpRecord(rec_type::BI, link_type::INP, name, inout, scanrate)
{
    if(znam.size() > 0) {
        add_field("ZNAM", znam);
    }
    if(onam.size() > 0) {
        add_field("ONAM", onam);
    }
}

EpBiRecord::~EpBiRecord()
{
}

int EpBiRecord::add_field(std::string fieldname, std::string fieldvalue) {
    return EpRecord::add_field(EpBiRecord::validfields, fieldname, fieldvalue);
}
