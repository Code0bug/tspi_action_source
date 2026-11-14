#ifndef MEDIA_SERVICE_MEDIA_MANAGER_H
#define MEDIA_SERVICE_MEDIA_MANAGER_H

#include <atomic>

enum class CamStatus {
    Preview,
    Photo,
    Record,
};

class MediaManager {
public:
    MediaManager() = default;
    ~MediaManager() = default;

    MediaManager(const MediaManager &) = delete;
    MediaManager &operator=(const MediaManager &) = delete;

    bool statusUpdate(CamStatus next);
    CamStatus statusGet() const;

private:
    std::atomic<CamStatus> status_{CamStatus::Preview};
};

#endif
