#pragma once

//#include "esp_log.h"

#include <memory>
#include <mutex>

template <class T>
class Singleton
{
public:
    using Shared = std::shared_ptr<T>;
    using Weak = std::weak_ptr<T>;

    template <typename... Args>
    [[nodiscard]] static Shared get_shared(Args &&...args)
    {
        std::scoped_lock lock{mutex};

        if (instance.expired())
        {
            //ESP_LOGW("Singleton", "Getting shared of %s from a new", T::TAG);
            auto shared = create(std::forward<Args>(args)...);
            instance = shared;
            return shared;
        }
        //ESP_LOGW("Singleton", "Getting shared of %s from existing weak", T::TAG);
        return Shared{instance};
    }

    [[nodiscard]] static Weak &get_weak()
    {
        std::scoped_lock lock{mutex};

        //ESP_LOGW("Singleton", "Getting weak of %s", T::TAG);
        return instance;
    }

protected:
    template <typename... Args>
    [[nodiscard]] static Shared create(Args &&...args)
    {
        //ESP_LOGE("Singleton", "Creating instance of %s", T::TAG);
        return Shared{new T{std::forward<Args>(args)...}};
    }

    static Weak instance;
    static std::mutex mutex;
};

template <class T>
Singleton<T>::Weak Singleton<T>::instance;

template <class T>
std::mutex Singleton<T>::mutex;