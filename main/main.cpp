#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include <vector>

#include "smartconfig.hpp"
#include "wifi.hpp"
#include "wrappers/queue.hpp"
#include "wrappers/task.hpp"

static const char *TAG = "main";

#define ESP_INTR_FLAG_DEFAULT 0

static auto gpio_evt_queue = queue::make_queue<uint32_t>(10);

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue->get(), &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
    using SmartConfig = sc::SmartConfig::Shared;

    uint32_t io_num{};
    std::vector<SmartConfig> instances;
    auto wifiobj{wifi::Wifi::get_shared()};

    while (true)
    {
        if (xQueueReceive(gpio_evt_queue->get(), &io_num, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "GPIO[%lu] intr, val: %d", io_num, gpio_get_level((gpio_num_t)io_num));

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

    constexpr auto pin = GPIO_NUM_34;
    constexpr auto pinmask = 1ULL << pin;

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
    xTaskCreate(gpio_task_example, "gpio_task_example", 4096, nullptr, 10, NULL);

    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(pin, gpio_isr_handler, (void *)pin);

    while (true)
        vTaskDelay(portMAX_DELAY);
}

int main()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    auto gpio_task_handle = task::make_task(gpio_task, "gpio_task", 4096, nullptr, 3);

    vTaskDelay(portMAX_DELAY);
    return 0;
}

extern "C" void app_main(void)
{
    const int ret = main();
    ESP_LOGE(TAG, "main() returned %d", ret);
}
