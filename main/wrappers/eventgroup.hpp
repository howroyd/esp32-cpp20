#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <memory>

namespace eventgroup
{

    struct Deleter
    {
        using pointer = EventGroupHandle_t;

        void operator()(EventGroupHandle_t freertoshandle) const;
    };

    using Eventgroup = std::unique_ptr<EventGroupHandle_t, Deleter>;

    [[nodiscard]] Eventgroup make_eventgroup_from_handle(EventGroupHandle_t freertoshandle);
    [[nodiscard]] Eventgroup make_eventgroup();

} // namespace eventgroup