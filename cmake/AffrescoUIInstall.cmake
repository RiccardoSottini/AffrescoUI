# Install rules and the AffrescoUI CMake package, included by CMakeLists.txt when AFFRESCOUI_INSTALL is on.
#
#   cmake --install build --prefix <prefix>
#
# installs the module libraries, their public headers (affresco/...), the license files and the package
# <prefix>/lib/cmake/AffrescoUI, so an application can write
#
#   find_package(AffrescoUI 0.1 CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE affrescoui::affrescoui)   # or one module: affrescoui::vcs, ...
#
# The dependencies are installed into the same prefix: the exact nlohmann/json, GLFW and FreeType this build pinned. The public
# header core/Json.hpp includes nlohmann/json, whose types carry its version in their names (json_abi_v3_11_3), so
# another version wouldn't link; and the static render library leaves GLFW to the application's link, which should be
# the GLFW it was tested with. AffrescoUIConfig.cmake finds them with find_dependency, so each is still one package an
# application that uses it too shares.
#
# Only static libraries are supported. The log sink (core/Log.hpp) is a static inside an inline function: with
# shared libraries each one could get its own copy (hidden visibility, Windows DLLs), and setLogSink() wouldn't reach
# them all. Making shared libraries work needs a compiled home for the sink and exported symbols.

if(BUILD_SHARED_LIBS)
	message(FATAL_ERROR "AFFRESCOUI_INSTALL needs static libraries (BUILD_SHARED_LIBS=OFF): "
		"cmake/AffrescoUIInstall.cmake explains why")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# AffrescoUIConfig.cmake asks for AFFRESCOUI_JSON_VERSION and AFFRESCOUI_GLFW_VERSION, which CMakeLists.txt pins
set(AFFRESCOUI_PACKAGE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/AffrescoUI)

# Exported as affrescoui::<module>, the same names as the aliases add_subdirectory users link to
foreach(module core data render export scene3d imaging format themes vcs channel net services automation)
	set_target_properties(affrescoui_${module} PROPERTIES EXPORT_NAME ${module})
	install(DIRECTORY modules/${module}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endforeach()
install(DIRECTORY modules/screen/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}) # Screen.hpp, affresco.hpp

# HarfBuzz is built here as one static library (cmake/AffrescoUIText.cmake), so it is installed as one of ours,
# affrescoui::harfbuzz; FreeType installs its own package, found by AffrescoUIConfig.cmake
install(TARGETS affrescoui_core affrescoui_data affrescoui_render affrescoui_export affrescoui_scene3d affrescoui_imaging affrescoui_format affrescoui_themes affrescoui_vcs affrescoui_channel affrescoui_net affrescoui_services affrescoui
	affrescoui_automation affrescoui_harfbuzz affrescoui_harfbuzz_subset affrescoui_regex affrescoui_syntax
	EXPORT AffrescoUITargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(TARGET affrescoui_audio) # the audio module (AFFRESCOUI_AUDIO): affrescoui::audio, affresco/audio/
	set_target_properties(affrescoui_audio PROPERTIES EXPORT_NAME audio)
	install(DIRECTORY modules/audio/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
	install(TARGETS affrescoui_audio EXPORT AffrescoUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
if(TARGET affrescoui_capi) # the C API: affrescoui::capi, affresco/affresco.h
	set_target_properties(affrescoui_capi PROPERTIES EXPORT_NAME capi)
	install(DIRECTORY modules/capi/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
	install(TARGETS affrescoui_capi EXPORT AffrescoUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(EXPORT AffrescoUITargets NAMESPACE affrescoui:: DESTINATION ${AFFRESCOUI_PACKAGE_DIR})

configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIConfig.cmake.in
	${PROJECT_BINARY_DIR}/AffrescoUIConfig.cmake INSTALL_DESTINATION ${AFFRESCOUI_PACKAGE_DIR})
# 0.x: a minor version may break the API, so find_package(AffrescoUI 0.1) accepts 0.1.* only
write_basic_package_version_file(${PROJECT_BINARY_DIR}/AffrescoUIConfigVersion.cmake
	COMPATIBILITY SameMinorVersion)
install(FILES ${PROJECT_BINARY_DIR}/AffrescoUIConfig.cmake ${PROJECT_BINARY_DIR}/AffrescoUIConfigVersion.cmake
	DESTINATION ${AFFRESCOUI_PACKAGE_DIR})

# Apache-2.0 asks redistributors to pass on the NOTICE file
install(FILES LICENSE NOTICE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
