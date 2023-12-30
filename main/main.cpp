#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#include <chrono>
#include <memory>
#include <vector>

#include "smartconfig.hpp"
#include "wifi.hpp"
#include "wrappers/nvs.hpp"
#include "wrappers/queue.hpp"
#include "wrappers/sharablequeue.hpp"
#include "wrappers/task.hpp"

#define CLEAR_WIFI_NVS

static constexpr const char *TAG = "main";

IRAM_ATTR static void gpio_isr_handler(void *arg);

struct gpio_isr
{
    gpio_num_t pin;
    gpio_config_t config;
    std::weak_ptr<queue::SharableQueue<gpio_num_t>> queue{};

    static constexpr auto isr_handler = gpio_isr_handler;

    struct Deleter
    {
        void operator()(gpio_isr *args) const
        {
            gpio_isr_handler_remove(args->pin);
            delete args;
        }
    };
};

static void gpio_isr_handler(void *arg)
{
    auto args = *reinterpret_cast<gpio_isr *>(arg);
    auto queue = args.queue.lock();

    ESP_DRAM_LOGD("gpio_isr_handler", "GPIO[%d] ISR", args.pin);

    if (queue)
        queue->push_from_isr(args.pin);
    else
        ESP_DRAM_LOGE("gpio_isr_handler", "Queue is null");
}

static void gpio_main(void *arg)
{
    using namespace std::chrono_literals;
    using SmartConfig = sc::SmartConfig::Shared;

    ESP_LOGI(TAG, "GPIO main started");

    std::unique_ptr<gpio_isr, gpio_isr::Deleter> args{reinterpret_cast<gpio_isr *>(arg), gpio_isr::Deleter{}};
    assert(args);
    auto queue = queue::make_sharablequeue<gpio_num_t>();
    assert(queue);
    args->queue = queue;

    gpio_config(&args->config);

    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(args->pin, args->isr_handler, args.get());

#ifdef CLEAR_WIFI_NVS
    wifi::Wifi::nvs_ssid_erase();
    wifi::Wifi::nvs_password_erase();
#endif

    auto wifiobj{wifi::Wifi::get_shared()};
    std::vector<SmartConfig> instances;

    while (true)
    {
        // if (const auto result = gpio_evt_sharablequeue->pop_wait(1s))
        if (const auto result = queue->pop_wait())
        {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d", result.item, gpio_get_level(result.item));

            if (instances.size() >= 5)
            {
                ESP_LOGW(TAG, "Clearing instances");
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

[[noreturn]] static void gpio_task(void *parm)
{
    ESP_LOGI(TAG, "GPIO task started");

    static constexpr auto pin = GPIO_NUM_34;

    // install gpio isr service
    gpio_install_isr_service(0);

    // start gpio task
    auto gpioargs = new gpio_isr{pin, gpio_config_t{
                                          .pin_bit_mask = 1ULL << pin,
                                          .mode = GPIO_MODE_INPUT,
                                          .pull_up_en = GPIO_PULLUP_DISABLE,
                                          .pull_down_en = GPIO_PULLDOWN_ENABLE,
                                          .intr_type = GPIO_INTR_NEGEDGE}};
    auto gpio_task = task::make_task(gpio_main, "gpio_main", 4096, gpioargs, 10);
    if (not gpio_task)
    {
        ESP_LOGE(TAG, "Failed to create gpio_main task");
        delete gpioargs;
        abort();
    }

    task::delay_forever();
}

int main()
{
    auto gpio_task_handle = task::make_task(gpio_task, "gpio_task", 4096, nullptr, 3);

    task::delay_forever();

    return 0;
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs::initialise_nvs());
    const int ret = main();
    ESP_LOGE(TAG, "main() returned %d", ret);
}
