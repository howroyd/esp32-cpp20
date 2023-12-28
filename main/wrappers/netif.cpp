#include "esp_log.h"

#include "netif.hpp"

namespace netif
{

    void NetifWifiDeleter::operator()(esp_netif_t *instance) const
    {
        if (instance)
        {
            ESP_LOGI("NetifWifiDeleter", "Deleting netif");
            esp_netif_destroy(instance);
        }
    }

    Netif make_netif(esp_netif_t *espnetifptr)
    {
        return Netif{espnetifptr, NetifWifiDeleter()};
    }

} // namespace netif