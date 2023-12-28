#pragma once

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
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
        ~SmartConfig()
        {
            ESP_LOGE(TAG, "Deconstructing instance");

            taskhandle.reset();

            esp_smartconfig_stop();

            esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler);

            event_group.reset();
            wifiobj.reset();
        }

        state_t get_state() const { return state; }

    protected:
        static constexpr const char *const TAG{"SmartConfig"};
        static state_t state;
        static smartconfig_start_config_t _config;
        static std::unique_ptr<smartconfig_start_config_t> smartconfigcfg;
        static wifi::Wifi::Shared wifiobj;

        SmartConfig()
        {
            ESP_LOGE(TAG, "Constructing instance");

            if (not wifiobj)
            {
                ESP_LOGI(TAG, "Creating wifi instance");
                wifiobj = wifi::Wifi::get_shared();
            }

            event_group = eventgroup::make_eventgroup();

            ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler, nullptr));

            assert(not taskhandle);
            taskhandle = task::make_task(taskfn, TAG, taskstacksize, nullptr, 3);
            ESP_LOGI(TAG, "Started task at %p", taskhandle.get());

            ESP_ERROR_CHECK(esp_smartconfig_start(smartconfigcfg.get()));
            state = state_t::STARTED;
        }
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
                UNHANDLED
            };

            static handle_success_t handle_sc_event(args_t args);

            static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
            {
                args_t args{static_cast<smartconfig_event_t>(event_id), event_data};

                auto weakinstance = get_weak();
                if (weakinstance.expired())
                {
                    ESP_LOGW(TAG, "Event %u ignored; no instance of this class exists", args.event_id);
                    return;
                }

                if (SC_EVENT == event_base)
                    handle_sc_event(args);
                else [[unlikely]]
                    ESP_LOGW(TAG, "Unhandled event %s", event_base);
            }
        };

        static task::Task taskhandle;
        [[noreturn]] static void taskfn(void *param);
        static constexpr auto taskstacksize = 512 * sizeof(int);

        static eventgroup::Eventgroup event_group;
    };

} // namespace sc