# AccessKit, the bridge from a window's accessibility tree to the platform's API (NSAccessibility on macOS, UI Automation
# on Windows, AT-SPI on Linux), included by CMakeLists.txt (decision D8). The release archive of its C bindings is
# pinned by its hash like the other dependencies, and holds prebuilt static libraries for each platform, so no Rust
# toolchain is needed. The target affrescoui_accesskit links the one for this platform and architecture, with the
# system libraries it needs. AFFRESCOUI_ACCESSIBILITY=OFF leaves it out: windows then expose nothing to assistive
# technologies (modules/render/src/platform/AccessibilityNone.cpp).
option(AFFRESCOUI_ACCESSIBILITY "Expose windows to screen readers through AccessKit (a prebuilt download)" ON)
if(NOT AFFRESCOUI_ACCESSIBILITY)
	return()
endif()

set(AFFRESCOUI_ACCESSKIT_VERSION 0.23.1)
FetchContent_Declare(accesskit
	URL https://github.com/AccessKit/accesskit-c/releases/download/${AFFRESCOUI_ACCESSKIT_VERSION}/accesskit-c-${AFFRESCOUI_ACCESSKIT_VERSION}.zip
	URL_HASH SHA256=35b7ca8a6f1e038b5da35e1e9e5a0adaed9bfcf21e1496d29598fbbadcc7043f
	SOURCE_SUBDIR affrescoui-uses-the-prebuilt-libraries SYSTEM) # a folder it doesn't have: its CMakeLists.txt isn't run
FetchContent_MakeAvailable(accesskit)

# The platform and the architecture, as the archive names its folders
if(APPLE)
	set(AFFRESCOUI_ACCESSKIT_OS macos)
elseif(WIN32)
	set(AFFRESCOUI_ACCESSKIT_OS windows)
else()
	set(AFFRESCOUI_ACCESSKIT_OS linux)
endif()
if(CMAKE_OSX_ARCHITECTURES)
	set(AFFRESCOUI_ACCESSKIT_PROCESSOR "${CMAKE_OSX_ARCHITECTURES}")
elseif(CMAKE_GENERATOR_PLATFORM)
	set(AFFRESCOUI_ACCESSKIT_PROCESSOR "${CMAKE_GENERATOR_PLATFORM}")
else()
	set(AFFRESCOUI_ACCESSKIT_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(AFFRESCOUI_ACCESSKIT_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)")
	set(AFFRESCOUI_ACCESSKIT_ARCH arm64)
elseif(AFFRESCOUI_ACCESSKIT_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)")
	set(AFFRESCOUI_ACCESSKIT_ARCH x86_64)
else()
	set(AFFRESCOUI_ACCESSKIT_ARCH x86)
endif()

set(AFFRESCOUI_ACCESSKIT_DIR ${accesskit_SOURCE_DIR}/lib/${AFFRESCOUI_ACCESSKIT_OS}/${AFFRESCOUI_ACCESSKIT_ARCH})
if(MSVC)
	set(AFFRESCOUI_ACCESSKIT_LIBRARY ${AFFRESCOUI_ACCESSKIT_DIR}/msvc/static/accesskit.lib)
elseif(WIN32)
	set(AFFRESCOUI_ACCESSKIT_LIBRARY ${AFFRESCOUI_ACCESSKIT_DIR}/mingw/static/libaccesskit.a)
else()
	set(AFFRESCOUI_ACCESSKIT_LIBRARY ${AFFRESCOUI_ACCESSKIT_DIR}/static/libaccesskit.a)
endif()
if(NOT EXISTS ${AFFRESCOUI_ACCESSKIT_LIBRARY})
	message(FATAL_ERROR "AccessKit has no static library for ${AFFRESCOUI_ACCESSKIT_OS} on ${AFFRESCOUI_ACCESSKIT_ARCH}: "
		"configure with -DAFFRESCOUI_ACCESSIBILITY=OFF")
endif()

# The libraries the prebuilt one needs, as its own accesskit-config.cmake lists them
if(APPLE)
	set(AFFRESCOUI_ACCESSKIT_SYSTEM "-framework AppKit" "-framework Foundation" "-framework CoreFoundation" objc)
elseif(WIN32)
	set(AFFRESCOUI_ACCESSKIT_SYSTEM bcrypt ntdll propsys runtimeobject uiautomationcore userenv ws2_32 comctl32)
else()
	find_package(Threads REQUIRED)
	set(AFFRESCOUI_ACCESSKIT_SYSTEM Threads::Threads m ${CMAKE_DL_LIBS})
endif()

# An interface target, so the installed package can name the library copied next to ours
include(GNUInstallDirs)
get_filename_component(AFFRESCOUI_ACCESSKIT_NAME ${AFFRESCOUI_ACCESSKIT_LIBRARY} NAME)
add_library(affrescoui_accesskit INTERFACE)
target_include_directories(affrescoui_accesskit SYSTEM INTERFACE $<BUILD_INTERFACE:${accesskit_SOURCE_DIR}/include>)
target_link_libraries(affrescoui_accesskit INTERFACE $<BUILD_INTERFACE:${AFFRESCOUI_ACCESSKIT_LIBRARY}>
	$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/${AFFRESCOUI_ACCESSKIT_NAME}> ${AFFRESCOUI_ACCESSKIT_SYSTEM})
if(APPLE)
	target_compile_definitions(affrescoui_accesskit INTERFACE ACCESSKIT_MACOS)
endif()
set_target_properties(affrescoui_accesskit PROPERTIES EXPORT_NAME accesskit)
if(AFFRESCOUI_INSTALL)
	install(TARGETS affrescoui_accesskit EXPORT AffrescoUITargets)
	install(FILES ${AFFRESCOUI_ACCESSKIT_LIBRARY} DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(FILES ${accesskit_SOURCE_DIR}/LICENSE-MIT ${accesskit_SOURCE_DIR}/LICENSE-APACHE
		DESTINATION ${CMAKE_INSTALL_DOCDIR}/accesskit)
endif()
