#ifndef __IMU_HAL_H__
#define __IMU_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define IMU_HAL_DEFAULT_DEVICE "/dev/icm42688p"

typedef struct {
    uint64_t seq;          /* 单调递增序号，用于发现丢样 */
    int64_t timestamp_us;  /* CLOCK_MONOTONIC，用户态收到中断通知的时刻，非传感器硬件时间戳 */
    float accel_g[3];      /* x/y/z，单位 g */
    float gyro_dps[3];     /* x/y/z，单位 deg/s */
    float temp_c;          /* 单位 ℃ */
} imu_sample_t;

typedef struct {
    const char *device;       /* NULL 则使用 IMU_HAL_DEFAULT_DEVICE */
    float accel_range_g;      /* 须与驱动配置一致，<=0 取 4 */
    float gyro_range_dps;     /* 须与驱动配置一致，<=0 取 1000 */
    unsigned int queue_depth; /* 内部队列样本数，0 取 512 */
} imu_config_t;

typedef struct imu_hal imu_hal_t;

/* 打开设备并启动内部采集线程（驱动产生 SIGIO 异步通知，由线程专门接收）。 */
imu_hal_t *imu_init(const imu_config_t *config);

/* 可用于 poll/epoll 的 fd，队列非空时可读。 */
int imu_get_fd(const imu_hal_t *ctx);

/* 非阻塞取一个样本。返回 1=有样本，0=暂无，<0=错误。 */
int imu_read_sample(imu_hal_t *ctx, imu_sample_t *sample);

/* 因队列满而丢弃的样本数累计。 */
uint64_t imu_get_dropped(const imu_hal_t *ctx);

void imu_deinit(imu_hal_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
