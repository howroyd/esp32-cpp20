#pragma once

#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <chrono>
#include <memory>

namespace semphr
{

    struct Deleter
    {
        using pointer = SemaphoreHandle_t;

        void operator()(SemaphoreHandle_t freertoshandle) const;
    };

    using Semaphore = std::unique_ptr<SemaphoreHandle_t, Deleter>;

    [[nodiscard]] Semaphore make_semaphore_from_handle(SemaphoreHandle_t freertoshandle);
    [[nodiscard]] Semaphore make_semaphore();
    [[nodiscard]] Semaphore make_counting_semaphore();

    [[nodiscard]] bool take(Semaphore &semaphore, std::chrono::milliseconds wait_time = std::chrono::milliseconds::max());
    bool give(Semaphore &semaphore);
    IRAM_ATTR bool give_from_isr(Semaphore &semaphore);

} // namespace semphr