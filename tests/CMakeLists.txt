# This executable proves that SDK headers and symbols can compile and link.
# It does not open hardware and is not installed with the application.
add_executable(dependency-smoke dependency_smoke.cpp)
target_link_libraries(dependency-smoke PRIVATE camera_hal camera_ipc Qt5::Widgets
    PkgConfig::FFMPEG PkgConfig::ALSA PkgConfig::MPP PkgConfig::RGA
    PkgConfig::EGL PkgConfig::GLES Threads::Threads)
if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    add_test(NAME dependency-smoke COMMAND dependency-smoke)
    add_test(NAME hal-abi COMMAND media-service)
endif()
