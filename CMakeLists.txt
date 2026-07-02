cmake_minimum_required(VERSION 3.16)

if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.5" CACHE STRING "Minimum macOS deployment target" FORCE)
	set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary" FORCE)
	set(X88000M_DEFAULT_FETCH_SDL3 ON)
elseif(WIN32)
	set(X88000M_DEFAULT_FETCH_SDL3 ON)
else()
	set(X88000M_DEFAULT_FETCH_SDL3 OFF)
endif()

project(X88000M LANGUAGES CXX)

set(X88000_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(X88000_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(X88000_BUNDLE_RESOURCES
	"${X88000_CMAKE_DIR}/README-macOS.txt")
set(X88000_FONT_RESOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSansJP-SemiBold.ttf")
set(X88000_FONT_LICENSE_RESOURCE
	"${CMAKE_CURRENT_SOURCE_DIR}/fonts/LICENSE-NotoSansJP.txt")
set(X88000_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(X88000_IMGUI_DIR "${X88000_THIRD_PARTY_DIR}/imgui")

option(X88000M_FETCH_SDL3 "Fetch SDL3 from upstream instead of using pkg-config" ${X88000M_DEFAULT_FETCH_SDL3})
option(X88000M_FETCH_IMGUI "Fetch Dear ImGui from upstream when third_party/imgui is unavailable" ON)
option(X88000M_BUILD_TOOLS "Build command-line diagnostic tools" ON)
set(X88000M_SDL3_GIT_TAG "release-3.4.12" CACHE STRING "SDL3 git tag used by FetchContent")
set(X88000M_IMGUI_GIT_TAG "docking" CACHE STRING "Dear ImGui git tag used by FetchContent (docking branch)")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(X88000M_PKGCONFIG_READY OFF)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	set(X88000M_PKGCONFIG_READY ON)
endif()

set(X88000_CORE_SOURCES
	"${X88000_SRC_DIR}/StdHeader.cpp"
	"${X88000_SRC_DIR}/X88Utility.cpp"
	"${X88000_SRC_DIR}/X88EnvFile.cpp"
	"${X88000_SRC_DIR}/DiskImage.cpp"
	"${X88000_SRC_DIR}/DiskImageCollection.cpp"
	"${X88000_SRC_DIR}/DiskImageFile.cpp"
	"${X88000_SRC_DIR}/X88DiskImageMemory.cpp"
	"${X88000_SRC_DIR}/TapeImage.cpp"
	"${X88000_SRC_DIR}/ParallelDevice.cpp"
	"${X88000_SRC_DIR}/ParallelNull.cpp"
	"${X88000_SRC_DIR}/ParallelPR201.cpp"
	"${X88000_SRC_DIR}/ParallelPrinter.cpp"
	"${X88000_SRC_DIR}/X88PrinterDrawerCGData.cpp"
	"${X88000_SRC_DIR}/PC88.cpp"
	"${X88000_SRC_DIR}/PC88Calendar.cpp"
	"${X88000_SRC_DIR}/PC88Crtc.cpp"
	"${X88000_SRC_DIR}/PC88Fdc.cpp"
	"${X88000_SRC_DIR}/PC88Opna.cpp"
	"${X88000_SRC_DIR}/PC88Pcg.cpp"
	"${X88000_SRC_DIR}/PC88Ppi.cpp"
	"${X88000_SRC_DIR}/PC88Timer.cpp"
	"${X88000_SRC_DIR}/PC88Usart.cpp"
	"${X88000_SRC_DIR}/PC88Z80Main.cpp"
	"${X88000_SRC_DIR}/PC88Z80Sub.cpp"
	"${X88000_SRC_DIR}/X88ScreenDrawer.cpp"
	"${X88000_SRC_DIR}/Z80.cpp"
	"${X88000_SRC_DIR}/Z80_2.cpp"
	"${X88000_SRC_DIR}/Z80Adapter.cpp"
	"${X88000_SRC_DIR}/Z80Data.cpp")

set(X88000_FRONTEND_SOURCES
	"${X88000_SRC_DIR}/sdl3/Sdl3FrontendMain.cpp"
	"${X88000_SRC_DIR}/sdl3/Sdl3Settings.cpp"
	"${X88000_SRC_DIR}/sdl3/Sdl3AudioOutput.cpp")

add_library(x88core STATIC ${X88000_CORE_SOURCES})
target_include_directories(x88core PUBLIC "${X88000_SRC_DIR}")
target_compile_definitions(x88core PUBLIC
	X88_GUI_SDL3=1
	X88_PRINTER_DRAW_NO_BACKEND=1)
find_library(X88000M_ICONV_LIBRARY NAMES iconv)
if(X88000M_ICONV_LIBRARY)
	target_link_libraries(x88core PUBLIC "${X88000M_ICONV_LIBRARY}")
endif()
set_target_properties(x88core PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF)

if(X88000M_BUILD_TOOLS)
	add_executable(opn_log_render
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/opn_log_render.cpp")
	target_link_libraries(opn_log_render PRIVATE x88core)
	target_include_directories(opn_log_render PRIVATE "${X88000_SRC_DIR}")
	set_target_properties(opn_log_render PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF)
endif()

set(X88000M_SDL3_LINK_TARGET "")
if(NOT X88000M_FETCH_SDL3 AND X88000M_PKGCONFIG_READY)
	pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
endif()
if(NOT X88000M_FETCH_SDL3 AND X88000M_PKGCONFIG_READY AND SDL3_FOUND)
	set(X88000M_SDL3_LINK_TARGET PkgConfig::SDL3)
elseif(X88000M_FETCH_SDL3)
	include(FetchContent)
	FetchContent_Declare(
		SDL3
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG ${X88000M_SDL3_GIT_TAG}
		GIT_SHALLOW TRUE)
	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
	set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
	set(SDL_SAMPLES OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(SDL3)
	if(TARGET SDL3::SDL3)
		set(X88000M_SDL3_LINK_TARGET SDL3::SDL3)
	elseif(TARGET SDL3-static)
		add_library(SDL3::SDL3 ALIAS SDL3-static)
		set(X88000M_SDL3_LINK_TARGET SDL3::SDL3)
	else()
		message(FATAL_ERROR "Failed to resolve SDL3::SDL3 after FetchContent.")
	endif()
else()
	message(FATAL_ERROR "SDL3 not found. Install the pkg-config module 'sdl3' or configure with -DX88000M_FETCH_SDL3=ON.")
endif()

set(X88000M_IMGUI_SOURCE_DIR "")
if(EXISTS "${X88000_IMGUI_DIR}/imgui.h")
	set(X88000M_IMGUI_SOURCE_DIR "${X88000_IMGUI_DIR}")
elseif(X88000M_FETCH_IMGUI)
	include(FetchContent)
	set(X88000M_IMGUI_DL_DIR "${CMAKE_BINARY_DIR}/_deps/imgui-src")
	if(NOT EXISTS "${X88000M_IMGUI_DL_DIR}/imgui.h")
		message(STATUS "Fetching Dear ImGui (${X88000M_IMGUI_GIT_TAG})...")
		execute_process(
			COMMAND git clone --depth 1 --branch ${X88000M_IMGUI_GIT_TAG}
				https://github.com/ocornut/imgui.git
				"${X88000M_IMGUI_DL_DIR}"
			RESULT_VARIABLE _git_result)
		if(NOT _git_result EQUAL 0)
			message(FATAL_ERROR "Failed to fetch Dear ImGui")
		endif()
	endif()
	set(X88000M_IMGUI_SOURCE_DIR "${X88000M_IMGUI_DL_DIR}")
else()
	message(FATAL_ERROR "Dear ImGui not found at third_party/imgui. Configure with -DX88000M_FETCH_IMGUI=ON to fetch automatically.")
endif()

if(APPLE)
	add_executable(X88000M MACOSX_BUNDLE ${X88000_FRONTEND_SOURCES})
elseif(WIN32)
	set(X88000_WIN_RC "${CMAKE_SOURCE_DIR}/resources/X88000M.rc")
	add_executable(X88000M WIN32 ${X88000_FRONTEND_SOURCES} ${X88000_WIN_RC})
	# Use main() as the entry point instead of WinMain.
	target_link_options(X88000M PRIVATE "/ENTRY:mainCRTStartup")
else()
	add_executable(X88000M ${X88000_FRONTEND_SOURCES})
endif()

target_link_libraries(X88000M PRIVATE ${X88000M_SDL3_LINK_TARGET} x88core)
target_compile_definitions(X88000M PRIVATE
	X88000_SDL3_HAS_CORE=1
	X88000_SDL3_HAS_IMGUI=1
	X88000M_VERSION="1.0.6"
	SDL_MAIN_HANDLED=1)
target_include_directories(X88000M PRIVATE
	"${X88000_SRC_DIR}"
	"${X88000M_IMGUI_SOURCE_DIR}"
	"${X88000M_IMGUI_SOURCE_DIR}/backends")
target_sources(X88000M PRIVATE
	"${X88000M_IMGUI_SOURCE_DIR}/imgui.cpp"
	"${X88000M_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
	"${X88000M_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
	"${X88000M_IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
	"${X88000M_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
	"${X88000M_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
set_target_properties(X88000M PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF)

if(APPLE)
	set_source_files_properties(${X88000_BUNDLE_RESOURCES} PROPERTIES
		MACOSX_PACKAGE_LOCATION Resources)
	set_source_files_properties(${X88000_FONT_RESOURCES} PROPERTIES
		MACOSX_PACKAGE_LOCATION "Resources/fonts")
	set_source_files_properties(${X88000_FONT_LICENSE_RESOURCE} PROPERTIES
		MACOSX_PACKAGE_LOCATION "Resources/fonts")
	target_sources(X88000M PRIVATE
		${X88000_BUNDLE_RESOURCES}
		${X88000_FONT_RESOURCES}
		${X88000_FONT_LICENSE_RESOURCE})

	set(X88000_ICON_FILE "${CMAKE_SOURCE_DIR}/resources/AppIcon.icns")
	if(EXISTS "${X88000_ICON_FILE}")
		set_source_files_properties("${X88000_ICON_FILE}" PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources)
		target_sources(X88000M PRIVATE "${X88000_ICON_FILE}")
	endif()

	set_target_properties(X88000M PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST
			"${X88000_CMAKE_DIR}/Info.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME "X88000M"
		MACOSX_BUNDLE_BUNDLE_VERSION "8"
		MACOSX_BUNDLE_COPYRIGHT "Original X88000 by Manuke; repository maintenance by bubio"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.bubio.x88000m"
		MACOSX_BUNDLE_LONG_VERSION_STRING "X88000M 1.0.6(8)"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.6"
		MACOSX_BUNDLE_ICON_FILE "AppIcon")
elseif(WIN32)
	# Copy font alongside the executable so it can be found at runtime.
	add_custom_command(TARGET X88000M POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
			"$<TARGET_FILE_DIR:X88000M>/fonts"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${X88000_FONT_RESOURCES}
			"$<TARGET_FILE_DIR:X88000M>/fonts/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${X88000_FONT_LICENSE_RESOURCE}
			"$<TARGET_FILE_DIR:X88000M>/fonts/")
endif()

# Installation and CPack configuration
include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
	install(TARGETS X88000M RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(FILES
		${X88000_FONT_RESOURCES}
		${X88000_FONT_LICENSE_RESOURCE}
		DESTINATION ${CMAKE_INSTALL_DATADIR}/X88000M/fonts)

	# Desktop file
	set(X88000_DESKTOP_FILE "${CMAKE_BINARY_DIR}/X88000M.desktop")
	file(WRITE "${X88000_DESKTOP_FILE}"
		"[Desktop Entry]\n"
		"Type=Application\n"
		"Name=X88000M\n"
		"Comment=PC-8801 Emulator\n"
		"Exec=X88000M\n"
		"Icon=X88000M\n"
		"Categories=Game;Emulator;\n")
	install(FILES "${X88000_DESKTOP_FILE}" DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

	# Icon (Using a more standard path, still 1024 for now but renamed appropriately)
	install(FILES "${CMAKE_SOURCE_DIR}/docs/AppIcon.png"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps"
		RENAME "X88000M.png")

	# Define installation paths for the source code to use
	target_compile_definitions(X88000M PRIVATE
		X88000_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/X88000M")
endif()

set(CPACK_PACKAGE_NAME "X88000M")
set(CPACK_PACKAGE_VENDOR "bubio")
set(CPACK_PACKAGE_VERSION "1.0.6")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PC-8801 Emulator")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(UNIX AND NOT APPLE)
	set(CPACK_GENERATOR "DEB;RPM")
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "bubio")
	set(CPACK_DEBIAN_PACKAGE_SECTION "games")
	set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
	set(CPACK_RPM_PACKAGE_AUTOREQ ON)
endif()

include(CPack)
