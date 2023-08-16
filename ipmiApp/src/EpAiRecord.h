/**
 * 
 * 
 * 
 * 
*/

#ifndef IPMIAPP_SRC_EPAIRECORD_H_
#define IPMIAPP_SRC_EPAIRECORD_H_

#include "EpRecord.h"

class EpAiRecord : public EpRecord
{
private:
    static const std::vector<std::string> validfields;
public:
    EpAiRecord(std::string name, std::string inout, std::string scanrate,
    std::string egu, std::string prec);
    ~EpAiRecord();
    int add_field(std::string fieldname, std::string fieldvalue);
};

#endif