/**
 * 
 * 
 * 
 * 
*/

#include "EpAiRecord.h"

const std::vector<std::string> EpAiRecord::validfields = {
    "EGU",
    "PREC",
    "HIHI",
    "HIGH",
    "LOW",
    "LOLO",
    "HHSV",
    "HSV",
    "LSV",
    "LLSV"
};

EpAiRecord::EpAiRecord(std::string name, std::string inout, std::string scanrate,
    std::string egu, std::string prec)
: EpRecord(rec_type::AI, link_type::INP, name, inout, scanrate)
{
    if(egu.size() > 0) {
        add_field("EGU", egu);
    }
    add_field("PREC", prec);
}

EpAiRecord::~EpAiRecord()
{
}

int EpAiRecord::add_field(std::string fieldname, std::string fieldvalue) {
    return EpRecord::add_field(EpAiRecord::validfields, fieldname, fieldvalue);
}
