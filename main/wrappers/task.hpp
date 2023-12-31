#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <chrono>
#include <memory>

namespace task
{

    struct Deleter
    {
        using pointer = TaskHandle_t; // Note; Required for std::unique_ptr's static assert to work since TaskHandle_t is a typedef struct of doom

        void operator()(TaskHandle_t freertoshandle) const;
    };

    using Task = std::unique_ptr<TaskHandle_t, Deleter>;

    [[nodiscard]] Task make_task_from_taskhandle(TaskHandle_t freertoshandle);
    [[nodiscard]] Task make_task(TaskFunction_t fn, const char *taskname, uint32_t taskstacksize, void *args, UBaseType_t taskpriority);

    void log_stack(const char *tag, uint32_t taskstacksize);

    [[nodiscard, gnu::const]] static inline constexpr TickType_t to_ticks(std::chrono::milliseconds ms) noexcept
    {
        if (ms == std::chrono::milliseconds::max())
            return portMAX_DELAY;
        else
            return pdMS_TO_TICKS(ms.count());
    }

    static inline void delay(std::chrono::milliseconds ms)
    {
        vTaskDelay(to_ticks(ms));
    }

    static inline void delay_until(std::chrono::milliseconds ms)
    {
        static auto xLastWakeTime{xTaskGetTickCount()};
        xTaskDelayUntil(&xLastWakeTime, to_ticks(ms));
    }

    [[noreturn]] static inline void delay_forever()
    {
        vTaskDelay(portMAX_DELAY);
        __builtin_unreachable();
    }

} // namespace task