#ifndef MEDIA_SERVICE_SAFE_QUEUE_H
#define MEDIA_SERVICE_SAFE_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template <typename T>
class SafeQueue {
public:
    SafeQueue() = default;
    ~SafeQueue() = default;

    SafeQueue(const SafeQueue &) = delete;
    SafeQueue &operator=(const SafeQueue &) = delete;

    void push(const T &item)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(item);
        cond_.notify_one();
    }

    void push(T &&item)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(std::move(item));
        cond_.notify_one();
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

private:
    std::mutex mtx_;
    std::condition_variable cond_;
    std::queue<T> queue_;
};

#endif
