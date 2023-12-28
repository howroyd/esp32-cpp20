#pragma once

#include "esp_netif.h"

#include <memory>

namespace netif
{

    struct NetifWifiDeleter
    {
        using pointer = esp_netif_t *;

        void operator()(esp_netif_t *instance) const;
    };

    using Netif = std::unique_ptr<esp_netif_t, NetifWifiDeleter>;

    [[nodiscard]] Netif make_netif(esp_netif_t *espnetifptr);

} // namespace netif