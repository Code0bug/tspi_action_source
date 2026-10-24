#define _GNU_SOURCE
#include "key_hal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct key_hal {
    int fd;
};

key_hal_t *key_hal_open(const char *device)
{
    if (!device)
        return NULL;
    int fd = open(device, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        perror("key_hal_open");
        return NULL;
    }
    key_hal_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        close(fd);
        return NULL;
    }
    ctx->fd = fd;
    return ctx;
}

key_hal_t *key_hal_open_by_name(const char *name)
{
    if (!name)
        return NULL;
    DIR *dir = opendir("/sys/class/input");
    if (!dir)
        return NULL;

    key_hal_t *ctx = NULL;
    struct dirent *de;
    while (!ctx && (de = readdir(dir))) {
        if (strncmp(de->d_name, "event", 5) != 0)
            continue;
        char path[320], buf[128] = {0};
        snprintf(path, sizeof(path), "/sys/class/input/%s/device/name", de->d_name);
        FILE *f = fopen(path, "r");
        if (!f)
            continue;
        if (fgets(buf, sizeof(buf), f) && strstr(buf, name)) {
            snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
            ctx = key_hal_open(path);
        }
        fclose(f);
    }
    closedir(dir);
    return ctx;
}

int key_hal_get_fd(const key_hal_t *ctx)
{
    return ctx ? ctx->fd : -1;
}

int key_hal_read_event(key_hal_t *ctx, key_hal_event_t *event)
{
    if (!ctx || !event)
        return -EINVAL;

    struct input_event ev;
    for (;;) {
        ssize_t n = read(ctx->fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN)
                return 0;
            return -errno;
        }
        if (n != (ssize_t)sizeof(ev))
            return -EIO;
        if (ev.type != EV_KEY)
            continue;
        if (ev.value < 0 || ev.value > KEY_HAL_REPEAT)
            continue;
        event->code = ev.code;
        event->action = (key_hal_action_t)ev.value;
        event->timestamp_us = (int64_t)ev.time.tv_sec * 1000000 + ev.time.tv_usec;
        return 1;
    }
}

int key_hal_wait_event(key_hal_t *ctx, key_hal_event_t *event, int timeout_ms)
{
    if (!ctx || !event)
        return -EINVAL;
    for (;;) {
        int ret = key_hal_read_event(ctx, event);
        if (ret != 0)
            return ret;
        struct pollfd pfd = { .fd = ctx->fd, .events = POLLIN };
        ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (ret == 0)
            return 0;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            return -EIO;
    }
}

void key_hal_close(key_hal_t *ctx)
{
    if (!ctx)
        return;
    close(ctx->fd);
    free(ctx);
}
