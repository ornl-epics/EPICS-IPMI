/* epicsshell.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#include "common.h"
#include "dispatcher.h"

#include <map>

#include <epicsExport.h>
#include <iocsh.h>

// ipmiConnect(conn_id, host_name, [username], [password], [protocol], [privlevel])
static const iocshArg ipmiConnectArg0 = { "connection id",  iocshArgString };
static const iocshArg ipmiConnectArg1 = { "host name",      iocshArgString };
static const iocshArg ipmiConnectArg2 = { "username",       iocshArgString };
static const iocshArg ipmiConnectArg3 = { "password",       iocshArgString };
static const iocshArg ipmiConnectArg4 = { "authtype",       iocshArgString };
static const iocshArg ipmiConnectArg5 = { "protocol",       iocshArgString };
static const iocshArg ipmiConnectArg6 = { "privlevel",      iocshArgString };
static const iocshArg* ipmiConnectArgs[] = {
    &ipmiConnectArg0,
    &ipmiConnectArg1,
    &ipmiConnectArg2,
    &ipmiConnectArg3,
    &ipmiConnectArg4,
    &ipmiConnectArg5,
    &ipmiConnectArg6
};
static const iocshFuncDef ipmiConnectFuncDef = { "ipmiConnect", 7, ipmiConnectArgs };

extern "C" void ipmiConnectCallFunc(const iocshArgBuf* args) {
    if (!args[0].sval || !args[1].sval) {
        printf("Usage: ipmiConnect <conn id> <hostname> [username] [password] [authtype] [protocol] [privlevel]\n");
        return;
    }

    std::string conn_id  =  args[0].sval;
    std::string hostname =  args[1].sval;
    std::string username = (args[2].sval ? args[2].sval : "");
    std::string password = (args[3].sval ? args[3].sval : "");

    static std::vector<std::string> authTypes = { "none", "plain", "md2", "md5" };
    std::string authType = "none";
    if (args[4].sval && common::contains(authTypes, args[4].sval)) {
        authType = args[4].sval;
    } else if (args[4].sval) {
        printf("ERROR: Invalid auth type '%s', choose from '%s'\n", args[4].sval, common::merge(authTypes, "',").c_str());
        return;
    }

    static std::vector<std::string> protocols = { "lan_2.0", "lan" };
    std::string protocol = "lan";
    if (args[5].sval && common::contains(protocols, args[5].sval)) {
        protocol = args[5].sval;
    } else if (args[5].sval) {
        printf("ERROR: Invalid protocol '%s', choose from '%s'\n", args[5].sval, common::merge(protocols, "','").c_str());
        return;
    }

    static std::vector<std::string> privLevels = { "user", "operator", "admin" };
    std::string privLevel = "operator";
    if (args[6].sval && common::contains(privLevels, args[6].sval)) {
        privLevel = args[6].sval;
    } else if (args[6].sval) {
        printf("ERROR: Invalid privilege level '%s', choose from '%s\n", args[6].sval, common::merge(privLevels, "','").c_str());
        return;
    }

    dispatcher::connect(conn_id, hostname, username, password, authType, protocol, privLevel);
}

// ipmiScan(conn_id, [types])
static const iocshArg ipmiScanArg0 = { "connection id",     iocshArgString };
static const iocshArg ipmiScanArg1 = { "type",              iocshArgString };
static const iocshArg ipmiScanArg2 = { "type",              iocshArgString };
static const iocshArg ipmiScanArg3 = { "type",              iocshArgString };
static const iocshArg ipmiScanArg4 = { "type",              iocshArgString };
static const iocshArg ipmiScanArg5 = { "type",              iocshArgString };
static const iocshArg* ipmiScanArgs[] = {
    &ipmiScanArg0,
    &ipmiScanArg1,
    &ipmiScanArg2,
    &ipmiScanArg3,
    &ipmiScanArg4,
    &ipmiScanArg5,
};
static const iocshFuncDef ipmiScanFuncDef = { "ipmiScan", 6, ipmiScanArgs };

extern "C" void ipmiScanCallFunc(const iocshArgBuf* args) {
    if (!args[0].sval) {
        printf("Usage: ipmiScan <conn id> [types]\n");
        return;
    }

    std::map<std::string, dispatcher::EntityType> validTypes = {
        { "sensor",         dispatcher::EntityType::SENSOR },
        { "fru",            dispatcher::EntityType::FRU },
        { "picmg_led",      dispatcher::EntityType::PICMG_LED },
    };

    std::vector<dispatcher::EntityType> types;
    if (!args[1].sval) {
        for (auto& it: validTypes) {
            types.push_back(it.second);
        }
    } else {
        // Check user selection
        for (int i = 1; i <= 5; i++) {
            if (args[i].sval) {
                bool found = false;
                for (auto& it: validTypes) {
                    if (it.first == args[i].sval) {
                        types.push_back(it.second);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    printf("ERROR: Unknown entity type '%s'", args[i].sval);
            }
        }
        if (types.empty())
            return;
    }

    dispatcher::scan(args[0].sval, types);
}

// ipmiDumpDb(conn_id, db_file)
static const iocshArg ipmiDumpDbArg0 = { "connection id",     iocshArgString };
static const iocshArg ipmiDumpDbArg1 = { "output file",       iocshArgString };
static const iocshArg ipmiDumpDbArg2 = { "PV prefix",         iocshArgString };
static const iocshArg* ipmiDumpDbArgs[] = {
    &ipmiDumpDbArg0,
    &ipmiDumpDbArg1,
    &ipmiDumpDbArg2,
};
static const iocshFuncDef ipmiDumpDbFuncDef = { "ipmiDumpDb", 3, ipmiDumpDbArgs };

extern "C" void ipmiDumpDbCallFunc(const iocshArgBuf* args) {
    if (!args[0].sval || !args[1].sval) {
        printf("Usage: ipmiDumpDb <conn id> <output file> [PV prefix]\n");
        return;
    }

    dispatcher::printDb(args[0].sval, args[1].sval, args[2].sval ? args[2].sval : "");
}

static void epicsipmiRegistrar ()
{
    static bool initialized  = false;
    if (!initialized) {
        initialized = false;
        iocshRegister(&ipmiConnectFuncDef, ipmiConnectCallFunc);
        iocshRegister(&ipmiScanFuncDef,    ipmiScanCallFunc);
        iocshRegister(&ipmiDumpDbFuncDef,  ipmiDumpDbCallFunc);
    }
}

extern "C" {
    epicsExportRegistrar(epicsipmiRegistrar);
}
