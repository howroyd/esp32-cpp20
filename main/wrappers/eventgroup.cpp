#include "esp_log.h"

#include "eventgroup.hpp"

namespace eventgroup
{

    void Deleter::operator()(EventGroupHandle_t freertoshandle) const
    {
        if (freertoshandle)
        {
            ESP_LOGI("EventgroupDeleter", "Deleting event group");
            vEventGroupDelete(freertoshandle);
        }
    }

    static constexpr Deleter deleter;

    Eventgroup make_eventgroup_from_handle(EventGroupHandle_t freertoshandle)
    {
        return Eventgroup{freertoshandle, deleter};
    }

    Eventgroup make_eventgroup()
    {
        return make_eventgroup_from_handle(xEventGroupCreate());
    }

} // namespace eventgroup