#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include "gpio.hpp"

namespace gpio
{

    std::unordered_set<gpio_num_t> GpioBase::used_pins{};
    std::unordered_map<gpio_num_t, gpio_config_t> GpioBase::configs{};
    std::mutex GpioBase::mutex{};

    GpioBase::GpioBase(gpio_num_t pin, gpio_config_t config, gpio_isr_t isr, void *isr_args) : pin{pin}, isr{isr}, isr_args{pin, config, {}, isr_args}
    {
        assert(GPIO_IS_VALID_GPIO(pin));
        std::scoped_lock _(mutex);
        assert(used_pins.find(pin) == used_pins.end());
        assert(configs.find(pin) == configs.end());
        const auto [piniter, pinsuccess] = used_pins.insert(pin);
        assert(pinsuccess);
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
    }

    GpioBase::~GpioBase()
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
    }

} // namespace gpio