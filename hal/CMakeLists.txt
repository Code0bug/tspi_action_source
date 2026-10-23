add_library(camera_hal SHARED src/v4l2_camera.c)
target_include_directories(camera_hal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(camera_hal PRIVATE PkgConfig::ALSA PkgConfig::MPP PkgConfig::RGA Threads::Threads)
set_target_properties(camera_hal PROPERTIES
    OUTPUT_NAME camera_hal VERSION 0.1.0 SOVERSION 0
    C_VISIBILITY_PRESET default)
target_link_options(camera_hal PRIVATE -Wl,--no-undefined)
install(TARGETS camera_hal LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
