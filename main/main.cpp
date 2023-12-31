#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include "gpio.hpp"
#include "smartconfig.hpp"
#include "wifi.hpp"
#include "wrappers/nvs.hpp"
#include "wrappers/task.hpp"

#include <memory>
#include <utility>
#include <vector>

// #define CLEAR_WIFI_NVS
#define KEEP_WIFI_ALIVE
#define PIN (gpio_num_t::GPIO_NUM_34)

static constexpr const char *TAG = "main";

static void gpio_isr_handler(void *arg)
{
    auto args = *reinterpret_cast<gpio::IsrArgs *>(arg);
    auto queue = args.queue.lock();

    ESP_DRAM_LOGD("gpio_isr_handler", "GPIO[%d] ISR", args.pin);

    if (queue)
        queue->push_from_isr({args.pin, args.config.intr_type});
    else
        ESP_DRAM_LOGE("gpio_isr_handler", "Queue is null");
}

[[nodiscard]] std::pair<std::unique_ptr<gpio::Gpio>, std::shared_ptr<gpio::IsrQueue>> unpack_gpio_task_arg(void *arg)
{
    auto args = reinterpret_cast<gpio::Gpio *>(arg);
    assert(args);
    auto queue = args->get_queue();
    assert(queue);
    return {std::unique_ptr<gpio::Gpio>{args}, queue};
}

[[noreturn]] static void gpio_main(void *arg)
{
    using SmartConfig = sc::SmartConfig::Shared;

    ESP_LOGI(TAG, "GPIO main started");

    auto [gpio, queue] = unpack_gpio_task_arg(arg);

#ifdef CLEAR_WIFI_NVS
    ESP_LOGE(TAG, "CLEAR_WIFI_NVS is enabled");
    wifi::Wifi::clear_nvs_on_construction = true;
#endif

#ifdef KEEP_WIFI_ALIVE
    ESP_LOGE(TAG, "KEEP_WIFI_ALIVE is enabled");
    auto wifiobj{wifi::Wifi::get_shared()};
#endif

    std::vector<SmartConfig> instances;

    while (true)
    {
        if (const auto result = queue->pop_wait())
        {
            const auto &item = result.item;
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d, state: %s", item.pin, gpio_get_level(item.pin), gpio::int_type_to_string(item.state).c_str());

            if (instances.size() >= 5)
            {
                ESP_LOGW(TAG, "Clearing instances and wiping WiFi NVS");
                auto _wifi = instances.front()->get_wifi();
                _wifi->nvs_erase();
                _wifi->disconnect();
                instances.clear();
            }
            else
            {
                ESP_LOGI(TAG, "Pushing back instance");
                instances.push_back(sc::SmartConfig::get_shared());
            }
        }
        else
            ESP_LOGI(TAG, "Waiting for interrupt");
    }
}

int main()
{
    ESP_LOGD(TAG, "C++ entrypoint");

    static_assert(GPIO_IS_VALID_GPIO(PIN), "Invalid GPIO pin");

    auto gpioargs = new gpio::Gpio{PIN, gpio_config_t{.pin_bit_mask = 1ULL << PIN, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_ENABLE, .intr_type = GPIO_INTR_NEGEDGE}, gpio_isr_handler};
    auto gpio_task = task::make_task(gpio_main, "gpio_main", 4096, gpioargs, 10);

    if (not gpio_task)
    {
        ESP_LOGE(TAG, "Failed to create gpio_main task");
        delete gpioargs;
        abort();
    }

    task::delay_forever();
}

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_ERROR_CHECK(nvs::initialise_nvs());

    main();
}
