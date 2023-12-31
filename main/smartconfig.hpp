#pragma once

#include "esp_event.h"
#include "esp_smartconfig.h"
#include "freertos/FreeRTOS.h"

#include "singleton.hpp"
#include "wifi.hpp"
#include "wrappers/eventgroup.hpp"
#include "wrappers/task.hpp"

namespace sc
{

    enum class state_t
    {
        IDLE,
        STARTED,
        CONNECTED,
        DONE,
        ERROR
    };

    static constexpr auto ESPTOUCH_DONE_BIT{BIT1};

    class SmartConfig : public Singleton<SmartConfig> // NOTE: CRTP
    {
        friend Singleton<SmartConfig>; // NOTE: So Singleton can use our private/protected constructor

    public:
        ~SmartConfig();

        state_t get_state() const { return state; }
        wifi::Wifi::Shared get_wifi() const { return wifiobj; }

    protected:
        static constexpr const char *const TAG{"SmartConfig"};
        static state_t state;
        static smartconfig_start_config_t _config;
        static std::unique_ptr<smartconfig_start_config_t> smartconfigcfg;
        static wifi::Wifi::Shared wifiobj;

        SmartConfig();

        SmartConfig(const SmartConfig &) = delete;
        SmartConfig &operator=(const SmartConfig &) = delete;

        struct EventHandlers
        {
            struct args_t
            {
                smartconfig_event_t event_id;
                const void *event_data;
            };

            enum class handle_success_t
            {
                OK,
                FAIL,
                UNHANDLED
            };

            static handle_success_t handle_sc_event(args_t args);

            static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
        };

        static task::Task taskhandle;
        [[noreturn]] static void taskfn(void *param);
        static constexpr auto taskstacksize = 640 * sizeof(int);

        static eventgroup::Eventgroup event_group;
    };

} // namespace sc