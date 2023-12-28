#include "smartconfig.hpp"

namespace sc
{

    state_t SmartConfig::state{state_t::IDLE};
    smartconfig_start_config_t SmartConfig::_config = SMARTCONFIG_START_CONFIG_DEFAULT(); // FIXME ref https://github.com/espressif/esp-idf/pull/12867
    std::unique_ptr<smartconfig_start_config_t> SmartConfig::smartconfigcfg(new smartconfig_start_config_t(_config));
    wifi::Wifi::Shared SmartConfig::wifiobj{nullptr};
    task::Task SmartConfig::taskhandle{};
    eventgroup::Eventgroup SmartConfig::event_group{};

    SmartConfig::EventHandlers::handle_success_t SmartConfig::EventHandlers::handle_sc_event(SmartConfig::EventHandlers::args_t args)
    {
        switch (args.event_id)
        {
        case SC_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "Scan done");
            break;
        case SC_EVENT_FOUND_CHANNEL:
            ESP_LOGI(TAG, "Found channel");
            break;
        case SC_EVENT_GOT_SSID_PSWD:
        {
            ESP_LOGI(TAG, "Got SSID and password");

            const smartconfig_event_got_ssid_pswd_t *const evt = reinterpret_cast<const smartconfig_event_got_ssid_pswd_t *>(args.event_data);
            wifi_config_t wifi_config{};

            std::copy(evt->ssid, evt->ssid + std::min(sizeof(evt->ssid), sizeof(wifi_config.sta.ssid)), wifi_config.sta.ssid);
            std::copy(evt->password, evt->password + std::min(sizeof(evt->password), sizeof(wifi_config.sta.password)), wifi_config.sta.password);
            if (evt->bssid_set)
            {
                wifi_config.sta.bssid_set = evt->bssid_set;
                std::copy(evt->bssid, evt->bssid + std::min(sizeof(evt->bssid), sizeof(wifi_config.sta.bssid)), wifi_config.sta.bssid);
            }

            std::string_view ssid_view(reinterpret_cast<const char *>(wifi_config.sta.ssid), sizeof(wifi_config.sta.ssid));
            std::string_view password_view(reinterpret_cast<const char *>(wifi_config.sta.password), sizeof(wifi_config.sta.password));

            ESP_LOGI(TAG, "SSID:%.*s", ssid_view.size(), ssid_view.data());
            ESP_LOGI(TAG, "PASSWORD:%.*s", password_view.size(), password_view.data());

            if (SC_TYPE_ESPTOUCH_V2 == evt->type)
            {
                uint8_t rvd_data[33]{};
                ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
                ESP_LOGI(TAG, "RVD_DATA:");
                for (const auto i : rvd_data)
                {
                    printf("%02x ", i);
                }
                printf("\n");
            }

            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            esp_wifi_connect();
            break;
        }
        case SC_EVENT_SEND_ACK_DONE:
            xEventGroupSetBits(event_group.get(), ESPTOUCH_DONE_BIT);
            break;
        [[unlikely]] default:
            ESP_LOGW(TAG, "Unhandled SC_EVENT %d", args.event_id);
            return handle_success_t::UNHANDLED;
        };
        return handle_success_t::OK;
    }

    void SmartConfig::taskfn(void *param)
    {
        while (true)
        {
            const auto uxBits = xEventGroupWaitBits(event_group.get(), ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);

            if (uxBits & ESPTOUCH_DONE_BIT)
            {
                ESP_LOGI(TAG, "Done!");
                esp_smartconfig_stop();
                state = state_t::DONE;

                task::log_stack(TAG, taskstacksize);

                taskhandle.reset(); // NOTE: This won't return
            }
        }
    }

} // namespace sc