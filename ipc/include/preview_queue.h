#ifndef TSPI_PREVIEW_QUEUE_H
#define TSPI_PREVIEW_QUEUE_H

#include <array>
#include <cstdint>

/* Used only by the display thread. A successful ioctl never retires scanout;
 * complete() is called exclusively by the DRM page-flip handler. */
class PreviewQueue {
public:
    bool offer(int slot, uint64_t sequence, int &dropped) {
        dropped = -1;
        if (slot < 0 || slot >= 3 || owned_[slot]) return false;
        dropped = waiting_;
        if (dropped >= 0) owned_[dropped] = false;
        owned_[slot] = true;
        sequence_[slot] = sequence;
        waiting_ = slot;
        return true;
    }
    int next() const { return waiting_ >= 0 ? waiting_ : current_; }
    int waiting() const { return waiting_; }
    int current() const { return current_; }
    uint64_t sequence(int slot) const { return sequence_[slot]; }
    void submitted(int slot) {
        submitted_ = slot;
        if (slot == waiting_) waiting_ = -1;
    }
    int complete() {
        int retired = current_ != submitted_ ? current_ : -1;
        if (retired >= 0) owned_[retired] = false;
        current_ = submitted_;
        return retired;
    }
    void discardWaiting() {
        if (waiting_ >= 0) owned_[waiting_] = false;
        waiting_ = -1;
    }
private:
    int waiting_ = -1, current_ = -1, submitted_ = -1;
    std::array<bool, 3> owned_{{false, false, false}};
    std::array<uint64_t, 3> sequence_{{0, 0, 0}};
};
#endif
