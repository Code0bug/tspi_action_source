# TSPI-ACTION

## 项目介绍

这是一个很有意思的开源项目，它叫做 **TSPI-ACTION**，是一款基于泰山派开发的高清防抖运动相机。项目从硬件设计到软件开发均由本人独自完成，并已将相关软硬件资料全部开源，你可以在下方的 **开源链接** 中找到完整内容。

本目录主要包含相机的**应用层软件**，由媒体服务进程（`media-service`）和 UI 进程（`camera-ui`）两部分组成。项目涉及 **V4L2、ALSA、Socket、DRM、FFmpeg、Qt、D-Bus** 等多方面的技术栈。目前，相机已实现**实时预览、拍照、录像、照片浏览和视频回放**等基本功能，并支持在录像过程中同步记录 IMU 数据，为后续的视频防抖处理提供数据支持。

**硬件平台**

- Rockchip RK3566 SoC
- OV8858 摄像头（4-lane MIPI，搭配 DW9714 实现自动对焦）
- 2.3 英寸 MIPI LCD 触摸屏
- ICM42688P IMU（SPI 接口）

**开源链接**

- 硬件开源地址：[泰山派高清防抖运动相机（Tspi Action） - 立创开源硬件平台](https://gitee.com/link?target=https%3A%2F%2Foshwhub.com%2Flzp21%2Fhd-smart-motion-camera)
- `tspi_action_bsp`：[Gitee](https://gitee.com/lzp1733149755/tspi_action_bsp)、[GitHub](https://github.com/Code0bug/tspi_action_bsp)
- 演示视频：[https://www.bilibili.com/video/BV1Qgq6BtEfx/?spm_id_from=333.1387.0.0](https://gitee.com/link?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1Qgq6BtEfx%2F%3Fspm_id_from%3D333.1387.0.0)
- ov8858驱动适配：[泰山派移植OV8858驱动并添加自动聚焦功能_泰山派ov8858-CSDN博客](https://gitee.com/link?target=https%3A%2F%2Fblog.csdn.net%2Fqq_53686293%2Farticle%2Fdetails%2F153208692%3Fspm%3D1001.2014.3001.5501)
- 2.3寸屏幕驱动适配：[泰山派移植2.3寸MIPI触摸屏及MIPI屏幕移植心得_泰山派mipi-CSDN博客](https://gitee.com/link?target=https%3A%2F%2Fblog.csdn.net%2Fqq_53686293%2Farticle%2Fdetails%2F156202739%3Fspm%3D1001.2014.3001.5502)

## 快速开始

> 本仓库与 `tspi_action_bsp` 配套使用。BSP 仓库包含系统构建相关的源码和配置，具体说明见该仓库的 README。
>
> 构建系统镜像前，将本仓库的最新提交ID填入 `tspi_action_bsp/buildroot/external/package/tspi_action_source/tspi_action_source.mk`，并确保构建环境可以访问远程 Git 仓库。应用层软件会随系统镜像自动打包。

日常调试可在本仓库直接交叉编译，在这之前，**需要先下载立创提供的 linux SDK（参考[2. SDK编译 | 立创开发板技术文档中心](https://wiki.lckfb.com/zh-hans/tspi-rk3566/project-case/fat-little-cell-phone/sdk-compile.html#linux-sdk)），和配套的`tspi_action_bsp` 仓库，按照 `tspi_action_bsp` 中的提示进行系统构建**，编译好 SDK 后，将如下示例路径替换为本机 SDK 的 Buildroot `host` 目录，然后在仓库根目录直接执行编译脚本即可编译。

```bash
export BUILDROOT_HOST_DIR=/path/to/linux_sdk/buildroot/output/rockchip_rk3566/host # 替换成你自己sdk的路径
./tools/build.sh
```

构建产物位于 `out/staging/usr/`。调试推荐使用 adb （[【立创·泰山派RK3566开发板】ADB安装与使用教程 | 立创开发板技术文档中心](https://wiki.lckfb.com/zh-hans/tspi-rk3566/tool-use/adb-install-use.html)）利用 adb 将产物同步到泰山派后，在板端终端运行相机启动脚本。具体操作见 [AGENTS.md](AGENTS.md)。

**强烈建议搭配 AI Agent 进行开发， [AGENTS.md](AGENTS.md) 中已经写好了详细的项目介绍，构建流程等，这能让你更加快速的上手，避免踩坑。**

## 相关参数

- 拍照分辨率为 3264 × 2448，约 800 万像素。
- 录像规格为 1080p（30 fps）。
- 实时预览帧率为 30 fps。
- IMU 数据记录频率目前为 100 Hz。
- 屏幕刷新率为 60 Hz。

## 相机使用

- 在预览界面短按实体按键拍照。
- 在预览界面长按实体按键开始录像，录像时短按同一按键停止。
- 点击预览界面左上方的图标进入相册，点击左下方的图标进入视频列表。
- 点击相册、视频列表、照片查看页或视频播放页左上角的返回图标，即可返回上一页。
- 在相册中点击缩略图查看照片，在视频列表中点击缩略图播放视频。长按对应缩略图可选择删除。

## 写在最后

这个项目是我在研究生阶段出于兴趣，自发设计并开发的一套软硬件系统。坦诚地说，项目目前仍存在许多不足，软件规范、架构和功能完善程度等方面都还有很大的提升空间，距离一款真正成熟、可靠且能够量产的运动相机，也还有相当长的距离。因此，也希望大家能够对项目中不够完善的地方多一些理解。

选择将项目完整开源出来，是希望把自己从零开始设计、开发和调试的过程分享出来，起到一个抛砖引玉的作用。希望能够有更多同学通过这个项目学习嵌入式 Linux，在此基础上进行自己的改进和创新，从中获得收获。

你可以基于这个项目完成自己的毕业设计，也可以将它作为求职过程中的项目经历。当然，如果你愿意，我更希望你能用它记录生活中的点点滴滴，去发现身边的美好，体验亲手为每一个像素注入灵魂的过程。

**希望这个小小的开源相机，既能成为你学习和探索技术的起点，也能让你的生活多添一份色彩。**





