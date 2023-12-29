#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#include <vector>

#include "smartconfig.hpp"
#include "wifi.hpp"
#include "wrappers/nvs.hpp"
#include "wrappers/queue.hpp"
#include "wrappers/task.hpp"

static const char *TAG = "main";

#define ESP_INTR_FLAG_DEFAULT 0

static auto gpio_evt_queue = queue::make_queue<gpio_num_t>(10);

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    gpio_num_t gpio_num = *reinterpret_cast<const gpio_num_t *>(arg);
    auto success = gpio_evt_queue->send_from_isr(gpio_num);
    ESP_DRAM_LOGD("gpio_isr_handler", "GPIO[%d] ISR: %s", gpio_num, success ? "success" : "failure");
}

static void gpio_task_example(void *arg)
{
    using SmartConfig = sc::SmartConfig::Shared;

    std::vector<SmartConfig> instances;
    wifi::Wifi::nvs_ssid_erase();
    wifi::Wifi::nvs_password_erase();
    auto wifiobj{wifi::Wifi::get_shared()};

    while (true)
    {
        if (const auto result = gpio_evt_queue->receive())
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
    }
}

[[noreturn]] static void gpio_task(void *parm)
{
    ESP_LOGI(TAG, "GPIO task started");

    static constexpr auto pin = GPIO_NUM_34;
    static constexpr auto pinmask = 1ULL << pin;

    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = pinmask;
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;

    gpio_config(&io_conf);

    // start gpio task
    auto gpio_task = task::make_task(gpio_task_example, "gpio_task_example", 4096, nullptr, 10);

    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(pin, gpio_isr_handler, const_cast<gpio_num_t *>(&pin));

    while (true)
        vTaskDelay(portMAX_DELAY);
}

int main()
{
    auto gpio_task_handle = task::make_task(gpio_task, "gpio_task", 4096, nullptr, 3);

    vTaskDelay(portMAX_DELAY);
    return 0;
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs::initialise_nvs());
    const int ret = main();
    ESP_LOGE(TAG, "main() returned %d", ret);
}
