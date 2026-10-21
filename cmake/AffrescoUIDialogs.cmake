# The file dialogs of the render module (open, save, choose a folder), included by modules/render/CMakeLists.txt:
# nativefiledialog-extended, downloaded as a release archive pinned by its hash and compiled into the render library
# (its own build isn't run). The OS shows the dialogs: Cocoa on macOS, the shell's dialogs on Windows, and on other
# systems the desktop portal over D-Bus, when the D-Bus development files are there (else there are no file dialogs).
set(AFFRESCOUI_NFD_VERSION 1.4.1)
FetchContent_Declare(nfd
	URL https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v${AFFRESCOUI_NFD_VERSION}.tar.gz
	URL_HASH SHA256=4a01eafa921169f009f3e83bea8e287d9e5d0b94ca9064cf7f0a546652d7c884
	SOURCE_SUBDIR affrescoui-compiles-its-sources SYSTEM) # a folder it doesn't have: its CMakeLists.txt isn't run
FetchContent_MakeAvailable(nfd)

# affrescoui_file_dialogs(<target>): adds the dialogs' sources for this OS to the target, or defines
# AFFRESCOUI_NO_FILE_DIALOGS on it where there are none
function(affrescoui_file_dialogs target)
	set(src ${nfd_SOURCE_DIR}/src)
	if(APPLE)
		set(sources ${src}/nfd_cocoa.m)
		target_link_libraries(${target} PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
	elseif(WIN32)
		set(sources ${src}/nfd_win.cpp)
		target_link_libraries(${target} PRIVATE ole32 uuid shell32)
	else()
		find_package(PkgConfig QUIET)
		if(PKG_CONFIG_FOUND)
			pkg_check_modules(AFFRESCOUI_DBUS QUIET dbus-1)
		endif()
		if(NOT AFFRESCOUI_DBUS_FOUND)
			message(STATUS "AffrescoUI: no file dialogs (the D-Bus development files, dbus-1, weren't found)")
			target_compile_definitions(${target} PRIVATE AFFRESCOUI_NO_FILE_DIALOGS)
			return()
		endif()
		set(sources ${src}/nfd_portal.cpp)
		target_include_directories(${target} SYSTEM PRIVATE ${AFFRESCOUI_DBUS_INCLUDE_DIRS})
		target_link_libraries(${target} PRIVATE ${AFFRESCOUI_DBUS_LINK_LIBRARIES})
		target_compile_definitions(${target} PRIVATE NFD_PORTAL) # nfd.h reads it too
	endif()
	target_sources(${target} PRIVATE ${sources})
	target_include_directories(${target} SYSTEM PRIVATE ${src}/include)
	# Its warnings are not ours to fix; the macOS file manages its memory by hand (no ARC), and picks the filter API
	# from the deployment target
	if(MSVC)
		set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "/w" SKIP_LINTING ON)
	else()
		set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "-w" SKIP_LINTING ON)
	endif()
endfunction()
