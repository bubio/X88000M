cmake_minimum_required(VERSION 3.16)

project(X88000M LANGUAGES CXX)

set(X88000_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED IMPORTED_TARGET gtk+-2.0)

file(GLOB X88000_SOURCES CONFIGURE_DEPENDS
	"${X88000_SRC_DIR}/*.cpp")

add_executable(X88000 ${X88000_SOURCES})

target_include_directories(X88000 PRIVATE "${X88000_SRC_DIR}")
target_link_libraries(X88000 PRIVATE PkgConfig::GTK2)

if(NOT APPLE)
	find_package(X11 REQUIRED)
	target_include_directories(X88000 PRIVATE ${X11_INCLUDE_DIR})
	target_link_libraries(X88000 PRIVATE ${X11_LIBRARIES})
endif()
