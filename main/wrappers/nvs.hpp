#pragma once

#include "nvs.h"
#include "nvs_flash.h"

#include <memory>
#include <string>
#include <string_view>

namespace nvs
{

    static constexpr auto TAG{"wrappers/nvs"};

    struct Deleter
    {
        using pointer = nvs_handle_t *;

        void operator()(nvs_handle_t *espidfhandle) const;
    };

    using Nvs = std::unique_ptr<nvs_handle_t, Deleter>;

    [[nodiscard]] Nvs make_nvs_from_handle(nvs_handle_t espidfhandle);
    [[nodiscard]] Nvs make_nvs(const char *partition_name, const char *namespace_name, nvs_open_mode_t open_mode = NVS_READWRITE);
    [[nodiscard]] Nvs make_nvs(const char *namespace_name, nvs_open_mode_t open_mode = NVS_READWRITE);

    esp_err_t initialise_nvs();

    bool erase_key(Nvs &handle, const char *key);

    [[nodiscard]] std::string get_string(Nvs &handle, const char *key);
    bool set_string(Nvs &handle, const char *key, std::string_view value);

} // namespace nvs