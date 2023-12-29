#include "esp_log.h"

#include "nvs.hpp"

namespace nvs
{

    void Deleter::operator()(nvs_handle_t *espidfhandle) const
    {
        if (espidfhandle)
        {
            ESP_LOGI(TAG, "Closing NVS connection");
            nvs_close((uint32_t)espidfhandle);
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
        ESP_LOGI(TAG, "nvs_flash_init returned %s", esp_err_to_name(ret));

        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            // NVS partition was truncated and needs to be erased
            // Retry nvs_flash_init
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
            ESP_LOGW(TAG, "nvs_flash_init returned %s", esp_err_to_name(ret));
        }
        return ret;
    }

    bool erase_key(Nvs &handle, const char *key)
    {
        const auto espidfhandle = (nvs_handle_t)handle.get();

        auto success = nvs_erase_key(espidfhandle, key);
        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase %s from NVS %lu: %s", key, espidfhandle, esp_err_to_name(success));
            return false;
        }
        success = nvs_commit(espidfhandle);
        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to commit %s to NVS %lu: %s", key, espidfhandle, esp_err_to_name(success));
            return false;
        }
        return true;
    }

    std::string get_string(Nvs &handle, const char *key)
    {
        const auto espidfhandle = (nvs_handle_t)handle.get();

        size_t required_size{};
        const auto exists = nvs_get_str(espidfhandle, key, NULL, &required_size);
        if (exists != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get %s from NVS %lu: %s", key, espidfhandle, esp_err_to_name(exists));
            return std::string();
        }
        char *str = new char[required_size];
        nvs_get_str(espidfhandle, key, str, &required_size);
        auto ret = std::string(str);
        delete[] str;
        return ret;
    }

    bool set_string(Nvs &handle, const char *key, std::string_view value)
    {
        const auto espidfhandle = (nvs_handle_t)handle.get();
        const std::string str{value};

        auto success = nvs_set_str(espidfhandle, key, str.c_str());
        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set %s to %s in NVS %lu: %s", key, str.c_str(), espidfhandle, esp_err_to_name(success));
            return false;
        }
        success = nvs_commit(espidfhandle);
        if (success != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to commit %s to %s in NVS %lu: %s", key, str.c_str(), espidfhandle, esp_err_to_name(success));
            return false;
        }
        return true;
    }

} // namespace eventgroup