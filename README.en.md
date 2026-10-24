# tspi-action

This branch contains a build skeleton, not a working camera application.

- camera-ui: minimal Qt Widgets entry point.
- media-service: minimal C++ executable linked to the C HAL.
- hal: versioned shared library with an ABI marker only.
- monitor: minimal entry point; supervision is not implemented.
- ipc: D-Bus build dependency; communication is not implemented.
- platform: preserved board resources, patches and device trees.

Set BUILDROOT_HOST_DIR to the RK3566 Buildroot host directory, then run
`./tools/build.sh`. Outputs are in out/rk3566; staging is in out/staging.
Qt5 Widgets, D-Bus, FFmpeg, ALSA, MPP and RGA are required in the
target sysroot. Rockit is no longer linked.

See [README.md](README.md) for instructions and limitations, and
[doc/refactoring.md](doc/refactoring.md) for the target architecture.
