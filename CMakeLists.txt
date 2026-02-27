cmake_minimum_required(VERSION 3.21)
project(links-widget VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
include(GNUInstallDirs)

if(NOT TARGET widget-sdk)
    find_package(widget-sdk REQUIRED)
endif()

add_library(links-widget MODULE
    LinksWidget.cpp
    LinksWidget.h
)

target_link_libraries(links-widget PRIVATE Qt6::Widgets widget-sdk)
target_compile_definitions(links-widget PRIVATE DASHBOARD_WIDGET_LIBRARY)

set_target_properties(links-widget PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

install(TARGETS links-widget
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dashboard/plugins
)
install(FILES links.json
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/dashboard/plugins
)
