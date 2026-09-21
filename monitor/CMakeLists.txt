add_executable(camera-monitor main.cpp)
target_link_libraries(camera-monitor PRIVATE Threads::Threads)
install(TARGETS camera-monitor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
