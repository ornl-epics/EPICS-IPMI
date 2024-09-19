/* print.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#include "common.h"
#include "print.h"

#include <cstring>
#include <limits>
#include <iomanip>
#include <iostream>

#include <alarm.h>
#include <alarmString.h>

namespace print {

void printScanReport(const std::string& header, const std::vector<Provider::Entity>& entities)
{
    size_t len = entities.size();
    uint8_t indent = 1;
    while (len > 10) {
        indent++;
        len /= 10;
    }

    std::cout << header << std::endl;
    unsigned i = 1;
    for (auto& entity: entities) {
        std::cout << std::right << std::setw(indent) << i++ << ": ";

        auto desc = entity.getField<std::string>("DESC", "<missing desc>");
        std::cout << std::left << std::setw(41) << desc.substr(0, 41) << " ";

        auto stringValue = entity.getField<std::string>("VAL", "<UDF string value>");
        auto doubleValue = entity.getField<double>     ("VAL", std::numeric_limits<double>::min());
        auto intValue    = entity.getField<int>        ("VAL", std::numeric_limits<int>::min());
        if (intValue != std::numeric_limits<int>::min())
            std::cout << intValue << " ";
        else if (doubleValue != std::numeric_limits<double>::min())
            std::cout << std::setprecision(2) << std::fixed << doubleValue << " ";
        else if (stringValue != "<UDF string value>")
            std::cout << stringValue << " ";
        else
            std::cout << "N/A ";

        auto unit = entity.getField<std::string>("UNIT", "");
        if (unit != "")
            std::cout << unit << " ";

        auto stat = entity.getField<int>("STAT", epicsAlarmNone);
        if (stat != epicsAlarmNone && stat < ALARM_NSTATUS)
            std::cout << epicsAlarmConditionStrings[stat] << " ";

        auto sevr = entity.getField<int>("SEVR", epicsSevNone);
        if (sevr != epicsSevNone && sevr < ALARM_NSEV)
            std::cout << epicsAlarmSeverityStrings[sevr] << " ";

        std::cout << std::endl;
    }
}

static std::string _epicsEscape(const std::string& str)
{
    std::string escaped = str;
    for (auto it = escaped.begin(); it != escaped.end(); it++) {
        if (isalnum(*it) == 0 && *it != ':') {
            if (*it == ' ') *it = ':';
            else            *it = '_';
        }
    }
    return escaped;
}

std::string getRecordType(const Provider::Entity& entity)
{
    static const std::vector<std::string> fields = {
        "ZRVL", "ZRST", "ONVL", "ONST", "TWVL", "TWST", "THVL", "THST",
        "FRVL", "FRST", "FVVL", "FVST", "SXVL", "SXST", "SVVL", "SVST",
        "EIVL", "EIST", "NIVL", "NIST", "TEVL", "TEST", "ELVL", "ELST",
        "TVVL", "TVST", "TTVL", "TTST", "FRVL", "FRST", "FFVL", "FFST"
    };

    for (auto& field: fields) {
        if (entity.find(field) != entity.end())
            return "enum";
    }

    auto doubleValue = entity.getField<double>("VAL", std::numeric_limits<double>::min());
    if (doubleValue != std::numeric_limits<double>::min())
        return "analog";

    auto intValue    = entity.getField<int>("VAL", std::numeric_limits<int>::min());
    if (intValue != std::numeric_limits<int>::min())
        return "long";

    auto stringValue = entity.getField<std::string>("VAL", "<UDF string value>");
    if (stringValue != "<UDF string value>")
        return "string";

    return "";
}

static void printRecordAnalog(FILE* dbfile, const std::string& recordName, const Provider::Entity& entity)
{
    auto doubleValue = entity.getField<double>     ("VAL", std::numeric_limits<double>::min());
    auto intValue    = entity.getField<int>        ("VAL", std::numeric_limits<int>::min());

    auto inp = entity.getField<std::string>("INP", "");
    auto out = entity.getField<std::string>("OUT", "");

    auto desc = entity.getField<std::string>("DESC", "");
    auto egu  = entity.getField<std::string>("EGU", "");
    auto prec = entity.getField<int>("PREC", std::numeric_limits<int>::min());
    auto lopr = entity.getField<double>("LOPR", std::numeric_limits<double>::min());
    auto hopr = entity.getField<double>("HOPR", std::numeric_limits<double>::min());
    auto low  = entity.getField<double>("LOW",  std::numeric_limits<double>::min());
    auto lolo = entity.getField<double>("LOLO", std::numeric_limits<double>::min());
    auto high = entity.getField<double>("HIGH", std::numeric_limits<double>::min());
    auto hihi = entity.getField<double>("HIHI", std::numeric_limits<double>::min());
    auto hyst = entity.getField<double>("HYST", std::numeric_limits<double>::min());

    fprintf(dbfile,     "record(%s, \"%s\") {\n", (out != "" ? "ao" : "ai"), recordName.substr(0, 60).c_str());
    fprintf(dbfile,     "  field(DTYP, \"ipmi\")\n");
    fprintf(dbfile,     "  field(%s,  \"%s\")\n", (!out.empty() ? "OUT" : "INP"), (!out.empty() ? out.c_str() : inp.c_str()));
    if (desc != "")
        fprintf(dbfile, "  field(DESC, \"%s\")\n", desc.substr(0, 40).c_str());
    if (egu != "")
        fprintf(dbfile, "  field(EGU,  \"%s\")\n", egu.substr(0, 15).c_str());
    if (doubleValue != std::numeric_limits<double>::min())
        fprintf(dbfile, "  field(VAL,  \"%f\")\n", doubleValue);
    if (intValue != std::numeric_limits<int>::min())
        fprintf(dbfile, "  field(VAL,  \"%d\")\n", intValue);
    if (prec != std::numeric_limits<int>::min())
        fprintf(dbfile, "  field(PREC, \"%d\")\n", prec);
    if (lopr != std::numeric_limits<double>::min())
        fprintf(dbfile, "  field(LOPR, \"%f\")\n", lopr);
    if (hopr != std::numeric_limits<double>::min())
        fprintf(dbfile, "  field(HOPR, \"%f\")\n", hopr);
    if (low != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(LOW,  \"%f\")\n", low);
        fprintf(dbfile, "  field(LSV,  \"MINOR\")\n");
    }
    if (lolo != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(LOLO, \"%f\")\n", lolo);
        fprintf(dbfile, "  field(LLSV, \"MAJOR\")\n");
    }
    if (high != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(HIGH, \"%f\")\n", high);
        fprintf(dbfile, "  field(HSV,  \"MINOR\")\n");
    }
    if (hihi != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(HIHI, \"%f\")\n", hihi);
        fprintf(dbfile, "  field(HHSV, \"MAJOR\")\n");
    }
    if (hyst != std::numeric_limits<double>::min())
        fprintf(dbfile, "  field(HYST, \"%f\")\n", hyst);
    fprintf(dbfile,     "}\n");
}

static void printRecordLong(FILE* dbfile, const std::string& recordName, const Provider::Entity& entity)
{
    auto intValue    = entity.getField<int>        ("VAL", 0);

    auto inp = entity.getField<std::string>("INP", "");
    auto out = entity.getField<std::string>("OUT", "");

    auto desc = entity.getField<std::string>("DESC", "");
    auto egu  = entity.getField<std::string>("EGU", "");
    auto low  = entity.getField<double>("LOW",  std::numeric_limits<double>::min());
    auto lolo = entity.getField<double>("LOLO", std::numeric_limits<double>::min());
    auto high = entity.getField<double>("HIGH", std::numeric_limits<double>::min());
    auto hihi = entity.getField<double>("HIHI", std::numeric_limits<double>::min());

    fprintf(dbfile,     "record(%s, \"%s\") {\n", (out != "" ? "longout" : "longin"), recordName.substr(0, 60).c_str());
    fprintf(dbfile,     "  field(DTYP, \"ipmi\")\n");
    fprintf(dbfile,     "  field(%s,  \"%s\")\n", (!out.empty() ? "OUT" : "INP"), (!out.empty() ? out.c_str() : inp.c_str()));
    fprintf(dbfile,     "  field(VAL,  \"%d\")\n", intValue);
    if (desc != "")
        fprintf(dbfile, "  field(DESC, \"%s\")\n", desc.substr(0, 40).c_str());
    if (egu != "")
        fprintf(dbfile, "  field(EGU,  \"%s\")\n", egu.substr(0, 15).c_str());
    if (low != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(LOW,  \"%f\")\n", low);
        fprintf(dbfile, "  field(LSV,  \"MINOR\")\n");
    }
    if (lolo != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(LOLO, \"%f\")\n", lolo);
        fprintf(dbfile, "  field(LLSV, \"MAJOR\")\n");
    }
    if (high != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(HIGH, \"%f\")\n", high);
        fprintf(dbfile, "  field(HSV,  \"MINOR\")\n");
    }
    if (hihi != std::numeric_limits<double>::min()) {
        fprintf(dbfile, "  field(HIHI, \"%f\")\n", hihi);
        fprintf(dbfile, "  field(HHSV, \"MAJOR\")\n");
    }
    fprintf(dbfile,     "}\n");
}

static void printRecordEnum(FILE* dbfile, const std::string& recordName, const Provider::Entity& entity)
{
    auto intValue = entity.getField<int>        ("VAL", 0);

    auto inp = entity.getField<std::string>("INP", "");
    auto out = entity.getField<std::string>("OUT", "");

    auto desc = entity.getField<std::string>("DESC", "");

    fprintf(dbfile,     "record(%s, \"%s\") {\n", (out != "" ? "mbbo" : "mbbi"), recordName.substr(0, 60).c_str());
    fprintf(dbfile,     "  field(DTYP, \"ipmi\")\n");
    fprintf(dbfile,     "  field(%s,  \"%s\")\n", (!out.empty() ? "OUT" : "INP"), (!out.empty() ? out.c_str() : inp.c_str()));
    if (desc != "")
        fprintf(dbfile, "  field(DESC, \"%s\")\n", desc.substr(0, 40).c_str());
    fprintf(dbfile,     "  field(VAL,  \"%d\")\n", intValue);

    static const std::vector<std::string> fields = {
        "ZR", "ON", "TW", "TH", "FR", "FV", "SX", "SV",
        "EI", "NI", "TE", "EL", "TV", "TT", "FR", "FF",
    };
    for (auto& field: fields) {
        auto vl = entity.getField<int>(field + "VL", std::numeric_limits<int>::min());
        auto st = entity.getField<std::string>(field + "ST", "<UDF string value>");
        if (vl != std::numeric_limits<int>::min() && st != "<UDF string value>") {
            fprintf(dbfile,     "  field(%sVL, \"%d\")\n", field.c_str(), vl);
            fprintf(dbfile,     "  field(%sST, \"%s\")\n", field.c_str(), st.c_str());
        }
    }

    fprintf(dbfile,     "}\n");
}

static void printRecordString(FILE* dbfile, const std::string& recordName, const Provider::Entity& entity)
{
    auto stringValue = entity.getField<std::string>("VAL", "<UDF string value>");

    auto inp = entity.getField<std::string>("INP", "");
    auto out = entity.getField<std::string>("OUT", "");

    auto desc = entity.getField<std::string>("DESC", "");

    fprintf(dbfile,     "record(%s, \"%s\") {\n", (out != "" ? "stringout" : "stringin"), recordName.substr(0, 60).c_str());
    fprintf(dbfile,     "  field(DTYP, \"ipmi\")\n");
    fprintf(dbfile,     "  field(%s,  \"%s\")\n", (!out.empty() ? "OUT" : "INP"), (!out.empty() ? out.c_str() : inp.c_str()));
    if (desc != "")
        fprintf(dbfile, "  field(DESC, \"%s\")\n", desc.substr(0, 40).c_str());
    if (stringValue != "<UDF string value>")
        fprintf(dbfile, "  field(VAL,  \"%s\")\n", stringValue.substr(0, 39).c_str());
    fprintf(dbfile,     "}\n");
}

void printRecord(FILE* dbfile, const std::string& prefix, const Provider::Entity& entity)
{
    auto name = entity.getField<std::string>("NAME", "");
    if (name == "") {
        LOG_WARN("Record didn't specify name field, skipping");
        return;
    }
    auto recordName = prefix + _epicsEscape(name);

    auto type = getRecordType(entity);

    if (type == "enum")
        printRecordEnum(dbfile, recordName, entity);
    else if (type == "analog")
        printRecordAnalog(dbfile, recordName, entity);
    else if (type == "long")
        printRecordLong(dbfile, recordName, entity);
    else if (type == "string")
        printRecordString(dbfile, recordName, entity);
    else
        LOG_WARN("Record didn't specify input or output link field, skipping");
}

}; // namespace print
