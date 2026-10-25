# 项目开发说明

当前有三个进程，已实现采集到 UI DRM plane 的预览链路，拍照与录像尚未实现。

- 当前目录、依赖、编译和安装：根目录 README.md。
- 预览架构与验证：doc/preview-display.md。
- UI：camera-ui/；媒体业务：media-service/；C ABI：hal/include/。
- HAL 由媒体服务链接；UI 平台插件自己管理 DRM，视频通过 socket 传 dma-buf。
- monitor/ 独立监管两个进程；ipc/ 包含预览通道及 D-Bus 接口定义。
- 外部依赖从匹配 SDK sysroot 获取，不使用旧 SDK 副本或 Rockit。
- 设置 BUILDROOT_HOST_DIR 后执行 ./tools/build.sh。
- out/ 为忽略的产物目录；camera-ui/platform/ 是 Qt tspi 平台插件。
- 主机测试见 tests/preview/；交叉编译通过不等于板端功能已验证。
