#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "nvs.hpp"

#include <cstdint>

namespace nvs
{

    [[nodiscard, gnu::const]] static nvs_handle_t get_espidfpointer(Nvs &espidfhandle)
    {
        return static_cast<nvs_handle_t>(reinterpret_cast<std::uintptr_t>(espidfhandle.get()));
    }

    void Deleter::operator()(nvs_handle_t *espidfhandlepointerofdoom) const
    {
        if (espidfhandlepointerofdoom)
        {
            ESP_LOGD(TAG, "Closing NVS connection");
            auto espidfhandle{static_cast<nvs_handle_t>(reinterpret_cast<std::uintptr_t>(espidfhandlepointerofdoom))};
            nvs_close(espidfhandle);
        }
    }

    static constexpr Deleter deleter;

    Nvs make_nvs_from_handle(nvs_handle_t espidfhandle)
    {
        return Nvs{(nvs_handle_t *)espidfhandle, deleter};
    }

    Nvs make_nvs(const char *namespace_name, nvs_open_mode_t open_mode)
    {
        nvs_handle_t out_handle{};
        auto success = nvs_open(namespace_name, open_mode, &out_handle);

        if (ESP_ERR_NVS_NOT_INITIALIZED == success)
        {
            initialise_nvs();
            success = nvs_open(namespace_name, open_mode, &out_handle);
        }

        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s", namespace_name, esp_err_to_name(success));
            return nullptr;
        }
        ESP_LOGI(TAG, "Opened NVS namespace %s with handle %lu", namespace_name, out_handle);
        return make_nvs_from_handle(out_handle);
    }

    esp_err_t initialise_nvs()
    {
        esp_err_t ret = nvs_flash_init();
        ESP_LOGD(TAG, "nvs_flash_init returned %s", esp_err_to_name(ret));

        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            // NVS partition was truncated and needs to be erased
            // Retry nvs_flash_init
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
            ESP_LOGD(TAG, "nvs_flash_init returned %s", esp_err_to_name(ret));
        }

        if (ESP_OK != ret)
            ESP_LOGE(TAG, "Failed to initialise NVS: %s", esp_err_to_name(ret));
        else
            ESP_LOGI(TAG, "Initialised NVS");

        return ret;
    }

    bool commit(Nvs &handle)
    {
        const auto espidfhandle{get_espidfpointer(handle)};

        auto success = nvs_commit(espidfhandle);
        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to commit NVS %lu: %s", espidfhandle, esp_err_to_name(success));
            return false;
        }
        return true;
    }

    bool erase_key(Nvs &handle, const char *key, bool docommit)
    {
        const auto espidfhandle{get_espidfpointer(handle)};

        auto success = nvs_erase_key(espidfhandle, key);

        if (ESP_OK == success and docommit)
            success = nvs_commit(espidfhandle);

        if (success)
            ESP_LOGI(TAG, "Erased %s from NVS %lu", key, espidfhandle);
        else
            ESP_LOGE(TAG, "Failed to erase %s from NVS %lu: %s", key, espidfhandle, esp_err_to_name(success));

        return true;
    }

    std::string get_string(Nvs &handle, const char *key)
    {
        const auto espidfhandle{get_espidfpointer(handle)};

        size_t required_size{};
        const auto exists = nvs_get_str(espidfhandle, key, NULL, &required_size);
        if (exists != ESP_OK)
        {
            ESP_LOGD(TAG, "No existing key %s in NVS %lu: %s", key, espidfhandle, esp_err_to_name(exists));
            return std::string();
        }
        char *str = new char[required_size];
        nvs_get_str(espidfhandle, key, str, &required_size);
        auto ret = std::string(str);
        delete[] str;
        return ret;
    }

    bool set_string(Nvs &handle, const char *key, std::string_view value, bool docommit)
    {
        const auto espidfhandle{get_espidfpointer(handle)};
        const std::string str{value};

        if (str == get_string(handle, key))
        {
            ESP_LOGI(TAG, "Not setting %s to %s in NVS %lu since it's already set to that", key, str.c_str(), espidfhandle);
            return true;
        }

        auto success = nvs_set_str(espidfhandle, key, str.c_str());
        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set %s to %s in NVS %lu: %s", key, str.c_str(), espidfhandle, esp_err_to_name(success));
            return false;
        }

        ESP_LOGI(TAG, "Set %s to %s in NVS %lu", key, str.c_str(), espidfhandle);

        if (docommit)
            success = nvs_commit(espidfhandle);

        return true;
    }

} // namespace nvs