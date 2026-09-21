add_executable(media-service main.cpp)
target_link_libraries(media-service PRIVATE camera_hal camera_ipc PkgConfig::FFMPEG Threads::Threads)
# Executables and HAL can be tested from a staged installation tree.
set_target_properties(media-service PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
install(TARGETS media-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
