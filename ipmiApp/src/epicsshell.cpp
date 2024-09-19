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

static void epicsipmiRegistrar ()
{
    static bool initialized  = false;
    if (!initialized) {
        initialized = false;
        iocshRegister(&ipmiConnectFuncDef, ipmiConnectCallFunc);
    }
}

extern "C" {
    epicsExportRegistrar(epicsipmiRegistrar);
}
