#define _GNU_SOURCE
#include "imu_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define IMU_RAW_WORDS 7
#define IMU_SIGNAL (SIGRTMIN + 1)

struct imu_hal {
    int fd;
    int evfd;
    float accel_scale;
    float gyro_scale;

    pthread_t thread;
    int thread_started;
    atomic_int stop;

    pthread_mutex_t lock;
    imu_sample_t *queue;
    unsigned int depth;
    unsigned int head;
    unsigned int count;
    uint64_t seq;
    atomic_uint_fast64_t dropped;
};

static int64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void push_sample(imu_hal_t *ctx, const int16_t raw[IMU_RAW_WORDS], int64_t ts)
{
    imu_sample_t s;
    s.timestamp_us = ts;
    for (int i = 0; i < 3; i++) {
        s.accel_g[i] = raw[i] * ctx->accel_scale;
        s.gyro_dps[i] = raw[3 + i] * ctx->gyro_scale;
    }
    s.temp_c = raw[6] / 132.48f + 25.0f;

    pthread_mutex_lock(&ctx->lock);
    s.seq = ctx->seq++;
    if (ctx->count == ctx->depth) { /* 丢最旧样本 */
        ctx->head = (ctx->head + 1) % ctx->depth;
        ctx->count--;
        atomic_fetch_add(&ctx->dropped, 1);
    }
    ctx->queue[(ctx->head + ctx->count) % ctx->depth] = s;
    ctx->count++;
    pthread_mutex_unlock(&ctx->lock);

    uint64_t one = 1;
    if (write(ctx->evfd, &one, sizeof(one)) < 0) { /* eventfd 计数溢出时忽略 */ }
}

static void *imu_thread(void *arg)
{
    imu_hal_t *ctx = arg;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, IMU_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    /* 只把异步通知定向到本线程 */
    struct f_owner_ex owner = { .type = F_OWNER_TID, .pid = (pid_t)syscall(SYS_gettid) };
    if (fcntl(ctx->fd, F_SETOWN_EX, &owner) < 0 ||
        fcntl(ctx->fd, F_SETSIG, IMU_SIGNAL) < 0) {
        perror("imu_hal: set owner/sig");
        return NULL;
    }
    int flags = fcntl(ctx->fd, F_GETFL);
    fcntl(ctx->fd, F_SETFL, flags | O_ASYNC);

    while (!atomic_load(&ctx->stop)) {
        struct timespec to = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
        siginfo_t info;
        int sig = sigtimedwait(&set, &info, &to);
        if (sig < 0)
            continue; /* 超时或被打断，检查 stop */
        int64_t ts = now_us();

        /* 驱动的 read 成功时拷贝 14 字节但返回 0，故以缓冲区填充为准 */
        int16_t raw[IMU_RAW_WORDS] = {0};
        ssize_t n = read(ctx->fd, raw, sizeof(raw));
        if (n < 0)
            continue;
        push_sample(ctx, raw, ts);
    }
    return NULL;
}

imu_hal_t *imu_init(const imu_config_t *config)
{
    const char *dev = (config && config->device) ? config->device : IMU_HAL_DEFAULT_DEVICE;
    float arange = (config && config->accel_range_g > 0) ? config->accel_range_g : 4.0f;
    float grange = (config && config->gyro_range_dps > 0) ? config->gyro_range_dps : 1000.0f;
    unsigned int depth = (config && config->queue_depth) ? config->queue_depth : 512;

    imu_hal_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->fd = -1;
    ctx->evfd = -1;
    ctx->depth = depth;
    ctx->accel_scale = arange / 32768.0f;
    ctx->gyro_scale = grange / 32768.0f;
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->queue = calloc(depth, sizeof(*ctx->queue));
    ctx->fd = open(dev, O_RDWR | O_CLOEXEC);
    ctx->evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC | EFD_SEMAPHORE);
    if (!ctx->queue || ctx->fd < 0 || ctx->evfd < 0) {
        perror("imu_init");
        goto fail;
    }

    /* 创建线程前阻塞该信号，避免其它线程默认处置导致进程被终止 */
    sigset_t set, old;
    sigemptyset(&set);
    sigaddset(&set, IMU_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &set, &old);
    int ret = pthread_create(&ctx->thread, NULL, imu_thread, ctx);
    pthread_sigmask(SIG_SETMASK, &old, NULL);
    if (ret != 0)
        goto fail;
    ctx->thread_started = 1;
    return ctx;

fail:
    imu_deinit(ctx);
    return NULL;
}

int imu_get_fd(const imu_hal_t *ctx)
{
    return ctx ? ctx->evfd : -1;
}

int imu_read_sample(imu_hal_t *ctx, imu_sample_t *sample)
{
    if (!ctx || !sample)
        return -EINVAL;

    int ret = 0;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->count > 0) {
        *sample = ctx->queue[ctx->head];
        ctx->head = (ctx->head + 1) % ctx->depth;
        ctx->count--;
        ret = 1;
    }
    pthread_mutex_unlock(&ctx->lock);

    if (ret == 1) { /* 消耗一个计数，保持 fd 可读性与队列一致 */
        uint64_t v;
        if (read(ctx->evfd, &v, sizeof(v)) < 0) { /* 计数可能已被清空 */ }
    }
    return ret;
}

uint64_t imu_get_dropped(const imu_hal_t *ctx)
{
    return ctx ? atomic_load(&((imu_hal_t *)ctx)->dropped) : 0;
}

void imu_deinit(imu_hal_t *ctx)
{
    if (!ctx)
        return;
    atomic_store(&ctx->stop, 1);
    if (ctx->thread_started)
        pthread_join(ctx->thread, NULL);
    if (ctx->fd >= 0)
        close(ctx->fd);
    if (ctx->evfd >= 0)
        close(ctx->evfd);
    pthread_mutex_destroy(&ctx->lock);
    free(ctx->queue);
    free(ctx);
}
