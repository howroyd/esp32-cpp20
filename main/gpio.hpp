#pragma once

#include "singleton.hpp"
#include "wrappers/sharablequeue.hpp"

#include "driver/gpio.h"
#include "esp_system.h"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

static constexpr bool operator==(const gpio_config_t &lhs, const gpio_config_t &rhs) noexcept
{
    return std::tie(lhs.pin_bit_mask, lhs.mode, lhs.pull_up_en, lhs.pull_down_en, lhs.intr_type) ==
           std::tie(rhs.pin_bit_mask, rhs.mode, rhs.pull_up_en, rhs.pull_down_en, rhs.intr_type);
}

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

    [[nodiscard, gnu::const]] static constexpr bool config_is_empty(const gpio_config_t &config) noexcept
    {
        constexpr auto empty = gpio_config_t{};
        return config == empty;
    }

    using ArduinoIdfPair = std::pair<const std::string, gpio_num_t>;

    static constexpr std::array<ArduinoIdfPair, 20> arduino_idf_map{{
        {"D2", gpio_num_t::GPIO_NUM_26},
        {"D3", gpio_num_t::GPIO_NUM_25},
        {"D4", gpio_num_t::GPIO_NUM_17},
        {"D5", gpio_num_t::GPIO_NUM_16},
        {"D6", gpio_num_t::GPIO_NUM_27},
        {"D7", gpio_num_t::GPIO_NUM_14},
        {"D8", gpio_num_t::GPIO_NUM_12},
        {"D9", gpio_num_t::GPIO_NUM_13},
        {"D10", gpio_num_t::GPIO_NUM_5},
        {"D11", gpio_num_t::GPIO_NUM_23},
        {"D12", gpio_num_t::GPIO_NUM_19},
        {"D13", gpio_num_t::GPIO_NUM_18},
        {"D14", gpio_num_t::GPIO_NUM_21},
        {"D15", gpio_num_t::GPIO_NUM_22},
        {"A0", gpio_num_t::GPIO_NUM_2},
        {"A1", gpio_num_t::GPIO_NUM_4},
        {"A2", gpio_num_t::GPIO_NUM_35},
        {"A3", gpio_num_t::GPIO_NUM_34},
        {"A4", gpio_num_t::GPIO_NUM_36},
        {"A5", gpio_num_t::GPIO_NUM_39},
    }};

    static constexpr gpio_num_t arduino_to_idf(const std::string &arduino_pin)
    {
        for (const auto &[arduino, idf] : arduino_idf_map)
            if (arduino == arduino_pin)
                return idf;
        return gpio_num_t::GPIO_NUM_NC;
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

        ~GpioBase() = default;

        bool initialise_pin(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr = nullptr, void *isr_args = nullptr);
        bool deinitilise_pin(gpio_num_t pin);
        [[nodiscard]] static bool in_use(gpio_num_t pin);
        [[nodiscard]] static std::optional<gpio_config_t> find_config(gpio_num_t pin);

    protected:
        friend Singleton<GpioBase>; // NOTE: So Singleton can use our private/protected constructor
        friend Gpio;

        static std::unordered_set<gpio_num_t> used_pins;
        static std::unordered_map<gpio_num_t, gpio_config_t> configs;
        static std::mutex mutex;

        GpioBase() = default;
        GpioBase(const GpioBase &) = delete;
        GpioBase(GpioBase &&) = delete;
        GpioBase &operator=(const GpioBase &) = delete;
        GpioBase &operator=(GpioBase &&) = delete;

        //GpioBase(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr = nullptr, void *isr_args = nullptr);

    private:
        gpio_num_t pin;
        gpio_isr_t isr{nullptr};
        IsrArgs isr_args{};
        std::shared_ptr<IsrQueue> isr_queue{};
    };

    class Gpio
    {
        GpioBase::Shared base;
        gpio_num_t pin;

    public:
        Gpio(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr = nullptr, void *isr_args = nullptr) : base{GpioBase::get_shared()}, pin{pin} {
            base->initialise_pin(pin, config, isr, isr_args);
        }
        ~Gpio() {
            base->deinitilise_pin(pin);
        }

        [[nodiscard]] gpio_num_t get_pin() const { return base->get_pin(); }
        [[nodiscard]] std::shared_ptr<IsrQueue> get_queue() const { return base->get_queue(); }
    };

} // namespace gpio