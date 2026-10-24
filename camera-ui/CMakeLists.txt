add_executable(camera-ui main.cpp)
target_link_libraries(camera-ui PRIVATE Qt5::Widgets service_ipc)
install(TARGETS camera-ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
