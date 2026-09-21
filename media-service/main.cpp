#include <camera/hal.h>

int main()
{
    // Resolve a C symbol from the shared HAL to verify the C++ -> C ABI link.
    return camera_hal_abi_version() == CAMERA_HAL_ABI_VERSION ? 0 : 1;
}
