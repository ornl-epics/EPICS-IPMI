/* epicsdevice.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#include <aiRecord.h>
#include <biRecord.h>
#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <devSup.h>
#include <epicsExport.h>
#include <mbbiRecord.h>
#include <recGbl.h>
#include <stringinRecord.h>

#include <limits>

#include "common.h"
#include "dispatcher.h"
#include <sstream>
#include <iostream>

struct IpmiRecord {
    CALLBACK callback;
    Provider::Entity entity;
};

template<typename T>
long initInpRecord(T* rec)
{
    try {
        /** The connection has already been initialized and the SDR read.
         * So this call will not only verify the connection is correct but
         * it will also verify that the Sensor exist.
        */
        dispatcher::checkLink(rec->inp.value.instio.string);
    }
    catch(const std::exception &e) {
        std::cerr << "ERROR: Record \'" << rec->name << "\' " << e.what() << '\n';
        if (rec->tpro == 1) {
            LOG_ERROR("invalid record link or no connection");
        }
        rec->dpvt = nullptr;
        return -1;
    }
    
    void* buffer = callocMustSucceed(1, sizeof(IpmiRecord), "ipmi::initGeneric");
    rec->dpvt = new (buffer) IpmiRecord;
    return 0;
}

static long processAiRecord(aiRecord* rec)
{
    IpmiRecord* ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);
    if (ctx == nullptr) {
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    if (rec->pact == 0) {
        rec->pact = 1;

        std::function<void()> cb = std::bind(callbackRequestProcessCallback, &ctx->callback, rec->prio, rec);
        if (dispatcher::scheduleGet(rec->inp.value.instio.string, cb, ctx->entity) == false) {
            // Keep PACT=1 to prevent further processing
            recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
            return -1;
        }

        return 0;
    }

    // This is the second pass, we got new value now update the record
    rec->pact = 0;

    rec->val = ctx->entity.getField<double>("VAL", rec->val);
    /** TODO: Why rval?*/
    /** rec->rval = rec->val;*/

    auto sevr = ctx->entity.getField<int>("SEVR", epicsSevNone);
    auto stat = ctx->entity.getField<int>("STAT", epicsAlarmNone);
    (void)recGblSetSevr(rec, stat, sevr);

    if (rec->egu[0] == 0)
        common::copy(ctx->entity.getField<std::string>("EGU", ""), rec->egu, sizeof(rec->egu));
    if (rec->desc[0] == 0)
        common::copy(ctx->entity.getField<std::string>("DESC", ""), rec->desc, sizeof(rec->desc));

    return 2;
}

static long processBiRecord(biRecord* rec) {

    return 0;
}

static long processStringinRecord(stringinRecord* rec)
{
    IpmiRecord* ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);

    if (rec->pact == 0) {
        rec->pact = 1;

        std::function<void()> cb = std::bind(callbackRequestProcessCallback, &ctx->callback, rec->prio, rec);
        if (dispatcher::scheduleGet(rec->inp.value.instio.string, cb, ctx->entity) == false) {
            // Keep PACT=1 to prevent further processing
            recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
            return -1;
        }

        return 0;
    }

    // This is the second pass, we got new value now update the record
    rec->pact = 0;

    common::copy(ctx->entity.getField<std::string>("VAL", rec->val), rec->val, sizeof(rec->val));

    auto sevr = ctx->entity.getField<int>("SEVR", epicsSevNone);
    auto stat = ctx->entity.getField<int>("STAT", epicsAlarmNone);
    (void)recGblSetSevr(rec, stat, sevr);

    if (rec->desc[0] == 0)
        common::copy(ctx->entity.getField<std::string>("DESC", ""), rec->desc, sizeof(rec->desc));

    return 0;
}

static long processMbbiRecord(mbbiRecord* rec)
{
    IpmiRecord* ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);

    if (rec->pact == 0) {
        rec->pact = 1;

        std::function<void()> cb = std::bind(callbackRequestProcessCallback, &ctx->callback, rec->prio, rec);
        if (dispatcher::scheduleGet(rec->inp.value.instio.string, cb, ctx->entity) == false) {
            // Keep PACT=1 to prevent further processing
            recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
            return -1;
        }

        return 0;
    }

    // This is the second pass, we got new value now update the record
    rec->pact = 0;

    rec->rval  = ctx->entity.getField<int>("VAL", 0);

    auto sevr = ctx->entity.getField<int>("SEVR", epicsSevNone);
    auto stat = ctx->entity.getField<int>("STAT", epicsAlarmNone);
    (void)recGblSetSevr(rec, stat, sevr);

    if (rec->desc[0] == 0)
        common::copy(ctx->entity.getField<std::string>("DESC", ""), rec->desc, sizeof(rec->desc));

    return 0;
}

extern "C" {

struct {
   long            number;
   DEVSUPFUN       report;
   DEVSUPFUN       init;
   DEVSUPFUN       init_record;
   DEVSUPFUN       get_ioint_info;
   DEVSUPFUN       read_ai;
   DEVSUPFUN       special_linconv;
} devEpicsIpmiAi = {
   6, // number
   NULL,                                // report
   NULL,                                // once-per-IOC initialization
   (DEVSUPFUN)initInpRecord<aiRecord>,  // once-per-record initialization
   NULL,                                // get_ioint_info
   (DEVSUPFUN)processAiRecord,
   NULL                                 // special_linconv
};
epicsExportAddress(dset, devEpicsIpmiAi);

struct {
   long            number;
   DEVSUPFUN       report;
   DEVSUPFUN       init;
   DEVSUPFUN       init_record;
   DEVSUPFUN       get_ioint_info;
   DEVSUPFUN       read_bi;
   DEVSUPFUN       special_linconv;
} devEpicsIpmiBi = {
   6, // number
   NULL,                                // report
   NULL,                                // once-per-IOC initialization
   (DEVSUPFUN)initInpRecord<biRecord>,  // once-per-record initialization
   NULL,                                // get_ioint_info
   (DEVSUPFUN)processBiRecord,
   NULL                                 // special_linconv
};
epicsExportAddress(dset, devEpicsIpmiBi);

struct {
   long            number;
   DEVSUPFUN       report;
   DEVSUPFUN       init;
   DEVSUPFUN       init_record;
   DEVSUPFUN       get_ioint_info;
   DEVSUPFUN       read_ai;
   DEVSUPFUN       special_linconv;
} devEpicsIpmiStringin = {
   6, // number
   NULL, // report
   NULL, // init
   (DEVSUPFUN)initInpRecord<stringinRecord>,
   NULL, // get_ioint_info
   (DEVSUPFUN)processStringinRecord,
   NULL  // special_linconv
};
epicsExportAddress(dset, devEpicsIpmiStringin);

struct {
   long            number;
   DEVSUPFUN       report;
   DEVSUPFUN       init;
   DEVSUPFUN       init_record;
   DEVSUPFUN       get_ioint_info;
   DEVSUPFUN       read_ai;
   DEVSUPFUN       special_linconv;
} devEpicsIpmiMbbi = {
   6, // number
   NULL,                                  // report
   NULL,                                  // once-per-IOC initialization
   (DEVSUPFUN)initInpRecord<mbbiRecord>,  // once-per-record initialization
   NULL,                                  // get_ioint_info
   (DEVSUPFUN)processMbbiRecord,
   NULL                                   // special_linconv
};
epicsExportAddress(dset, devEpicsIpmiMbbi);

}; // extern "C"
