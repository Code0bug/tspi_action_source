#ifndef __KEY_HAL_H__
#define __KEY_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    KEY_HAL_RELEASED = 0,
    KEY_HAL_PRESSED = 1,
    KEY_HAL_REPEAT = 2,
} key_hal_action_t;

typedef struct {
    uint16_t code;          /* linux/input-event-codes.h 中的 KEY_* */
    key_hal_action_t action;
    int64_t timestamp_us;
} key_hal_event_t;

typedef struct key_hal key_hal_t;

/* device 为 evdev 节点（如 /dev/input/event1）。 */
key_hal_t *key_hal_open(const char *device);

/* 按设备名（/sys/class/input/eventN/device/name）查找并打开，匹配子串。 */
key_hal_t *key_hal_open_by_name(const char *name);

/* 返回可用于 poll/epoll 的 fd，失败返回 -1。 */
int key_hal_get_fd(const key_hal_t *ctx);

/* 非阻塞读取一个按键事件。返回 1=有事件，0=暂无，<0=错误。 */
int key_hal_read_event(key_hal_t *ctx, key_hal_event_t *event);

/* 等待事件，timeout_ms<0 表示一直等待。返回值同上。 */
int key_hal_wait_event(key_hal_t *ctx, key_hal_event_t *event, int timeout_ms);

void key_hal_close(key_hal_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
