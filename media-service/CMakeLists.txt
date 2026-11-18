add_executable(media-service
    main.cpp
    video_manager.cpp
    service.cpp
    key_manager.cpp
    media_manager.cpp
    audio_manager.cpp
    mp4_muxer.cpp
    utils/mpp_codec.cpp)
target_include_directories(media-service PRIVATE
    ${CMAKE_SYSROOT}/usr/include/rockchip
    ${CMAKE_CURRENT_SOURCE_DIR}/utils)
target_link_libraries(media-service PRIVATE camera_hal service_ipc PkgConfig::FFMPEG PkgConfig::RGA PkgConfig::OPENCV PkgConfig::MPP Threads::Threads)
# Executables and HAL can be tested from a staged installation tree.
set_target_properties(media-service PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
install(TARGETS media-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
