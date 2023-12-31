#pragma once

#include "nvs.h"
#include "nvs_flash.h"

#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

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

    bool commit(Nvs &handle);

    bool erase_key(Nvs &handle, const char *key, bool docommit = true);

    template <std::convertible_to<std::string_view>... Keys>
    bool erase_keys(Nvs &handle, Keys &&...keys)
    {
        auto status = (erase_key(handle, keys, false) and ...);
        if (status)
            status = commit(handle);
        return status;
    }

    [[nodiscard]] std::string get_string(Nvs &handle, const char *key);

    bool set_string(Nvs &handle, const char *key, std::string_view value, bool docommit = true);

    template <std::convertible_to<std::pair<std::string_view, std::string_view>>... Keys>
    bool set_strings(Nvs &handle, Keys &&...keys)
    {
        auto status = (set_string(handle, keys.first, keys.second, false) and ...);
        if (status)
            status = commit(handle);
        return status;
    }

} // namespace nvs