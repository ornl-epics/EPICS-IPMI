/* provider.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#include "common.h"
#include "provider.h"

#include <alarm.h>
#include <epicsThread.h>

#include <limits>
#include <iostream>


extern "C" {
    static void providerThread(void* ctx)
    {
        reinterpret_cast<Provider*>(ctx)->tasksThread();
    }
};

Provider::Provider(const std::string& conn_id)
: mConnId(conn_id)
{
    
}

Provider::~Provider()
{
    stopThread();
}

bool Provider::stopThread(double timeout)
{
    if (m_tasks.processing) {
        m_tasks.processing = false;
        m_tasks.event.signal();

        if (timeout > 0)
            return m_tasks.stopped.wait(timeout);

        m_tasks.stopped.wait();
    }
    return true;
}

void Provider::start() {

    epicsThreadCreate(mConnId.c_str(), epicsThreadPriorityLow,
        epicsThreadStackMedium, (EPICSTHREADFUNC)&providerThread, this);
}

bool Provider::schedule(const Task&& task)
{
    m_tasks.mutex.lock();
    m_tasks.queue.emplace_back(task);
    m_tasks.event.signal();
    m_tasks.mutex.unlock();
    return true;
}

void Provider::tasksThread()
{
    while (m_tasks.processing) {
        
        process();
        m_tasks.mutex.lock();

        if (m_tasks.queue.empty()) {
            m_tasks.mutex.unlock();
            ///m_tasks.event.wait();
            epicsThreadSleep(0.1);
            continue;
        }

        Task task = std::move(m_tasks.queue.front());
        m_tasks.queue.pop_front();
        m_tasks.mutex.unlock();

        const EntityAddrType::Type ADDRESS_TYPE = task.entAddrTyp->getEntityAddressType();

        try {
            
            switch (ADDRESS_TYPE)
            {
                case EntityAddrType::Type::SENSOR:
                {
                    Entity ent = getEntityValue(task.entAddrTyp);

                    for (auto& kv: ent)
                    {
                        task.entity[kv.first] = std::move(kv.second);
                    }
                    /** We have to set these back to normal if we had
                     * set them below in the catch... otherwise they
                     * stay in alarm.
                    */
                    task.entity["SEVR"] = (int)epicsSevNone;
                    task.entity["STAT"] = (int)epicsAlarmNone;
                }
                case EntityAddrType::Type::OEM_CMD:
                {
                    write_oem_command(task.entAddrTyp, task.entity);
                }
            }
            
            

        } catch (std::runtime_error &e) {
            task.entity["SEVR"] = (int)epicsSevInvalid;
            task.entity["STAT"] = (int)epicsAlarmComm;
            LOG_ERROR(e.what());
        } catch (...) {
            task.entity["SEVR"] = (int)epicsSevInvalid;
            task.entity["STAT"] = (int)epicsAlarmComm;
            LOG_ERROR("Unhandled exception getting IPMI entity");
        }
        task.callback();
    }

    m_tasks.stopped.signal();
}
