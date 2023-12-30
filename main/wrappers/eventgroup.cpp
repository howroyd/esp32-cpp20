#include "esp_log.h"

#include "eventgroup.hpp"
#include "task.hpp"

namespace eventgroup
{

    [[gnu::pure]] static EventBits_t eventbits2freertos(const Eventbits &bits)
    {
        if constexpr (n_event_bits <= (sizeof(unsigned long) * 8))
            return bits.to_ulong();
        else
            return bits.to_ullong();
    }

    [[gnu::const]] static constexpr auto bool2pdTrue(bool b) { return b ? pdTRUE : pdFALSE; }

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
        return {freertoshandle, deleter};
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

        return {xEventGroupClearBits(event_group.get(), eventbits2freertos(bits))};
    }

    BitsReturn clear_bits_from_isr(Eventgroup &event_group, Eventbits bits)
    {
        assert(bits.any());

        auto before = get_bits_from_isr(event_group);
        auto success = xEventGroupClearBitsFromISR(event_group.get(), eventbits2freertos(bits));

        return {before.bits, before.success && pdPASS == success};
    }

    BitsReturn set_bits(Eventgroup &event_group, Eventbits bits)
    {
        return {xEventGroupSetBits(event_group.get(), eventbits2freertos(bits))};
    }

    BitsReturn set_bits_from_isr(Eventgroup &event_group, Eventbits bits)
    {
        auto before = get_bits_from_isr(event_group);
        BaseType_t higher_priority_task_woken = pdFALSE;
        auto success = xEventGroupSetBitsFromISR(event_group.get(), eventbits2freertos(bits), &higher_priority_task_woken);

        if (pdTRUE == higher_priority_task_woken)
            portYIELD_FROM_ISR();

        return {before.bits, before.success && pdPASS == success};
    }

    BitsReturn wait_bits(Eventgroup &event_group, Eventbits bits, bool clear_on_exit, bool wait_for_all_bits, std::chrono::milliseconds wait_time)
    {
        assert(bits.any());

        const Eventbits bitsreceived = xEventGroupWaitBits(event_group.get(), eventbits2freertos(bits), bool2pdTrue(clear_on_exit), bool2pdTrue(wait_for_all_bits), task::to_ticks(wait_time));

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