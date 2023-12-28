#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <memory>

namespace queue
{

    template <class Item>
    struct QueueHandle
    {
        using pointer = QueueHandle_t;
        using element = Item;

        [[nodiscard]] QueueHandle_t get() { return freertoshandle; }

        QueueHandle(QueueHandle_t handle) : freertoshandle{handle} {}
        ~QueueHandle()
        {
            if (freertoshandle)
            {
                ESP_LOGI("QueueHandle", "Deleting queue");
                vQueueDelete(freertoshandle);
            }
        }

    private:
        QueueHandle_t freertoshandle;
    };

    template <class Item>
    using Queue = std::shared_ptr<QueueHandle<Item>>;

    template <class Item>
    [[nodiscard]] Queue<Item> make_queue(size_t nitems)
    {
        return Queue<Item>{std::make_shared<QueueHandle<Item>>(xQueueCreate(nitems, sizeof(Item)))};
    }

} // namespace queue