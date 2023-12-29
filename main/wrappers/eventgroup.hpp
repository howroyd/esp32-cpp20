#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <bitset>
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

    static_assert((sizeof(EventBits_t) * 8) <= (sizeof(unsigned long) * 8));
    using Eventbits = std::bitset<sizeof(EventBits_t) * 8>;

    inline EventBits_t Eventbits_to_EventBits_t(const Eventbits &bits)
    {
        if constexpr ((sizeof(EventBits_t) * 8) <= (sizeof(unsigned long) * 8))
        {
            return bits.to_ulong();
        }
        else
        {
            return bits.to_ullong();
        }
    }

    struct BitsReturn
    {
        Eventbits bits;
        bool success = true;
        bool higher_priority_task_woken = false;
        operator bool() const { return success; } // NOTE: Success that bits were returned, not that the bits were set
    };

    [[nodiscard]] BitsReturn get_bits(const Eventgroup &event_group);
    [[nodiscard]] BitsReturn get_bits_from_isr(const Eventgroup &event_group);
    [[nodiscard]] BitsReturn clear_bits(Eventgroup &event_group, Eventbits bits);
    [[nodiscard]] BitsReturn clear_bits_from_isr(Eventgroup &event_group, Eventbits bits);
    BitsReturn set_bits(Eventgroup &event_group, Eventbits bits);
    [[nodiscard]] BitsReturn set_bits_from_isr(Eventgroup &event_group, Eventbits bits);
    [[nodiscard]] BitsReturn wait_bits(Eventgroup &event_group, Eventbits bits, bool clear_on_exit, bool wait_for_all_bits, TickType_t ticks_to_wait);

} // namespace eventgroup