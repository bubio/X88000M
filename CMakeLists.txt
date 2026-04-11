cmake_minimum_required(VERSION 3.16)

project(X88000M LANGUAGES CXX)

set(X88000_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(X88000_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(X88000_BUNDLE_RESOURCES
	"${X88000_CMAKE_DIR}/README-macOS.txt")
set(X88000_FONT_RESOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSansJP-Regular.ttf")
set(X88000_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(X88000_IMGUI_DIR "${X88000_THIRD_PARTY_DIR}/imgui")

option(X88000M_BUILD_LEGACY_GTK "Build legacy GTK frontend" ON)
option(X88000M_BUILD_SDL3_FRONTEND "Build SDL3 frontend prototype" ON)
option(X88000M_FETCH_SDL3 "Fetch SDL3 from upstream when pkg-config is unavailable" OFF)
option(X88000M_FETCH_IMGUI "Fetch Dear ImGui from upstream when third_party/imgui is unavailable" ON)
set(X88000M_SDL3_GIT_TAG "release-3.4.2" CACHE STRING "SDL3 git tag used by FetchContent")
set(X88000M_IMGUI_GIT_TAG "docking" CACHE STRING "Dear ImGui git tag used by FetchContent (docking branch)")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)

file(GLOB X88000_SOURCES CONFIGURE_DEPENDS
	"${X88000_SRC_DIR}/*.cpp")
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

set(X88000M_GTK2_READY OFF)
if(X88000M_BUILD_LEGACY_GTK OR X88000M_BUILD_SDL3_FRONTEND)
	pkg_check_modules(GTK2 REQUIRED IMPORTED_TARGET gtk+-2.0)
	set(X88000M_GTK2_READY ON)
endif()

if(X88000M_BUILD_SDL3_FRONTEND AND X88000M_GTK2_READY)
	add_library(x88core STATIC ${X88000_CORE_SOURCES})
	target_include_directories(x88core PUBLIC "${X88000_SRC_DIR}")
	target_link_libraries(x88core PUBLIC PkgConfig::GTK2)
	target_compile_definitions(x88core PUBLIC X88_PRINTER_DRAW_NO_BACKEND=1)
	set_target_properties(x88core PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF)
endif()

if(X88000M_BUILD_LEGACY_GTK)
	if(APPLE)
		add_executable(X88000 MACOSX_BUNDLE ${X88000_SOURCES})
	else()
		add_executable(X88000 ${X88000_SOURCES})
	endif()

	target_include_directories(X88000 PRIVATE "${X88000_SRC_DIR}")
	target_link_libraries(X88000 PRIVATE PkgConfig::GTK2)

	set_target_properties(X88000 PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF)

	if(NOT APPLE)
		find_package(X11 REQUIRED)
		target_include_directories(X88000 PRIVATE ${X11_INCLUDE_DIR})
		target_link_libraries(X88000 PRIVATE ${X11_LIBRARIES})
	endif()

	if(APPLE)
		set_source_files_properties(${X88000_BUNDLE_RESOURCES} PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources)
		target_sources(X88000 PRIVATE ${X88000_BUNDLE_RESOURCES})

		set_target_properties(X88000 PROPERTIES
			MACOSX_BUNDLE_INFO_PLIST
				"${X88000_CMAKE_DIR}/Info.plist.in"
			MACOSX_BUNDLE_BUNDLE_NAME "X88000M"
			MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
			MACOSX_BUNDLE_COPYRIGHT "Original X88000 by Manuke; repository maintenance by bubio"
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.bubio.x88000m"
			MACOSX_BUNDLE_LONG_VERSION_STRING "X88000M 0.1.0"
			MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0")
	endif()
endif()

if(X88000M_BUILD_SDL3_FRONTEND)
	set(X88000M_SDL3_AVAILABLE OFF)
	set(X88000M_SDL3_LINK_TARGET "")
	pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
	if(SDL3_FOUND)
		set(X88000M_SDL3_AVAILABLE ON)
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
			set(X88000M_SDL3_AVAILABLE ON)
			set(X88000M_SDL3_LINK_TARGET SDL3::SDL3)
		elseif(TARGET SDL3-static)
			add_library(SDL3::SDL3 ALIAS SDL3-static)
			set(X88000M_SDL3_AVAILABLE ON)
			set(X88000M_SDL3_LINK_TARGET SDL3::SDL3)
		else()
			message(WARNING "SDL3 frontend disabled: failed to resolve SDL3::SDL3 after FetchContent.")
		endif()
	else()
		message(WARNING "SDL3 frontend disabled: pkg-config module 'sdl3' not found. Set X88000M_FETCH_SDL3=ON to fetch from source.")
	endif()

		if(X88000M_SDL3_AVAILABLE)
			set(X88000_SDL3_SOURCES
				"${X88000_SRC_DIR}/sdl3/Sdl3FrontendMain.cpp"
				"${X88000_SRC_DIR}/sdl3/Sdl3Settings.cpp"
				"${X88000_SRC_DIR}/sdl3/Sdl3AudioOutput.cpp")

			if(APPLE)
				add_executable(X88000SDL3 MACOSX_BUNDLE ${X88000_SDL3_SOURCES})
			else()
				add_executable(X88000SDL3 ${X88000_SDL3_SOURCES})
			endif()
			target_link_libraries(X88000SDL3 PRIVATE ${X88000M_SDL3_LINK_TARGET})
			if(TARGET x88core)
				target_link_libraries(X88000SDL3 PRIVATE x88core)
				target_compile_definitions(X88000SDL3 PRIVATE
				X88000_SDL3_HAS_CORE=1
				X88000M_VERSION="0.1.0")
			endif()
			target_include_directories(X88000SDL3 PRIVATE "${X88000_SRC_DIR}")
			set_target_properties(X88000SDL3 PROPERTIES
				CXX_STANDARD 11
				CXX_STANDARD_REQUIRED ON
				CXX_EXTENSIONS OFF)
			if(APPLE)
				set_source_files_properties(${X88000_BUNDLE_RESOURCES} PROPERTIES
					MACOSX_PACKAGE_LOCATION Resources)
				set_source_files_properties(${X88000_FONT_RESOURCES} PROPERTIES
					MACOSX_PACKAGE_LOCATION "Resources/fonts")
				target_sources(X88000SDL3 PRIVATE
					${X88000_BUNDLE_RESOURCES}
					${X88000_FONT_RESOURCES})
				# App icon
				set(X88000_ICON_FILE "${CMAKE_SOURCE_DIR}/resources/AppIcon.icns")
				if(EXISTS "${X88000_ICON_FILE}")
					set_source_files_properties("${X88000_ICON_FILE}" PROPERTIES
						MACOSX_PACKAGE_LOCATION Resources)
					target_sources(X88000SDL3 PRIVATE "${X88000_ICON_FILE}")
				endif()
				set_target_properties(X88000SDL3 PROPERTIES
					MACOSX_BUNDLE_INFO_PLIST
						"${X88000_CMAKE_DIR}/Info.plist.in"
					MACOSX_BUNDLE_BUNDLE_NAME "X88000M SDL3"
					MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
					MACOSX_BUNDLE_COPYRIGHT "Original X88000 by Manuke; repository maintenance by bubio"
					MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.bubio.x88000m.sdl3"
					MACOSX_BUNDLE_LONG_VERSION_STRING "X88000M SDL3 0.1.0"
					MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
					MACOSX_BUNDLE_ICON_FILE "AppIcon")
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
			set(imgui_SOURCE_DIR "${X88000M_IMGUI_DL_DIR}")
			set(X88000M_IMGUI_SOURCE_DIR "${imgui_SOURCE_DIR}")
		endif()

		if(X88000M_IMGUI_SOURCE_DIR)
			target_compile_definitions(X88000SDL3 PRIVATE X88000_SDL3_HAS_IMGUI=1)
			target_include_directories(X88000SDL3 PRIVATE
				"${X88000M_IMGUI_SOURCE_DIR}"
				"${X88000M_IMGUI_SOURCE_DIR}/backends")
			target_sources(X88000SDL3 PRIVATE
				"${X88000M_IMGUI_SOURCE_DIR}/imgui.cpp"
				"${X88000M_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
				"${X88000M_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
				"${X88000M_IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
				"${X88000M_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
				"${X88000M_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
		else()
			message(STATUS "X88000SDL3: Dear ImGui not found at third_party/imgui. Set X88000M_FETCH_IMGUI=ON to fetch automatically.")
		endif()
	endif()
endif()
