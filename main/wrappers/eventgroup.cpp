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

    BitsReturn get_bits(const Eventgroup &event_group)
    {
        return {xEventGroupGetBits(event_group.get())};
    }

    BitsReturn get_bits_from_isr(const Eventgroup &event_group)
    {
        return {xEventGroupGetBitsFromISR(event_group.get())};
    }

    BitsReturn clear_bits(Eventgroup &event_group, Eventbits bits)
    {
        assert(bits.any());

        return {xEventGroupClearBits(event_group.get(), Eventbits_to_EventBits_t(bits))};
    }

    BitsReturn clear_bits_from_isr(Eventgroup &event_group, Eventbits bits)
    {
        assert(bits.any());

        auto before = get_bits_from_isr(event_group);
        auto success = xEventGroupClearBitsFromISR(event_group.get(), Eventbits_to_EventBits_t(bits));
        return {before.bits, before.success && pdPASS == success};
    }

    BitsReturn set_bits(Eventgroup &event_group, Eventbits bits)
    {
        return {xEventGroupSetBits(event_group.get(), Eventbits_to_EventBits_t(bits))};
    }

    BitsReturn set_bits_from_isr(Eventgroup &event_group, Eventbits bits)
    {
        const auto before = get_bits_from_isr(event_group);
        BaseType_t higher_priority_task_woken = pdFALSE;
        const auto success = xEventGroupSetBitsFromISR(event_group.get(), Eventbits_to_EventBits_t(bits), &higher_priority_task_woken);
        return {before.bits, before.success && pdPASS == success, pdTRUE == higher_priority_task_woken};
    }

    BitsReturn wait_bits(Eventgroup &event_group, Eventbits bits, bool clear_on_exit, bool wait_for_all_bits, TickType_t ticks_to_wait)
    {
        assert(bits.any());

        const Eventbits bitsreceived = xEventGroupWaitBits(event_group.get(), Eventbits_to_EventBits_t(bits), clear_on_exit ? pdTRUE : pdFALSE, wait_for_all_bits ? pdTRUE : pdFALSE, ticks_to_wait);

        const auto masked = bitsreceived & bits;

        if (wait_for_all_bits)
        {
            const auto invertedmask = ~bits;
            return {bitsreceived, (masked | invertedmask).all()};
        }
        else
            return {bitsreceived, masked.any()};
    }

} // namespace eventgroup