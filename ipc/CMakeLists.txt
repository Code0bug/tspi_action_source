# Build dependency boundary only; no IPC protocol or implementation yet.
add_library(camera_ipc INTERFACE)
target_link_libraries(camera_ipc INTERFACE PkgConfig::DBUS)
