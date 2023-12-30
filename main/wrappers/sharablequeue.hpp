#pragma once

#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <queue>

#include "semphr.hpp"
#include "task.hpp"

namespace queue
{

    template <class T, class Container = std::queue<T>>
    class SharableQueue
    {
        Container queue{};
        mutable std::recursive_mutex mutex{};
        mutable semphr::Semaphore semaphore{semphr::make_semaphore()};

    public:
        struct Ret
        {
            bool success;
            T item{};
            operator bool() const { return success; }
        };

        [[nodiscard]] auto empty() const
        {
            std::scoped_lock _{mutex};
            return queue.empty();
        }

        [[nodiscard]] auto size() const
        {
            std::scoped_lock _{mutex};
            return queue.size();
        }

        void push(const T &item)
        {
            std::scoped_lock _{mutex};
            queue.push(item);
            semphr::give(semaphore);
        }

        void push(T &&item)
        {
            std::scoped_lock _{mutex};
            queue.push(std::move(item));
            semphr::give(semaphore);
        }

        template <class... Args>
        void emplace(Args &&...args)
        {
            std::scoped_lock _{mutex};
            queue.emplace(std::forward<Args>(args)...);
            semphr::give(semaphore);
        }

        IRAM_ATTR void push_from_isr(const T &item)
        {
            queue.push(item);
            semphr::give_from_isr(semaphore);
        }

        IRAM_ATTR void push_from_isr(T &&item)
        {
            queue.push(std::move(item));
            semphr::give_from_isr(semaphore);
        }

        template <class... Args>
        IRAM_ATTR void emplace_from_isr(Args &&...args)
        {
            queue.emplace(std::forward<Args>(args)...);
            semphr::give_from_isr(semaphore);
        }

        [[nodiscard]] Ret pop()
        {
            std::scoped_lock _{mutex};

            if (empty())
                return {false};

            Ret ret{true, std::move(queue.front())};
            queue.pop();

            return ret;
        }

        [[nodiscard]] Ret pop_wait(std::chrono::milliseconds wait_for = std::chrono::milliseconds::max())
        {
            std::scoped_lock _{mutex};

            if (auto waiting = pop()) // NOTE: Check is there is something already in the queue waiting to be popped
                return waiting;

            if (not semphr::take(semaphore, wait_for)) // NOTE: Wait for something to be pushed to the queue
                return {false};

            Ret ret{true, std::move(queue.front())};

            queue.pop();

            return ret;
        }
    };

    template <class T, class Container = std::queue<T>>
    [[nodiscard]] auto make_sharablequeue()
    {
        return std::make_shared<SharableQueue<T, Container>>();
    }

} // namespace queue