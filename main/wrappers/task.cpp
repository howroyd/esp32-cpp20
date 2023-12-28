#include "esp_log.h"

#include "task.hpp"

namespace task
{

    void Deleter::operator()(TaskHandle_t freertoshandle) const
    {
        if (freertoshandle)
        {
            ESP_LOGI("TaskDeleter", "Deleting task");
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
        xTaskCreate(fn, taskname, taskstacksize, args, taskpriority, &freertoshandle);
        return make_task_from_taskhandle(freertoshandle);
    }

} // namespace task