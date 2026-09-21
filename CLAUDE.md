# 项目开发说明

当前是三层、三个进程的构建骨架，不具备旧版相机功能。

- 当前目录、依赖、编译和安装：根目录 README.md。
- 完整规划：doc/refactoring.md；历史资料：doc/history/。
- UI：camera-ui/；媒体业务：media-service/；C ABI：hal/include/。
- HAL 只由媒体服务链接，UI 通过未来 IPC 与服务交互。
- monitor/ 尚无进程管理逻辑，ipc/ 尚无通信实现。
- 外部依赖从匹配 SDK sysroot 获取，不使用旧 SDK 副本或 Rockit。
- 设置 BUILDROOT_HOST_DIR 后执行 ./tools/build.sh。
- out/ 为忽略的产物目录，platform/ 为保留的板级资源。
- 本轮验证交叉编译与链接，尚无板端功能验证。
