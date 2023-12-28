#pragma once

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "singleton.hpp"
#include "wrappers/eventgroup.hpp"
#include "wrappers/netif.hpp"
#include "wrappers/task.hpp"

namespace wifi
{

    enum class state_t
    {
        IDLE,
        NETIF_INITIALISED,
        STARTED,
        CONNECTED,
        GOT_IP,
        DONE,
        ERROR
    };

    static constexpr auto CONNECTED_BIT{BIT0};

    class Wifi : public Singleton<Wifi> // NOTE: CRTP
    {
        friend Singleton<Wifi>; // NOTE: So Singleton can use our private/protected constructor

    public:
        ~Wifi()
        {
            ESP_LOGE(TAG, "Deconstructing instance");

            taskhandle.reset();

            ESP_LOGI(TAG, "esp_wifi_disconnect");
            esp_wifi_disconnect();

            ESP_LOGI(TAG, "esp_wifi_stop");
            esp_wifi_stop();

            ESP_LOGI(TAG, "esp_wifi_deinit");
            esp_wifi_deinit();

            ESP_LOGI(TAG, "esp_wifi_clear_default_wifi_driver_and_handlers");
            esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif.get());

            esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler);
            esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler);

            esp_netif_deinit(); // FIXME: Apparently not yet implemented by Espressif

            event_group.reset();
            sta_netif.reset();
        }

        static state_t get_state() { return state; }

    protected:
        static constexpr const char *const TAG{"Wifi"};
        static state_t state;
        static netif::Netif sta_netif;
        static std::unique_ptr<wifi_init_config_t> wifiinitcfg;

        Wifi()
        {
            ESP_LOGE(TAG, "Constructing instance");

            ESP_ERROR_CHECK(esp_netif_init());

            const auto eventloopsuccess = esp_event_loop_create_default();                   // NOTE: we guarantee not to destroy this if we get destructed
            assert(ESP_OK == eventloopsuccess || ESP_ERR_INVALID_STATE == eventloopsuccess); // NOTE: ESP_ERR_INVALID_STATE is returned if the default event loop has already been created

            sta_netif = netif::make_netif(esp_netif_create_default_wifi_sta());
            assert(sta_netif.get());

            state = state_t::NETIF_INITIALISED;

            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler, nullptr));
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &EventHandlers::event_handler, nullptr));

            // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(wifiinitcfg.get()));

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_start());

            event_group = eventgroup::make_eventgroup();

            state = state_t::STARTED;
        }

        struct EventHandlers
        {
            struct args_t
            {
                wifi_event_t event_id;
                const void *event_data = nullptr;
            };

            enum class handle_success_t
            {
                OK,
                UNHANDLED
            };

            static handle_success_t handle_wifi_event(args_t args);
            static handle_success_t handle_ip_event(args_t args);

            static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
            {
                args_t args{static_cast<wifi_event_t>(event_id), event_data};

                auto weakinstance = get_weak();
                if (weakinstance.expired()) [[unlikely]]
                {
                    ESP_LOGW(TAG, "Event %u ignored; no instance of this class exists", args.event_id);
                    return;
                }

                if (WIFI_EVENT == event_base)
                    handle_wifi_event(args);
                else if (IP_EVENT == event_base)
                    handle_ip_event(args);
                else [[unlikely]]
                    ESP_LOGW(TAG, "Unhandled event %s", event_base);
            }
        };

        Wifi(const Wifi &) = delete;
        Wifi &operator=(const Wifi &) = delete;

        static task::Task taskhandle;
        [[noreturn]] static void taskfn(void *param);
        static constexpr auto taskstacksize = 896 * sizeof(int);

        static eventgroup::Eventgroup event_group;
    };

} // namespace wifi