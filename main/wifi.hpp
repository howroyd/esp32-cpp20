#pragma once

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
        ~Wifi();

        static bool clear_nvs_on_construction;
        [[nodiscard, gnu::pure]] static state_t get_state() noexcept { return state; }
        [[nodiscard, gnu::pure]] static wifi_config_t get_config()
        {
            wifi_config_t wifi_config{};
            esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
            return wifi_config;
        }

        [[nodiscard]] std::string nvs_ssid() const;
        [[nodiscard]] std::string nvs_password() const;
        bool nvs_set(const wifi_config_t &config);
        bool nvs_erase();

        bool disconnect();
        bool reconnect_to(wifi_config_t &wifi_config);

    protected:
        static constexpr const char *const TAG{"Wifi"};
        static state_t state;
        static netif::Netif sta_netif;
        static std::unique_ptr<wifi_init_config_t> wifiinitcfg;
        static nvs::Nvs storage;

        Wifi();

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

            static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
        };

        Wifi(const Wifi &) = delete;
        Wifi &operator=(const Wifi &) = delete;

        static task::Task taskhandle;
        [[noreturn]] static void taskfn(void *param);
        static constexpr auto taskstacksize = 640 * sizeof(int);

        static eventgroup::Eventgroup event_group;
    };

} // namespace wifi