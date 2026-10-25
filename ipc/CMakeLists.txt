# D-Bus 接口常量（include/media_service.h）和 system bus 策略文件。
add_library(service_ipc INTERFACE)
add_library(preview_ipc STATIC preview_protocol.c)
target_include_directories(preview_ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(service_ipc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(service_ipc INTERFACE PkgConfig::DBUS)
install(FILES dbus-policy/com.tspi.MediaService.conf
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
