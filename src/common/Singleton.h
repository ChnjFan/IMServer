//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_SINGLETON_H
#define IMSERVER_SINGLETON_H

#include <memory>
#include <mutex>

template<typename T>
class Singleton {
public:
    // 单例模式删除拷贝构造
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static std::shared_ptr<T> getInstance() {
        static std::once_flag onceToInit;
        std::call_once(onceToInit, [&]() {
            // make_shared 无法调用私有构造函数，单例模式都将构造函数放在 private 中
            instance_ = std::shared_ptr<T>(new T);
        });
        return instance_;
    }
protected:
    Singleton() = default;

    static std::shared_ptr<T> instance_;
};

template<typename T>
std::shared_ptr<T> Singleton<T>::instance_ = nullptr;

#endif //IMSERVER_SINGLETON_H