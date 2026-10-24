pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
add_executable(drm-camera-preview drm-camera-preview.c ${CMAKE_SOURCE_DIR}/hal/src/camera_hal.c)
target_include_directories(drm-camera-preview PRIVATE ${CMAKE_SOURCE_DIR}/hal/include)
target_link_libraries(drm-camera-preview PRIVATE PkgConfig::RGA PkgConfig::LIBDRM)
