#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include "gpio.hpp"

namespace gpio
{

    std::unordered_set<gpio_num_t> GpioBase::used_pins{};
    std::unordered_map<gpio_num_t, gpio_config_t> GpioBase::configs{};
    std::mutex GpioBase::mutex{};



    bool GpioBase::initialise_pin(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr, void *isr_args)
    {
        assert(GPIO_IS_VALID_GPIO(pin));
        std::scoped_lock _(mutex);
        assert(used_pins.find(pin) == used_pins.end());
        assert(configs.find(pin) == configs.end());
        const auto [piniter, pinsuccess] = used_pins.insert(pin);
        assert(pinsuccess);
        assert(not config_is_empty(config));
        const auto [configiter, configsuccess] = configs.insert({pin, config});
        assert(configsuccess);

        ESP_LOGI(TAG, "Registered GPIO[%d]", pin);

        gpio_config(&config);

        if (isr)
        {
            isr_queue = queue::make_sharablequeue<IsrRet>();
            assert(isr_queue);
            this->isr_args.queue = isr_queue;

            auto installsuccess = gpio_install_isr_service(0);
            assert(ESP_OK == installsuccess || ESP_ERR_INVALID_STATE == installsuccess); // NOTE: Installed or already installed
            ESP_ERROR_CHECK(gpio_isr_handler_add(pin, isr, &this->isr_args));
        }

        return true;
    }

    bool GpioBase::deinitilise_pin(gpio_num_t pin)
    {
        std::scoped_lock _(mutex);

        if (isr)
            gpio_isr_handler_remove(pin);

        gpio_reset_pin(pin);

        assert(used_pins.find(pin) != used_pins.end());
        assert(configs.find(pin) != configs.end());
        const auto configerased = configs.erase(pin);
        assert(configerased);
        const auto pinerased = used_pins.erase(pin);
        assert(pinerased);

        ESP_LOGI(TAG, "Unregistered GPIO[%d]", pin);

        return true;
    }

    bool GpioBase::in_use(gpio_num_t pin)
    {
        std::scoped_lock _(mutex);
        const bool is_used = used_pins.find(pin) != used_pins.end();
        const bool is_configured = configs.find(pin) != configs.end();
        assert(is_used == is_configured);
        return is_used;
    }

    std::optional<gpio_config_t> GpioBase::find_config(gpio_num_t pin)
    {
        if (not in_use(pin))
            return {};

        std::scoped_lock _(mutex);
        return {configs.find(pin)->second};
    }

} // namespace gpio