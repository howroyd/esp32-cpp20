#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "semphr.hpp"
#include "task.hpp"

namespace semphr
{

    void Deleter::operator()(SemaphoreHandle_t freertoshandle) const
    {
        if (freertoshandle)
        {
            ESP_LOGD("SemaphoreDeleter", "Deleting semaphore");
            vSemaphoreDelete(freertoshandle);
        }
    }

    static constexpr Deleter deleter;

    Semaphore make_semaphore_from_handle(SemaphoreHandle_t freertoshandle)
    {
        return {freertoshandle, deleter};
    }

    Semaphore make_semaphore()
    {
        return make_semaphore_from_handle(xSemaphoreCreateBinary());
    }

    Semaphore make_counting_semaphore(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount = 0)
    {
        return make_semaphore_from_handle(xSemaphoreCreateCounting(uxMaxCount, uxInitialCount));
    }

    bool take(Semaphore &semaphore, std::chrono::milliseconds wait_time)
    {
        if (semaphore)
            return pdTRUE == xSemaphoreTake(semaphore.get(), task::to_ticks(wait_time));
        else
            return false;
    }

    bool give(Semaphore &semaphore)
    {
        if (semaphore)
            return pdTRUE == xSemaphoreGive(semaphore.get());
        else
            return false;
    }

    bool give_from_isr(Semaphore &semaphore)
    {
        BaseType_t higher_priority_task_woken = pdFALSE;
        auto ret = pdTRUE == xSemaphoreGiveFromISR(semaphore.get(), &higher_priority_task_woken);
        if (pdTRUE == higher_priority_task_woken)
            portYIELD_FROM_ISR();
        return ret;
    }

} // namespace semphr