add_library(media_hal SHARED src/camera_hal.c src/audio_hal.c)
target_include_directories(media_hal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(media_hal PRIVATE PkgConfig::ALSA PkgConfig::MPP PkgConfig::RGA)
set_target_properties(media_hal PROPERTIES
    OUTPUT_NAME media_hal
    C_VISIBILITY_PRESET default)
target_link_options(media_hal PRIVATE -Wl,--no-undefined)
install(TARGETS media_hal LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
