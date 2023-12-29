#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <cstddef>
#include <memory>

namespace queue
{

    template <class Item>
    struct QueueHandle
    {
        using pointer = QueueHandle_t;
        using element = Item;

        std::size_t size() const { return uxQueueMessagesWaiting(freertoshandle); }
        std::size_t spaces() const { return uxQueueSpacesAvailable(freertoshandle); }
        bool empty() const { return 0 == size(); }
        bool full() const { return 0 == spaces(); }

        std::size_t size_from_isr() const { return uxQueueMessagesWaitingFromISR(freertoshandle); }
        bool empty_from_isr() const { return xQueueIsQueueEmptyFromISR(freertoshandle); }
        bool full_from_isr() const { return xQueueIsQueueFullFromISR(freertoshandle); }

        struct Success
        {
            bool success;
            bool higher_priority_task_woken = false;
            operator bool() const { return success; }
        };
        struct ItemReturn
        {
            Success success;
            Item item;
            operator bool() const { return bool(success); }
        };

        [[nodiscard]] ItemReturn receive(TickType_t ticks = portMAX_DELAY)
        {
            Item item{};
            return {{pdTRUE == xQueueReceive(freertoshandle, &item, ticks)}, item};
        }

        [[nodiscard]] ItemReturn receive_from_isr()
        {
            Item item{};
            BaseType_t higher_priority_task_woken = pdFALSE;
            return {{pdTRUE == xQueueReceiveFromISR(freertoshandle, &item, &higher_priority_task_woken), pdTRUE == higher_priority_task_woken}, item};
        }

        Success send(Item item, TickType_t ticks = portMAX_DELAY)
        {
            return {pdTRUE == xQueueSend(freertoshandle, &item, ticks)};
        }

        Success send_from_isr(Item &item)
        {
            BaseType_t higher_priority_task_woken = pdFALSE;
            return {pdTRUE == xQueueSendFromISR(freertoshandle, &item, &higher_priority_task_woken), pdTRUE == higher_priority_task_woken};
        }

        Success send_to_back(Item &item, TickType_t ticks = portMAX_DELAY)
        {
            return {pdTRUE == xQueueSendToBack(freertoshandle, &item, ticks)};
        }

        Success send_to_front(Item &item, TickType_t ticks = portMAX_DELAY)
        {
            return {pdTRUE == xQueueSendToFront(freertoshandle, &item, ticks)};
        }

        Success send_to_back_from_isr(Item &item, BaseType_t *pxHigherPriorityTaskWoken = nullptr)
        {
            BaseType_t higher_priority_task_woken = pdFALSE;
            return {pdTRUE == xQueueSendToBackFromISR(freertoshandle, &item, &higher_priority_task_woken), pdTRUE == higher_priority_task_woken};
        }

        Success send_to_front_from_isr(Item &item, BaseType_t *pxHigherPriorityTaskWoken = nullptr)
        {
            BaseType_t higher_priority_task_woken = pdFALSE;
            return {pdTRUE == xQueueSendToFrontFromISR(freertoshandle, &item, pxHigherPriorityTaskWoken)};
        }

        [[nodiscard]] ItemReturn peek(TickType_t ticks = portMAX_DELAY) const
        {
            Item item{};
            return {{pdTRUE == xQueuePeek(freertoshandle, &item, ticks)}, item};
        }

        [[nodiscard]] ItemReturn peek_from_isr() const
        {
            Item item{};
            BaseType_t higher_priority_task_woken = pdFALSE;
            return {{pdTRUE == xQueuePeekFromISR(freertoshandle, &item, &higher_priority_task_woken), pdTRUE == higher_priority_task_woken}, item};
        }

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