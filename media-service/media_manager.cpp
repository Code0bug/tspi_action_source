#include "media_manager.h"

bool MediaManager::statusUpdate(CamStatus next)
{
    CamStatus cur = status_.load();

    if (cur == CamStatus::Preview &&
        (next == CamStatus::Photo || next == CamStatus::Record)) {
        status_.store(next);
        return true;
    }
    if (cur == CamStatus::Photo && next == CamStatus::Preview) {
        status_.store(next);
        return true;
    }
    if (cur == CamStatus::Record && next == CamStatus::Preview) {
        status_.store(next);
        return true;
    }
    return false;
}

CamStatus MediaManager::statusGet() const
{
    return status_.load();
}
