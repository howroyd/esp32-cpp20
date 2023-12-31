#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include "smartconfig.hpp"

namespace sc
{

    [[nodiscard, gnu::pure]] constexpr wifi_config_t ssid_pswd_to_config(const smartconfig_event_got_ssid_pswd_t &evt)
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

    state_t SmartConfig::state{state_t::IDLE};
    smartconfig_start_config_t SmartConfig::_config = SMARTCONFIG_START_CONFIG_DEFAULT(); // FIXME ref https://github.com/espressif/esp-idf/pull/12867
    std::unique_ptr<smartconfig_start_config_t> SmartConfig::smartconfigcfg(new smartconfig_start_config_t(_config));
    wifi::Wifi::Shared SmartConfig::wifiobj{nullptr};
    task::Task SmartConfig::taskhandle{};
    eventgroup::Eventgroup SmartConfig::event_group{};

    SmartConfig::SmartConfig()
    {
        ESP_LOGD(TAG, "Constructing instance");

        if (not wifiobj)
            wifiobj = wifi::Wifi::get_shared();

        event_group = eventgroup::make_eventgroup();

        ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler, nullptr));

        assert(not taskhandle);
        taskhandle = task::make_task(taskfn, TAG, taskstacksize, nullptr, 3);

        ESP_ERROR_CHECK(esp_smartconfig_start(smartconfigcfg.get()));
        state = state_t::STARTED;
    }

    SmartConfig::~SmartConfig()
    {
        ESP_LOGD(TAG, "Deconstructing instance");

        taskhandle.reset();

        esp_smartconfig_stop();

        esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler);

        event_group.reset();
        wifiobj.reset();
    }

    void SmartConfig::EventHandlers::event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        args_t args{static_cast<smartconfig_event_t>(event_id), event_data};

        auto weakinstance = get_weak();
        if (weakinstance.expired())
        {
            ESP_LOGW(TAG, "Event %u ignored; no instance of this class exists", args.event_id);
            return;
        }

        auto success = handle_success_t::UNHANDLED;

        if (SC_EVENT == event_base)
            success = handle_sc_event(args);

        if (handle_success_t::OK != success)
            ESP_LOGW(TAG, "Event %u unhandled with status %d", args.event_id, static_cast<int>(success));
    }

    SmartConfig::EventHandlers::handle_success_t SmartConfig::EventHandlers::handle_sc_event(SmartConfig::EventHandlers::args_t args)
    {
        switch (args.event_id)
        {
        case SC_EVENT_SCAN_DONE:
            ESP_LOGD(TAG, "Scan done");
            break;
        case SC_EVENT_FOUND_CHANNEL:
            ESP_LOGD(TAG, "Found channel");
            break;
        case SC_EVENT_GOT_SSID_PSWD:
        {
            ESP_LOGI(TAG, "Got SSID and password");

            wifi_config_t wifi_config{ssid_pswd_to_config(*reinterpret_cast<const smartconfig_event_got_ssid_pswd_t *>(args.event_data))};

            const auto [ssidview, passwordview] = wifi::config_to_ssidpasswordview(wifi_config);
            ESP_LOGI(TAG, "SSID:%.*s", ssidview.size(), ssidview.data());
            ESP_LOGD(TAG, "PASSWORD:%.*s", passwordview.size(), passwordview.data());

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
            const auto bits = eventgroup::wait_bits(event_group, ESPTOUCH_DONE_BIT, true, false);

            if (bits)
            {
                wifiobj->nvs_set(wifiobj->get_config());
                const auto [ssid, password] = wifi::config_to_ssidpasswordview(wifiobj->get_config());
                ESP_LOGI(TAG, "Saving creds for %.*s to NVS", ssid.size(), ssid.data());
                ESP_LOGI(TAG, "Done!");
                esp_smartconfig_stop();
                state = state_t::DONE;

                task::log_stack(TAG, taskstacksize);

                taskhandle.reset(); // NOTE: This won't return
            }
        }
    }

} // namespace sc