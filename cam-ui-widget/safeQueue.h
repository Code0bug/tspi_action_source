#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

template <typename T>
class SafeQueue {
public:
    explicit SafeQueue(size_t max_size = 0) : max_size_(max_size) {}
    ~SafeQueue() = default;

    // 禁止拷贝
    SafeQueue(const SafeQueue&) = delete;
    SafeQueue& operator=(const SafeQueue&) = delete;

    // 推入队列，如果超过容量，删除队头
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            queue_.pop(); // 删除队头
        }
        queue_.push(item);
        cond_.notify_one();
    }

    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            queue_.pop(); // 删除队头
        }
        queue_.push(std::move(item));
        cond_.notify_one();
    }

    // 阻塞弹出队列元素
    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // 尝试弹出队列元素，超时返回 false
    bool pop(T& item, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                            [this]{ return !queue_.empty(); }))
            return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 非阻塞尝试弹出
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if(queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 获取队列大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    // 判断队列是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    // 清空队列
    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<T> empty_q;
        std::swap(queue_, empty_q);
    }

    // 设置最大容量（0 表示无限制）
    void set_max_size(size_t max_size) {
        std::lock_guard<std::mutex> lock(mtx_);
        max_size_ = max_size;
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable cond_;
    std::queue<T> queue_;
    size_t max_size_ = 0; // 0 表示无限容量
};




#endif // SAFEQUEUE_H
