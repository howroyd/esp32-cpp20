#include "smartconfig.hpp"

namespace sc
{

    state_t SmartConfig::state{state_t::IDLE};
    smartconfig_start_config_t SmartConfig::_config = SMARTCONFIG_START_CONFIG_DEFAULT(); // FIXME ref https://github.com/espressif/esp-idf/pull/12867
    std::unique_ptr<smartconfig_start_config_t> SmartConfig::smartconfigcfg(new smartconfig_start_config_t(_config));
    wifi::Wifi::Shared SmartConfig::wifiobj{nullptr};
    task::Task SmartConfig::taskhandle{};
    eventgroup::Eventgroup SmartConfig::event_group{};

    [[nodiscard]] wifi_config_t ssid_pswd_to_config(const smartconfig_event_got_ssid_pswd_t &evt)
    {
        wifi_config_t wifi_config{};

        std::copy(evt.ssid, evt.ssid + std::min(sizeof(evt.ssid), sizeof(wifi_config.sta.ssid)), wifi_config.sta.ssid);
        std::copy(evt.password, evt.password + std::min(sizeof(evt.password), sizeof(wifi_config.sta.password)), wifi_config.sta.password);

        if (evt.bssid_set)
        {
            wifi_config.sta.bssid_set = evt.bssid_set;
            std::copy(evt.bssid, evt.bssid + std::min(sizeof(evt.bssid), sizeof(wifi_config.sta.bssid)), wifi_config.sta.bssid);
        }

        return wifi_config;
    }

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

            wifi_config_t wifi_config{ssid_pswd_to_config(*reinterpret_cast<const smartconfig_event_got_ssid_pswd_t *>(args.event_data))};

            const auto [ssidview, passwordview] = wifi::config_to_ssidpasswordview(wifi_config);
            ESP_LOGI(TAG, "SSID:%.*s", ssidview.size(), ssidview.data());
            ESP_LOGI(TAG, "PASSWORD:%.*s", passwordview.size(), passwordview.data());

            if (ssidview.empty() or passwordview.empty())
            {
                ESP_LOGW(TAG, "SSID or password is empty");
                return handle_success_t::FAIL;
            }

            if (not wifiobj->reconnect_to(wifi_config))
            {
                ESP_LOGW(TAG, "Failed to reconnect");
                return handle_success_t::FAIL;
            }
            break;
        }
        case SC_EVENT_SEND_ACK_DONE:
            eventgroup::set_bits(event_group, ESPTOUCH_DONE_BIT);
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
            // const auto uxBits = xEventGroupWaitBits(event_group.get(), ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
            const auto bits = eventgroup::wait_bits(event_group, ESPTOUCH_DONE_BIT, true, false);

            // if (uxBits & ESPTOUCH_DONE_BIT)
            if (bits)
            {
                const auto [ssidview, passwordview] = wifi::config_to_ssidpasswordview(wifiobj->config());

                if (ssidview.empty() or passwordview.empty()) [[unlikely]]
                {
                    ESP_LOGE(TAG, "SSID or password is empty!");
                }
                else
                {
                    ESP_LOGI(TAG, "Saving creds to NVS");

                    wifiobj->nvs_ssid(ssidview);
                    wifiobj->nvs_password(passwordview);
                }

                ESP_LOGI(TAG, "Done!");
                esp_smartconfig_stop();
                state = state_t::DONE;

                task::log_stack(TAG, taskstacksize);

                taskhandle.reset(); // NOTE: This won't return
            }
        }
    }

} // namespace sc