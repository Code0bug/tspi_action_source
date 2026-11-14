add_executable(media-service main.cpp video_manager.cpp service.cpp key_manager.cpp media_manager.cpp)
target_link_libraries(media-service PRIVATE camera_hal service_ipc PkgConfig::FFMPEG PkgConfig::RGA PkgConfig::OPENCV Threads::Threads)
# Executables and HAL can be tested from a staged installation tree.
set_target_properties(media-service PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
install(TARGETS media-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
