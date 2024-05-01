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
#include <boRecord.h>
#include <dbBase.h>
#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <devSup.h>
#include <epicsExport.h>
#include <recGbl.h>

#include <limits>

#include "common.h"
#include "dispatcher.h"
#include "EntityAddrType.h"
#include <sstream>
#include <iostream>
#include <memory>

struct IpmiRecord {
    CALLBACK callback;
    Provider::Entity entity;
    std::shared_ptr<EntityAddrType> entAddrType{nullptr};
};

template<typename T>
long initInpRecord(T* rec)
{
    
    void *buffer = callocMustSucceed(1, sizeof(IpmiRecord), "ipmi::initGeneric");
    rec->dpvt = new (buffer) IpmiRecord;

    std::shared_ptr<EntityAddrType> eaddrt = nullptr;
    try {
        /** The connection has already been initialized and the SDR read.
         * So this call will not only verify the connection is correct but
         * it will also verify that the Sensor exist.
        */
        eaddrt = std::make_shared<EntityAddrType>(rec->inp.value.instio.string);
        
        dispatcher::checkLink(eaddrt);
        IpmiRecord *ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);
        ctx->entAddrType = eaddrt;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Record Init \'" + std::string(rec->name) + "\': " + e.what() + '\n');
        return -1;
    }
    return 0;
}

static long processAiRecord(aiRecord* rec)
{
    IpmiRecord *ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);
    
    if (ctx == nullptr) {
        /** Something did not go right in record-init*/
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    /** We never fully initialized. Maybe the device was offline during IOC boot,
     *  which kept the entityAddrType from getting created.
     *  Let's try to get the entityAddrType again and recover.
    */
    if(ctx->entAddrType == nullptr) {
        std::shared_ptr<EntityAddrType> eaddrt = nullptr;
        try
        {
            eaddrt = std::make_shared<EntityAddrType>(rec->inp.value.instio.string);
            dispatcher::checkLink(eaddrt);
            ctx->entAddrType = eaddrt;
        }
        catch(const std::exception &e)
        {
            LOG_ERROR("Record Process \'" + std::string(rec->name) +
            "\': failed to parse inout string - " + e.what() + '\n');
            return -1;
        }
    }
    
    if (rec->pact == 0) {
        rec->pact = 1;

        std::function<void()> cb = std::bind(callbackRequestProcessCallback, &ctx->callback, rec->prio, rec);

        try
        {
            dispatcher::scheduleGet(ctx->entAddrType, cb, ctx->entity);
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("Record Process \'" + std::string(rec->name) + "\': " + e.what() + '\n');
            recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);

            /** Try again, next time around*/
            rec->pact = 0;
            return -1;
        }

        return 0;
    }

    // This is the second pass, we got new value now update the record
    rec->pact = 0;

    rec->val = ctx->entity.getField<double>("VAL", rec->val);
    /**
     * Use the 'THRESHOLDS' to check the readable bits that are associated.
     * int thresholds = ctx->entity.getField<int>("THRESHOLDS", 0);
     * */
    /** Do we have thresholds? Check the readable status bits to find out.
     *  also, these are only available on threshold-type sensors.
    */

    if(ctx->entity.hasField("HIHI"))
    {
        rec->hihi = ctx->entity.getField<double>("HIHI", rec->hihi);
        rec->hhsv = MAJOR_ALARM;
    }
    if(ctx->entity.hasField("HIGH"))
    {
        rec->high = ctx->entity.getField<double>("HIGH", rec->high);
        rec->hsv = MINOR_ALARM;
    }
    if(ctx->entity.hasField("LOW"))
    {
        rec->low = ctx->entity.getField<double>("LOW", rec->low);
        rec->lsv = MINOR_ALARM;
    }
    if(ctx->entity.hasField("LOLO"))
    {
        rec->lolo = ctx->entity.getField<double>("LOLO", rec->lolo);
        rec->llsv = MAJOR_ALARM;
    }

    auto sevr = ctx->entity.getField<int>("SEVR", epicsSevNone);
    auto stat = ctx->entity.getField<int>("STAT", epicsAlarmNone);
    (void)recGblSetSevr(rec, stat, sevr);

    return 2;
}

static long initBoRecord(boRecord* rec)
{
    void *buffer = callocMustSucceed(1, sizeof(IpmiRecord), "ipmi::initGeneric");
    rec->dpvt = new (buffer) IpmiRecord;

    std::shared_ptr<EntityAddrType> eaddrt = nullptr;
    try {
        /** The connection has already been initialized and the SDR read.
         * So this call will not only verify the connection is correct but
         * it will also verify that the Sensor exist.
        */
        eaddrt = std::make_shared<EntityAddrType>(rec->out.value.instio.string);
        dispatcher::checkLink(eaddrt);
        
        IpmiRecord *ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);
        ctx->entAddrType = eaddrt;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Record Init \'" + std::string(rec->name) + "\': " + e.what() + '\n');
        return -1;
    }

    rec->rval = 0;
    return 0;
}

static long processBoRecord(boRecord* rec)
{
    
    IpmiRecord *ctx = reinterpret_cast<IpmiRecord*>(rec->dpvt);
    
    if (ctx == nullptr)
    {
        /** Something did not go right in record-init*/
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    /** We never fully initialized. Maybe the device was offline during IOC boot,
     *  which kept the entityAddrType from getting created.
     *  Let's try to get the entityAddrType again and recover.
    */
    if(ctx->entAddrType == nullptr)
    {
        std::shared_ptr<EntityAddrType> eaddrt = nullptr;
        try
        {
            eaddrt = std::make_shared<EntityAddrType>(rec->out.value.instio.string);
            dispatcher::checkLink(eaddrt);
            ctx->entAddrType = eaddrt;
        }
        catch(const std::exception &e)
        {
            LOG_ERROR("Record Process \'" + std::string(rec->name) +
            "\': failed to parse inout string - " + e.what() + '\n');
            return -1;
        }
    }
    
    if (rec->pact == 0)
    {
        rec->pact = 1;
        std::function<void()> cb = std::bind(callbackRequestProcessCallback, &ctx->callback, rec->prio, rec);
        try
        {
            ///TODO: I am not sure if we are going to need a callback or not. But for now we use it.
            /// Currently, the only ouput is a reboot command that doed not return anything.
            ctx->entity["VAL"] = rec->val;
            dispatcher::scheduleWrite(ctx->entAddrType, cb, ctx->entity);
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("Record Process \'" + std::string(rec->name) + "\': " + e.what() + '\n');
            recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);

            /** Try again, next time around*/
            rec->pact = 0;
            return -1;
        }
        return 0;
    }
        

    // This is the second pass, we got new value now update the record
    rec->pact = 0;

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
   DEVSUPFUN       write_bo;
} devEpicsIpmiBo = {
   5, // number
   NULL,                                // report
   NULL,                                // once-per-IOC initialization
   (DEVSUPFUN)initBoRecord,  // once-per-record initialization
   NULL,                                // get_ioint_info
   (DEVSUPFUN)processBoRecord
};
epicsExportAddress(dset, devEpicsIpmiBo);



}; // extern "C"
