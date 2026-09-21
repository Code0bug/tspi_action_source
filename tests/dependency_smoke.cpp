#include <camera/hal.h>
#include <QWidget>
#include <dbus/dbus.h>
#include <alsa/asoundlib.h>
#include <rockchip/rk_mpi.h>
#include <rga/RgaApi.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <linux/videodev2.h>
#include <sys/socket.h>
#include <cstdio>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

int main()
{
    // Taking volatile function addresses verifies link symbols without device I/O.
    auto volatile mpp_symbol = &mpp_create;
    auto volatile rga_symbol = &c_RkRgaInit;
    auto volatile egl_symbol = &eglGetDisplay;
    auto volatile gl_symbol = &glGetString;
    int major = 0, minor = 0, micro = 0;
    dbus_get_version(&major, &minor, &micro);
    std::printf("Qt=%s (%s), D-Bus=%d.%d.%d, ALSA=%s\n",
                qVersion(), QWidget::staticMetaObject.className(),
                major, minor, micro, snd_asoundlib_version());
    std::printf("FFmpeg=%u/%u/%u/%u, V4L2=%zu, SCM_RIGHTS=%d\n",
                avformat_version(), avcodec_version(), avutil_version(),
                swresample_version(), sizeof(v4l2_buffer), SCM_RIGHTS);
    return camera_hal_abi_version() == CAMERA_HAL_ABI_VERSION &&
           mpp_symbol && rga_symbol && egl_symbol && gl_symbol ? 0 : 1;
}
