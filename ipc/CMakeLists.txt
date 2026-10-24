# D-Bus 接口常量（include/camera_ipc.h）和 system bus 策略文件。
add_library(camera_ipc INTERFACE)
target_include_directories(camera_ipc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(camera_ipc INTERFACE PkgConfig::DBUS)
install(FILES dbus-policy/com.tspi.MediaService.conf
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
