#pragma once

#include "singleton.hpp"
#include "wrappers/sharablequeue.hpp"

#include "driver/gpio.h"
#include "esp_system.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace gpio
{

    static constexpr const char *const TAG{"Gpio"};

    [[nodiscard, gnu::const]] static constexpr std::string int_type_to_string(gpio_int_type_t state)
    {
        switch (state)
        {
        case gpio_int_type_t::GPIO_INTR_DISABLE:
            return "INTR_DISABLE";
        case gpio_int_type_t::GPIO_INTR_POSEDGE:
            return "INTR_POSEDGE";
        case gpio_int_type_t::GPIO_INTR_NEGEDGE:
            return "INTR_NEGEDGE";
        case gpio_int_type_t::GPIO_INTR_ANYEDGE:
            return "INTR_ANYEDGE";
        case gpio_int_type_t::GPIO_INTR_LOW_LEVEL:
            return "INTR_LOW_LEVEL";
        case gpio_int_type_t::GPIO_INTR_HIGH_LEVEL:
            return "INTR_HIGH_LEVEL";
        [[unlikely]] default:
            return "Unknown";
        }
    }

    struct IsrRet
    {
        gpio_num_t pin;
        gpio_int_type_t state{gpio_int_type_t::GPIO_INTR_DISABLE};
    };

    using IsrQueue = queue::SharableQueue<IsrRet>;

    struct IsrArgs
    {
        gpio_num_t pin;
        gpio_config_t config; // TODO merge or reference with map
        std::weak_ptr<IsrQueue> queue{};
        void *isr_args;
    };

    class Gpio;

    class GpioBase : public Singleton<GpioBase> // NOTE: CRTP
    {
    public:
        gpio_num_t get_pin() const { return pin; }
        std::shared_ptr<IsrQueue> get_queue() const { return isr_queue; }

        ~GpioBase();

    protected:
        friend Singleton<GpioBase>; // NOTE: So Singleton can use our private/protected constructor
        friend Gpio;

        static std::unordered_set<gpio_num_t> used_pins;
        static std::unordered_map<gpio_num_t, gpio_config_t> configs;
        static std::mutex mutex;

        GpioBase() = delete;
        GpioBase(const GpioBase &) = delete;
        GpioBase(GpioBase &&) = delete;
        GpioBase &operator=(const GpioBase &) = delete;
        GpioBase &operator=(GpioBase &&) = delete;

        GpioBase(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr = nullptr, void *isr_args = nullptr);

    private:
        gpio_num_t pin;
        gpio_isr_t isr{nullptr};
        IsrArgs isr_args{};
        std::shared_ptr<IsrQueue> isr_queue{};
    };

    class Gpio
    {
        GpioBase::Shared base;

    public:
        Gpio(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr = nullptr, void *isr_args = nullptr) : base{GpioBase::get_shared(pin, config, isr, isr_args)} {}

        [[nodiscard]] gpio_num_t get_pin() const { return base->get_pin(); }
        [[nodiscard]] std::shared_ptr<IsrQueue> get_queue() const { return base->get_queue(); }
    };

    // struct Deleter
    // {
    //     using pointer = EventGroupHandle_t;

    //     void operator()(EventGroupHandle_t freertoshandle) const;
    // };

    // using Eventgroup = std::unique_ptr<EventGroupHandle_t, Deleter>;

    // [[nodiscard]] Eventgroup make_eventgroup_from_handle(EventGroupHandle_t freertoshandle);
    // [[nodiscard]] Eventgroup make_eventgroup();

    // using Eventbits = std::bitset<n_event_bits>;

    // struct BitsReturn
    // {
    //     Eventbits bits;
    //     bool success = true;
    //     operator bool() const { return success; } // NOTE: Success that bits were returned, not that the bits were set
    // };

    // [[nodiscard]] BitsReturn get_bits(const Eventgroup &event_group);
    // [[nodiscard]] IRAM_ATTR BitsReturn get_bits_from_isr(const Eventgroup &event_group);
    // BitsReturn clear_bits(Eventgroup &event_group, Eventbits bits);
    // IRAM_ATTR BitsReturn clear_bits_from_isr(Eventgroup &event_group, Eventbits bits);
    // BitsReturn set_bits(Eventgroup &event_group, Eventbits bits);
    // IRAM_ATTR BitsReturn set_bits_from_isr(Eventgroup &event_group, Eventbits bits);
    // [[nodiscard]] BitsReturn wait_bits(Eventgroup &event_group, Eventbits bits, bool clear_on_exit, bool wait_for_all_bits, std::chrono::milliseconds wait_time = std::chrono::milliseconds::max());

} // namespace gpio