#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include "wifi.hpp"

namespace wifi
{

    [[nodiscard, gnu::pure]] SsidPasswordView config_to_ssidpasswordview(const wifi_config_t &config)
    {
        return {std::string_view(reinterpret_cast<const char *>(config.sta.ssid), sizeof(config.sta.ssid)),
                std::string_view(reinterpret_cast<const char *>(config.sta.password), sizeof(config.sta.password))};
    }

    bool Wifi::clear_nvs_on_construction{false};
    state_t Wifi::state{state_t::IDLE};
    netif::Netif Wifi::sta_netif;
    std::unique_ptr<wifi_init_config_t> Wifi::wifiinitcfg{new wifi_init_config_t(WIFI_INIT_CONFIG_DEFAULT())};
    nvs::Nvs Wifi::storage{};
    task::Task Wifi::taskhandle{};
    eventgroup::Eventgroup Wifi::event_group{};

    Wifi::Wifi()
    {
        ESP_LOGD(TAG, "Constructing instance");

        storage = nvs::make_nvs(TAG, NVS_READWRITE);
        assert(storage);

        if (clear_nvs_on_construction)
            nvs_erase();

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

    Wifi::~Wifi()
    {
        ESP_LOGD(TAG, "Deconstructing instance");

        taskhandle.reset();

        ESP_LOGD(TAG, "esp_wifi_disconnect");
        esp_wifi_disconnect();

        ESP_LOGD(TAG, "esp_wifi_stop");
        esp_wifi_stop();

        ESP_LOGD(TAG, "esp_wifi_deinit");
        esp_wifi_deinit();

        ESP_LOGD(TAG, "esp_wifi_clear_default_wifi_driver_and_handlers");
        esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif.get());

        ESP_LOGD(TAG, "esp_event_handler_unregister");
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler);
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandlers::event_handler);

        ESP_LOGD(TAG, "esp_netif_deinit");
        esp_netif_deinit(); // FIXME: Apparently not yet implemented by Espressif

        event_group.reset();
        sta_netif.reset();
        storage.reset();

        state = state_t::IDLE;
        ESP_LOGI(TAG, "Wifi deconstructed");
    }

    std::string Wifi::nvs_ssid() const
    {
        return nvs::get_string(storage, "ssid");
    }

    std::string Wifi::nvs_password() const
    {
        return nvs::get_string(storage, "password");
    }

    bool Wifi::nvs_set(const wifi_config_t &config)
    {
        const auto [ssid, password] = config_to_ssidpasswordview(config);
        return nvs::set_strings(storage, std::make_pair("ssid", ssid), std::make_pair("password", password));
    }

    bool Wifi::nvs_erase()
    {
        return nvs::erase_keys(storage, "ssid", "password");
    }

    bool Wifi::disconnect()
    {
        auto status = esp_wifi_disconnect();
        if (ESP_OK != status)
        {
            ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(status));
            return false;
        }
        return true;
    }

    bool Wifi::reconnect_to(wifi_config_t &wifi_config)
    {
        auto status = esp_wifi_disconnect();

        status = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (ESP_OK != status)
        {
            ESP_LOGE(TAG, "Failed to set config: %s", esp_err_to_name(status));
            return false;
        }

        status = esp_wifi_connect();
        if (ESP_OK != status)
        {
            ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(status));
            return false;
        }

        return true;
    }

    void Wifi::EventHandlers::event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        args_t args{static_cast<wifi_event_t>(event_id), event_data};

        auto weakinstance = get_weak();
        if (weakinstance.expired()) [[unlikely]]
        {
            ESP_LOGE(TAG, "Event %u ignored; no instance of this class exists", args.event_id);
            return;
        }

        if (WIFI_EVENT == event_base)
            handle_wifi_event(args);
        else if (IP_EVENT == event_base)
            handle_ip_event(args);
        else [[unlikely]]
            ESP_LOGW(TAG, "Unhandled event %s", event_base);
    }

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
                const auto [ssid, password] = config_to_ssidpasswordview(get_config());
                ESP_LOGI(TAG, "WiFi Connected to AP %.*s", ssid.size(), ssid.data());
                state = state_t::DONE;

                task::log_stack(TAG, taskstacksize);

                taskhandle.reset(); // NOTE: This won't return
            }
        }
    }

} // namespace wifi