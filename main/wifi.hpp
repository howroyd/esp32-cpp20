#pragma once

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "singleton.hpp"
#include "wrappers/eventgroup.hpp"
#include "wrappers/netif.hpp"
#include "wrappers/nvs.hpp"
#include "wrappers/task.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

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

    using SsidPasswordView = std::pair<std::string_view, std::string_view>;
    SsidPasswordView config_to_ssidpasswordview(const wifi_config_t &config);

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

        std::string nvs_ssid() const
        {
            return nvs::get_string(storage, "ssid");
        }
        bool nvs_ssid(std::string_view ssid)
        {
            return nvs::set_string(storage, "ssid", ssid);
        }
        static bool nvs_ssid_erase()
        {
            return nvs::erase_key(storage, "ssid");
        }
        std::string nvs_password() const
        {
            return nvs::get_string(storage, "password");
        }
        bool nvs_password(std::string_view password)
        {
            return nvs::set_string(storage, "password", password);
        }
        static bool nvs_password_erase()
        {
            return nvs::erase_key(storage, "password");
        }

        bool reconnect_to(wifi_config_t &wifi_config)
        {
            auto status = esp_wifi_disconnect();
            if (ESP_OK != status)
            {
                ESP_LOGW(TAG, "Failed to disconnect: %s", esp_err_to_name(status));
                return false;
            }

            status = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            if (ESP_OK != status)
            {
                ESP_LOGW(TAG, "Failed to set config: %s", esp_err_to_name(status));
                return false;
            }

            status = esp_wifi_connect();
            if (ESP_OK != status)
            {
                ESP_LOGW(TAG, "Failed to connect: %s", esp_err_to_name(status));
                return false;
            }

            return true;
        }

        auto config() const
        {
            wifi_config_t wifi_config{};
            esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
            return wifi_config;
        }

    protected:
        static constexpr const char *const TAG{"Wifi"};
        static state_t state;
        static netif::Netif sta_netif;
        static std::unique_ptr<wifi_init_config_t> wifiinitcfg;
        static nvs::Nvs storage;

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

            ESP_ERROR_CHECK(esp_wifi_init(wifiinitcfg.get()));

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_start());

            event_group = eventgroup::make_eventgroup();

            state = state_t::STARTED;

            const auto ssid = nvs_ssid();
            const auto password = nvs_password();

            if (not ssid.empty() and not password.empty())
            {
                ESP_LOGI(TAG, "Connecting to %s", ssid.c_str());
                wifi_config_t wifi_config{};
                std::copy(ssid.begin(), ssid.end(), wifi_config.sta.ssid);
                std::copy(password.begin(), password.end(), wifi_config.sta.password);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                ESP_ERROR_CHECK(esp_wifi_connect());
            }
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
        static constexpr auto taskstacksize = 640 * sizeof(int);

        static eventgroup::Eventgroup event_group;
    };

} // namespace wifi