add_subdirectory(platform)
add_executable(camera-ui main.cpp camera_window.cpp camera_window.h)
set_target_properties(camera-ui PROPERTIES AUTOMOC ON)
target_link_libraries(camera-ui PRIVATE Qt5::Widgets service_ipc)
add_dependencies(camera-ui qtspi)
target_compile_definitions(camera-ui PRIVATE
    TSPI_PLUGIN_RELATIVE="../${CMAKE_INSTALL_LIBDIR}/qt/plugins")
install(TARGETS camera-ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
