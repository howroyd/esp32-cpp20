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

    inline void log_stack(const char *tag, auto taskstacksize)
    {
        const auto stackused = static_cast<double>(taskstacksize) - uxTaskGetStackHighWaterMark(nullptr);
        const auto stackusedpercentage = static_cast<double>(stackused) / taskstacksize * 100.0;
        ESP_LOGE(tag, "Stack used: %.0f words (%.2f%%)", stackused, stackusedpercentage);
    }

    [[nodiscard]] static inline constexpr TickType_t to_ticks(std::chrono::milliseconds ms)
    {
        if (ms == std::chrono::milliseconds::max())
            return portMAX_DELAY;
        else
            return pdMS_TO_TICKS(ms.count());
    }

} // namespace task