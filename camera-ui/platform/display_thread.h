#ifndef TSPI_DISPLAY_THREAD_H
#define TSPI_DISPLAY_THREAD_H

#include <QImage>
#include <QRegion>
#include <QSize>
#include <atomic>
#include <future>
#include <mutex>
#include <thread>

class DisplayThread {
public:
    ~DisplayThread();
    QSize start();
    void publish(const QImage &image, const QRegion &dirty);
private:
    void run(std::promise<QSize> ready);
    std::thread thread_;
    std::atomic<bool> stopping_{false};
    int wake_ = -1;
    std::mutex mutex_;
    QImage latest_;
    QRegion dirty_;
};
#endif
