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
#include <vector>

#if __cplusplus > 201402L
#include <variant>
#else
#include "variant.hpp"
namespace std
{
  using namespace ::mpark;

  // mpart::get<>() doesn't get merged into std namespace automatically!!!
  template <std::size_t I, typename... Ts>
  inline constexpr ::mpark::variant_alternative_t<I, ::mpark::variant<Ts...>> &get(
      ::mpark::variant<Ts...> &v) {
    return ::mpark::detail::generic_get<I>(v);
  }

  template <std::size_t I, typename... Ts>
  inline constexpr ::mpark::variant_alternative_t<I, ::mpark::variant<Ts...>> &&get(
      ::mpark::variant<Ts...> &&v) {
    return ::mpark::detail::generic_get<I>(::mpark::lib::move(v));
  }

  template <std::size_t I, typename... Ts>
  inline constexpr const ::mpark::variant_alternative_t<I, ::mpark::variant<Ts...>> &get(
      const ::mpark::variant<Ts...> &v) {
    return ::mpark::detail::generic_get<I>(v);
  }

  template <std::size_t I, typename... Ts>
  inline constexpr const ::mpark::variant_alternative_t<I, ::mpark::variant<Ts...>> &&get(
      const ::mpark::variant<Ts...> &&v) {
    return ::mpark::detail::generic_get<I>(::mpark::lib::move(v));
  }

  template <typename T, typename... Ts>
  inline constexpr T &get(::mpark::variant<Ts...> &v) {
    return ::mpark::get<::mpark::detail::find_index_checked<T, Ts...>::value>(v);
  }

  template <typename T, typename... Ts>
  inline constexpr T &&get(::mpark::variant<Ts...> &&v) {
    return ::mpark::get<::mpark::detail::find_index_checked<T, Ts...>::value>(::mpark::lib::move(v));
  }

  template <typename T, typename... Ts>
  inline constexpr const T &get(const ::mpark::variant<Ts...> &v) {
    return ::mpark::get<::mpark::detail::find_index_checked<T, Ts...>::value>(v);
  }

  template <typename T, typename... Ts>
  inline constexpr const T &&get(const ::mpark::variant<Ts...> &&v) {
    return ::mpark::get<::mpark::detail::find_index_checked<T, Ts...>::value>(::mpark::lib::move(v));
  }
}
#endif

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
        };
        struct Task {
            std::string address;
            std::function<void()> callback;
            Entity& entity;
            Task(const std::string& address_, const std::function<void()>& cb, Entity& entity_)
                : address(address_)
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
         * @brief Scan for all sensors in the connected IPMI device.
         * @return A list of sensors
         */
        virtual std::vector<Entity> getSensors() = 0;

        /**
         * @brief Scans for all FRUs in the connected IPMI device.
         * @return A list of FRUs
         */
        virtual std::vector<Entity> getFrus() = 0;

        /**
         * @brief Scans for all PICMG LEDs in the connected IPMI device.
         * @return A list of LEDs
         */
        virtual std::vector<Entity> getPicmgLeds() = 0;

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

    private:
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
        virtual Entity getEntity(const std::string& address) = 0;
};
