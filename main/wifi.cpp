#include "wifi.hpp"

namespace wifi
{

    SsidPasswordView config_to_ssidpasswordview(const wifi_config_t &config)
    {
        return {std::string_view(reinterpret_cast<const char *>(config.sta.ssid), sizeof(config.sta.ssid)),
                std::string_view(reinterpret_cast<const char *>(config.sta.password), sizeof(config.sta.password))};
    }

    state_t Wifi::state{state_t::IDLE};
    netif::Netif Wifi::sta_netif;
    std::unique_ptr<wifi_init_config_t> Wifi::wifiinitcfg{new wifi_init_config_t(WIFI_INIT_CONFIG_DEFAULT())};
    nvs::Nvs Wifi::storage{nvs::make_nvs(TAG, NVS_READWRITE)};
    task::Task Wifi::taskhandle{};
    eventgroup::Eventgroup Wifi::event_group{};

    Wifi::EventHandlers::handle_success_t Wifi::EventHandlers::handle_wifi_event(Wifi::EventHandlers::args_t args)
    {
        switch (args.event_id)
        {
        case WIFI_EVENT_STA_START:
            assert(not taskhandle);
            taskhandle = task::make_task(taskfn, TAG, taskstacksize, nullptr, 3);
            ESP_LOGI(TAG, "Started task at %p", taskhandle.get());
            break;
        case WIFI_EVENT_STA_CONNECTED:
            state = state_t::CONNECTED;
            ESP_LOGI(TAG, "Connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            eventgroup::clear_bits(event_group, CONNECTED_BIT);
            break;
        [[unlikely]] default:
            ESP_LOGW(TAG, "Unhandled WIFI_EVENT %d", args.event_id);
            return handle_success_t::UNHANDLED;
        };
        return handle_success_t::OK;
    }

    Wifi::EventHandlers::handle_success_t Wifi::EventHandlers::handle_ip_event(Wifi::EventHandlers::args_t args)
    {
        switch (args.event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            state = state_t::GOT_IP;
            eventgroup::set_bits(event_group, CONNECTED_BIT);
            break;
        [[unlikely]] default:
            ESP_LOGW(TAG, "Unhandled IP_EVENT %d", args.event_id);
            return handle_success_t::UNHANDLED;
        };
        return handle_success_t::OK;
    }

    void Wifi::taskfn(void *param)
    {
        while (true)
        {
            // const auto uxBits = xEventGroupWaitBits(event_group.get(), CONNECTED_BIT, true, false, portMAX_DELAY);
            const auto bits = eventgroup::wait_bits(event_group, CONNECTED_BIT, true, false);

            // if (uxBits & CONNECTED_BIT)
            if (bits)
            {
                ESP_LOGI(TAG, "WiFi Connected to AP");
                state = state_t::DONE;

                task::log_stack(TAG, taskstacksize);

                taskhandle.reset(); // NOTE: This won't return
            }
        }
    }

} // namespace wifi