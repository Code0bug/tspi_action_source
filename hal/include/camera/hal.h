#ifndef CAMERA_HAL_H
#define CAMERA_HAL_H

#include <stdint.h>

#define CAMERA_HAL_ABI_VERSION 1u
#define CAMERA_HAL_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

/* Skeleton ABI marker only; this does not report hardware readiness. */
CAMERA_HAL_API uint32_t camera_hal_abi_version(void);

#ifdef __cplusplus
}
#endif

#endif
