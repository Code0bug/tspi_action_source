#include <camera_hal.h>

int main()
{
    auto volatile init = &camera_init;
    auto volatile get_frame = &camera_get_frame;
    auto volatile release_frame = &camera_release_frame;
    return init && get_frame && release_frame ? 0 : 1;
}
