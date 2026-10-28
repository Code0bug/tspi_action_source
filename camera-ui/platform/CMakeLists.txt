# Private Qt APIs: build against the exact Qt supplied by the image.
find_package(Qt5 5.15 REQUIRED COMPONENTS FbSupport FontDatabaseSupport
    EventDispatcherSupport ServiceSupport)
add_library(qtspi MODULE tspi_plugin.cpp)
set_target_properties(qtspi PROPERTIES AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms")
target_link_libraries(qtspi PRIVATE Qt5::GuiPrivate Qt5::FbSupportPrivate
    Qt5::FontDatabaseSupportPrivate Qt5::EventDispatcherSupportPrivate
    Qt5::ServiceSupportPrivate)
target_compile_options(qtspi PRIVATE -Wall -Wextra -Werror)
target_link_options(qtspi PRIVATE -Wl,--no-undefined)
install(TARGETS qtspi LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt/plugins/platforms)
