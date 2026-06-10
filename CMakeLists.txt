cmake_minimum_required(VERSION 3.31)
project(DemoApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

add_executable(demo_app cpp/main/main.cpp)

target_link_libraries(demo_app PRIVATE 
    PkgConfig::GSTREAMER 
    PkgConfig::GTK3
)

if(BUILD_TESTING)
    add_test(NAME DemoApp COMMAND demo_app)
endif()
