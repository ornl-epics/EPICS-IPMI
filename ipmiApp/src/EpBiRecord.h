/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_EPBIRECORD_H_
#define IPMIAPP_SRC_EPBIRECORD_H_

#include "EpRecord.h"

class EpBiRecord : public EpRecord
{
private:
    static const std::vector<std::string> validfields;
public:
    EpBiRecord(std::string name, std::string inout, std::string scanrate,
    std::string znam, std::string onam);
    ~EpBiRecord();
    int add_field(std::string fieldname, std::string fieldvalue);
};

#endif