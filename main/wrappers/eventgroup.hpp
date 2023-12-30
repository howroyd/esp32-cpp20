#pragma once

#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <bitset>
#include <chrono>
#include <memory>

namespace eventgroup
{

    static constexpr auto n_event_bits = sizeof(EventBits_t) * 8;
    static_assert(n_event_bits <= (sizeof(unsigned long long) * 8), "EventBits_t is too large");

    struct Deleter
    {
        using pointer = EventGroupHandle_t;

        void operator()(EventGroupHandle_t freertoshandle) const;
    };

    using Eventgroup = std::unique_ptr<EventGroupHandle_t, Deleter>;

    [[nodiscard]] Eventgroup make_eventgroup_from_handle(EventGroupHandle_t freertoshandle);
    [[nodiscard]] Eventgroup make_eventgroup();

    using Eventbits = std::bitset<n_event_bits>;

    struct BitsReturn
    {
        Eventbits bits;
        bool success = true;
        operator bool() const { return success; } // NOTE: Success that bits were returned, not that the bits were set
    };

    [[nodiscard]] BitsReturn get_bits(const Eventgroup &event_group);
    [[nodiscard]] IRAM_ATTR BitsReturn get_bits_from_isr(const Eventgroup &event_group);
    BitsReturn clear_bits(Eventgroup &event_group, Eventbits bits);
    IRAM_ATTR BitsReturn clear_bits_from_isr(Eventgroup &event_group, Eventbits bits);
    BitsReturn set_bits(Eventgroup &event_group, Eventbits bits);
    IRAM_ATTR BitsReturn set_bits_from_isr(Eventgroup &event_group, Eventbits bits);
    [[nodiscard]] BitsReturn wait_bits(Eventgroup &event_group, Eventbits bits, bool clear_on_exit, bool wait_for_all_bits, std::chrono::milliseconds wait_time = std::chrono::milliseconds::max());

} // namespace eventgroup