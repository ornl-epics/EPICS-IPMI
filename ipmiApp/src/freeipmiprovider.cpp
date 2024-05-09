/* freeipmiprovider.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

#include "freeipmiprovider.h"
#include <iostream>
#include <sstream>
#include "IpmiException.h"


FreeIpmiProvider::FreeIpmiProvider(const std::string& conn_id, const std::string& hostname,
                                   const std::string& username, const std::string& password,
                                   const std::string& authtype, const std::string& protocol,
                                   const std::string& privlevel)
: Provider(conn_id)
{

    mConnManager = new IpmiConnectionManager(conn_id, hostname, username, password,
    authtype, protocol, privlevel);

    mSdrManager = new IpmiSdrManager(*mConnManager);

    ///TODO: Check that the info is there before we print it.
    std::cout << mSdrManager->getHeaderAsString() << std::endl;
    start();
}

FreeIpmiProvider::~FreeIpmiProvider()
{
    if (stopThread() == false)
        LOG_WARN("Processing thread did not stop");
}

bool FreeIpmiProvider::is_valid_oem_cmd(const std::string &vendor_id, const std::string &command)
{
    return mConnManager->is_valid_oem_command(vendor_id, command);
}

FreeIpmiProvider::Entity FreeIpmiProvider::getEntityValue(const std::shared_ptr<EntityAddrType> entAddrType) {

    if(!entAddrType) {
        throw std::runtime_error("In method FreeIpmiProvider::getEntityValue(...) EntityAddrType parameter is null.");
    }

    const EntityAddrType::Type addressType = entAddrType->getEntityAddressType();

    Entity entity;

    switch (addressType) {

    case EntityAddrType::Type::SENSOR:
        entity = getSensorReading(entAddrType);
        break;

    case EntityAddrType::Type::PICMG_LED:
        ///TODO: Finish getPicmgLedReading(entAddrType);
        ///entity = getPicmgLedReading(entAddrType);
        break;
    
    default:
        throw std::runtime_error("Invalid Entity address type \'" + entAddrType->getEntityAddressTypeAsString() + "\'");
        break;
    }

    return entity;
}

void FreeIpmiProvider::write_oem_command(const std::shared_ptr<EntityAddrType> entAddrType, Entity &entity)
{
    mConnManager->write_oem_command(entAddrType, entity);
}

FreeIpmiProvider::Entity FreeIpmiProvider::getPicmgLedReading(const std::shared_ptr<EntityAddrType> entAddrType) {
    
    throw std::runtime_error("getPicmgLedReading() is unsupported currently...");
    if(!entAddrType) {
        throw std::runtime_error("In method FreeIpmiProvider::getPicmgLedReading(...) EntityAddrType parameter is null.");
    }
    /** Find the FRU and Then find the PICMGLED*/
    std::shared_ptr<IpmiFruDevLocRec> fru = nullptr;
    std::shared_ptr<PicmgLed> led = nullptr;
    
    fru = mSdrManager->getFruByDeviceSlaveAddress(entAddrType->getPicmgLedFruDeviceSlaveSddress().first);
    if(!fru) {
        throw std::runtime_error("Fru object is null in getPicmgLedReading().");
    }
    led = fru->getStatusLedById(entAddrType->getPicmgLedId().first);
    if(!led) {
        throw std::runtime_error("PICMG_LED object is null in getPicmgLedReading().");
    }
    ///return readPicmgLed(this->m_ctx.ipmi,led);
}

FreeIpmiProvider::Entity FreeIpmiProvider::getSensorReading(const std::shared_ptr<EntityAddrType> entAddrType) {

    
    if(!entAddrType) {
        throw std::runtime_error("In method FreeIpmiProvider::getSensorReading(...) EntityAddrType parameter is null.");
    }
    std::shared_ptr<IpmiSensorRecComp> sp = nullptr;
    const std::string key = entAddrType->getSensorIdAsKey();
    sp = mSdrManager->findSensorByMapKey(key);

    if(!sp) {
        throw std::runtime_error("Could not find sensor in map by key \'" + key + "\'");
    }
    
    return mConnManager->getSensorReading(sp);
    
}

std::shared_ptr<IpmiSensorRecComp> FreeIpmiProvider::findSensorByMapKey(std::string key) {

    return mSdrManager->findSensorByMapKey(key);
}

std::shared_ptr<PicmgLed> FreeIpmiProvider::getPicmgLedByAddress(uint8_t fru_id, uint8_t led_id) {
    
    std::shared_ptr<IpmiFruDevLocRec> pfru = nullptr;
    std::shared_ptr<PicmgLed> ppicmgled = nullptr;
    pfru = mSdrManager->getFruByDeviceSlaveAddress(fru_id);
    if(pfru) {
        ppicmgled = pfru.get()->getStatusLedById(led_id);
    }
    
    return ppicmgled;
}

void FreeIpmiProvider::process() {

    try
    {
        if(mConnManager)
        {
            mConnManager->process();
            try
            {
                if(mSdrManager && mConnManager->isConnected())
                {
                    mSdrManager->process();
                }
            }
            catch(const std::exception& e)
            {
                std::cout << "*** mSdrManager Exception Caught" << std::endl;
                std::cerr << e.what() << '\n';
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cout << "*** mConnManager Exception Caught" << std::endl;
        std::cerr << e.what() << '\n';
    }
    
    
    
}

