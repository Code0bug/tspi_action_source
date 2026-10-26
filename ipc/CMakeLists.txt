add_library(ipc_socket STATIC ipc_socket.c)
target_include_directories(ipc_socket PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(TARGETS ipc_socket ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ipc_socket.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# D-Bus 接口常量（include/media_service.h）和 system bus 策略文件。
add_library(service_ipc INTERFACE)
target_include_directories(service_ipc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(service_ipc INTERFACE PkgConfig::DBUS ipc_socket)
install(FILES dbus-policy/com.tspi.MediaService.conf
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
