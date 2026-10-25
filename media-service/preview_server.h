#ifndef TSPI_PREVIEW_SERVER_H
#define TSPI_PREVIEW_SERVER_H

#include <camera_hal.h>
#include <preview_protocol.h>
#include <array>
#include <string>

/* Capture-thread owned. No operation waits for the UI or for a free slot. */
class PreviewServer {
public:
    ~PreviewServer();
    bool start();
    void pump();
    void render(const camera_t &camera, const camera_frame_t &frame);
private:
    void disconnect();
    bool initialize(uint32_t width, uint32_t height);
    int listener_ = -1, client_ = -1, lock_ = -1;
    std::string path_;
    std::array<int, 3> fds_{{-1, -1, -1}};
    std::array<bool, 3> busy_{{false, false, false}};
    std::array<uint64_t, 3> sequences_{{0, 0, 0}};
    preview_packet description_{};
    uint64_t generation_ = 0, sequence_ = 0;
};
#endif
