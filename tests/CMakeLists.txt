add_executable(display-camera-test display-camera-test.c
    ${CMAKE_SOURCE_DIR}/hal/src/camera_hal.c
    ${CMAKE_SOURCE_DIR}/hal/src/display_hal.c)
target_include_directories(display-camera-test PRIVATE ${CMAKE_SOURCE_DIR}/hal/include)
target_link_libraries(display-camera-test PRIVATE PkgConfig::RGA PkgConfig::LIBDRM)

add_executable(display_color_test display_color_test.c ${CMAKE_SOURCE_DIR}/hal/src/display_hal.c)
target_include_directories(display_color_test PRIVATE ${CMAKE_SOURCE_DIR}/hal/include)
target_link_libraries(display_color_test PRIVATE PkgConfig::LIBDRM)
