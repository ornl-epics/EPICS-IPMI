/** 
 * 
 * 
 * 
 */

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>

enum link_type {
    INP = 0,
    OUT = 1
};

enum rec_type {
    AI = 0,
    AO = 1,
    BI = 2,
    BO = 3,
    MBBI = 4,
    MBBO = 5
};

const std::vector<std::string> rec_type_str = {
    "ai",
    "ao",
    "bi",
    "bo",
    "mbbi",
    "mbbo"
};

typedef std::map<int, std::map<std::string, std::string>> rec_fields;

class EpRecord
{
protected:
    rec_fields fields;
    const link_type linktype;
    const rec_type rectyp;
public:
    EpRecord(rec_type, link_type, std::string name, std::string inout, std::string scanrate);
    ~EpRecord();
    int add_field(const std::vector<std::string> &valid_fields, const std::string field_name, const std::string field_value);
    std::string to_string();
};