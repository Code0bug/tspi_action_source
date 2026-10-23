#include <v4l2_camera.h>

int main()
{
    auto volatile init = &v4l2_camera_init;
    auto volatile get_frame = &v4l2_camera_get_frame;
    auto volatile release_frame = &v4l2_camera_release_frame;
    return init && get_frame && release_frame ? 0 : 1;
}
