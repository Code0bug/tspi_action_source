# TSPI-ACTION

## Project Overview

**TSPI-ACTION** is an open-source HD action camera with video stabilization, built on the Taishan Pi. I designed the hardware and developed the software on my own. The links below provide the full set of open-source hardware and software materials.

This repository contains the camera's **application software**. The media service (`media-service`) and UI (`camera-ui`) run as separate processes. The software uses **V4L2, ALSA, sockets, DRM, FFmpeg, Qt, and D-Bus**. The camera supports **live preview, photo capture, video recording, photo browsing, and video playback**. It also logs IMU data alongside the video for stabilization in post-processing.

**Hardware Platform**

- Rockchip RK3566 SoC
- OV8858 camera (4-lane MIPI, with DW9714 for autofocus)
- 2.3-inch MIPI LCD touchscreen
- ICM42688P IMU (SPI interface)

**Open-Source Links**

- [Tspi Action HD Stabilized Action Camera on the Lichuang Open Hardware Platform](https://gitee.com/link?target=https%3A%2F%2Foshwhub.com%2Flzp21%2Fhd-smart-motion-camera)
- [TSPI-ACTION board support package source (`tspi_action_bsp`)](https://gitee.com/lzp1733149755/tspi_action_bsp)
- [Camera demo video on Bilibili](https://gitee.com/link?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1Qgq6BtEfx%2F%3Fspm_id_from%3D333.1387.0.0)
- [Porting the OV8858 driver and adding autofocus on the Taishan Pi (CSDN)](https://gitee.com/link?target=https%3A%2F%2Fblog.csdn.net%2Fqq_53686293%2Farticle%2Fdetails%2F153208692%3Fspm%3D1001.2014.3001.5501)
- [Porting the 2.3-inch MIPI touchscreen driver to the Taishan Pi (CSDN)](https://gitee.com/link?target=https%3A%2F%2Fblog.csdn.net%2Fqq_53686293%2Farticle%2Fdetails%2F156202739%3Fspm%3D1001.2014.3001.5502)

## Quick Start

> This repository is used with `tspi_action_bsp`, which contains the source and configuration files for building the system. See the BSP repository's README for details.
>
> Before building the system image, enter this repository's latest commit ID in `tspi_action_bsp/buildroot/external/package/tspi_action_source/tspi_action_source.mk`. Make sure the build environment can access the remote Git repository. The build will package the application software into the system image automatically.

For day-to-day debugging, you can cross-compile directly from this repository. **First download the Linux SDK from Lichuang and the companion `tspi_action_bsp` repository, then follow the BSP instructions to build the system.** See the [Lichuang SDK build guide](https://wiki.lckfb.com/zh-hans/tspi-rk3566/project-case/fat-little-cell-phone/sdk-compile.html#linux-sdk).

Once the SDK is built, set `BUILDROOT_HOST_DIR` to your SDK's Buildroot `host` directory and run the build script from the repository root.

```bash
export BUILDROOT_HOST_DIR=/path/to/linux_sdk/buildroot/output/rockchip_rk3566/host
./tools/build.sh
```

The build artifacts are staged in `out/staging/usr/`. For debugging, use `adb` to copy them to the Taishan Pi, then run the camera startup script from a terminal on the board. See the [Lichuang adb setup guide](https://wiki.lckfb.com/zh-hans/tspi-rk3566/tool-use/adb-install-use.html) and the deployment instructions in [AGENTS.md](AGENTS.md).

**I strongly recommend using an AI coding agent for development.** The project overview and build workflow in [AGENTS.md](AGENTS.md) can help you get started faster and avoid common pitfalls.

## Specifications

- Photo resolution is 3264 × 2448, approximately 8 megapixels.
- Video recording is 1080p at 30 fps.
- Live preview runs at 30 fps.
- The current IMU logging rate is 100 Hz.
- The display refresh rate is 60 Hz.

## Using the Camera

- On the preview screen, briefly press the physical button to take a photo.
- On the preview screen, press and hold the physical button to start recording. Press it briefly while recording to stop.
- Tap the icon at the upper left of the preview screen to open the photo gallery. Tap the icon at the lower left to open the video list.
- Tap the back icon at the upper left of the photo gallery, video list, photo viewer, or video player to return to the previous screen.
- Tap a thumbnail in the photo gallery to view a photo, or tap one in the video list to play a video. Press and hold a thumbnail for the option to delete it.

## Final Notes

I designed and developed this hardware and software system as a personal project during graduate school. The project still has plenty of room for improvement in coding standards, architecture, and feature completeness. It has a long way to go before it becomes a mature, reliable camera ready for mass production. I hope you'll keep those limitations in mind as you explore it.

I made the entire project open source to share how I designed, built, and debugged it from scratch. I hope it helps more students learn embedded Linux and gives them a starting point for their own improvements and ideas.

You can use it as a foundation for your graduation project or include your work on it in your portfolio when applying for jobs. I'd especially love to see you use it to capture everyday moments, notice the beauty around you, and experience what it's like to bring each pixel to life yourself.

**I hope this little open-source camera gives you a starting point for learning and exploring technology and brings a little more color to your life.**
