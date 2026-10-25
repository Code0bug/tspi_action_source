add_executable(media-service main.cpp camera_manager.cpp service.cpp preview_server.cpp)
target_link_libraries(media-service PRIVATE camera_hal service_ipc preview_ipc
    PkgConfig::FFMPEG PkgConfig::RGA PkgConfig::LIBDRM Threads::Threads)
# Executables and HAL can be tested from a staged installation tree.
set_target_properties(media-service PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
install(TARGETS media-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
