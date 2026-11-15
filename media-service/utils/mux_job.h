#ifndef MEDIA_SERVICE_MUX_JOB_H
#define MEDIA_SERVICE_MUX_JOB_H

#include <camera_hal.h>

#include <cstdint>
#include <string>
#include <vector>

enum class MuxJobType {
    Open,
    Video,
    Audio,
    Close,
    Stop,
};

struct MuxJob {
    MuxJobType type;
    camera_frame_t video;
    std::vector<uint8_t> pcm;
    uint64_t pts;
    std::string path;
};

#endif
