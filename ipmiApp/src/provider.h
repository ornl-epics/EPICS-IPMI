/* provider.h
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#pragma once

#include <epicsEvent.h>
#include <epicsMutex.h>

#include <string>
#include <list>
#include <map>
#include <memory>
#include <functional>
#include <vector>
#include "IpmiSensorRecComp.h"
#include "PicmgLed.h"
#include "EntityAddrType.h"
#include <variant>

/**
 * @class Provider
 * @file provider.h
 * @brief Base Provider class with public interfaces that derived classes must implement.
 */
class Provider {
    public:
        typedef std::variant<int,double,std::string> Variant;           //!< Generic container for entity fields
        class Entity : public std::map<std::string, Variant> {
            public:
                template <typename T>
                T getField(const std::string& field, const T& default_) const
                {
                    auto it = find(field);
                    if (it != end()) {
                        auto ptr = std::get_if<T>(&it->second);
                        if (ptr)
                            return *ptr;
                    }
                    return default_;
                }
                
                bool hasField(const std::string& field)
                {
                    auto it = find(field);
                    return (it != end());
                }
        };
        struct Task {
            std::shared_ptr<EntityAddrType> entAddrTyp;
            std::function<void()> callback;
            Entity& entity;
            
            Task(std::shared_ptr<EntityAddrType> entAddrTyp_, const std::function<void()>& cb, Entity& entity_)
                : entAddrTyp(entAddrTyp_)
                , callback(cb)
                , entity(entity_)
            {};
        };

        struct comm_error : public std::runtime_error {
            using std::runtime_error::runtime_error;
        };
        struct syntax_error : public std::runtime_error {
            using std::runtime_error::runtime_error;
        };
        struct process_error : public std::runtime_error {
            using std::runtime_error::runtime_error;
        };

        Provider(const std::string& conn_id);

        ~Provider();

        /**
         * @brief Schedules retrieving IPMI value and calling cb function when done.
         * @param address IPMI entity address
         * @param cb function to be called upon (un)succesfull completion
         * @return true if succesfully scheduled and will invoke record post-processing
         */
        bool schedule(const Task&& task);

        /**
         * @brief Thread processing enqueued tasks
         */
        void tasksThread();

        /**
         * @brief Stop the processing thread, to be run from destructor.
         * @param timeout in seconds to wait for thread, 0 means no timeout
         * @return true if thread successfully stopped in given time
         */
        bool stopThread(double timeout=0.0);

        void start();

    private:

        const std::string mConnId;
        struct {
            bool processing{true};
            std::list<Task> queue;
            epicsMutex mutex;
            epicsEvent event;
            epicsEvent stopped;
        } m_tasks;

        /**
         * @brief Based on the address, determine IPMI entity type and retrieve its current value.
         * @param address FreeIPMI implementation specific address
         * @return current value
         */
        virtual Entity getEntityValue(const std::shared_ptr<EntityAddrType> entAddrType) = 0;
        virtual void write_oem_command(const std::shared_ptr<EntityAddrType> entAddrType, Entity &entity) = 0;
        
        virtual void process() = 0;

};
