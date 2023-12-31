#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "task.hpp"

namespace task
{

    void Deleter::operator()(TaskHandle_t freertoshandle) const
    {
        if (freertoshandle)
        {
            ESP_LOGD("TaskDeleter", "Deleting task");
            vTaskDelete(freertoshandle);
        }
    }

    static constexpr Deleter deleter;

    Task make_task_from_taskhandle(TaskHandle_t freertoshandle)
    {
        return Task(freertoshandle, deleter);
    }

    Task make_task(TaskFunction_t fn, const char *taskname, uint32_t taskstacksize, void *args, UBaseType_t taskpriority)
    {
        TaskHandle_t freertoshandle{nullptr};
        const auto success = xTaskCreate(fn, taskname, taskstacksize, args, taskpriority, &freertoshandle);
        ESP_LOGI("Task", "Task %s created: %s", taskname, success ? "success" : "failure");

        if (!success)
        {
            return nullptr;
        }
        return make_task_from_taskhandle(freertoshandle);
    }

    void log_stack(const char *tag, uint32_t taskstacksize)
    {
        const auto stackused = static_cast<double>(taskstacksize) - uxTaskGetStackHighWaterMark(nullptr);
        const auto stackusedpercentage = static_cast<double>(stackused) / taskstacksize * 100.0;
        if (stackusedpercentage > 90.0)
            ESP_LOGW(tag, "Stack used: %.0f words (%.2f%%)", stackused, stackusedpercentage);
        else
            ESP_LOGI(tag, "Stack used: %.0f words (%.2f%%)", stackused, stackusedpercentage);
    }

} // namespace task