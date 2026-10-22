# capi: the C API (include/affresco/affresco.h, README.md here), for C hosts and for bindings in other languages. It is
# built on the public API of the library (affresco::Screen, Embedding), plus the render module's headless window and
# manual clock (src/Headless.hpp, not public yet), as automation uses them.
set(affrescoui_capi_sources src/Common.cpp src/Screen.cpp src/Input.cpp)
affrescoui_module(capi ${affrescoui_capi_sources})
target_link_libraries(affrescoui_capi PUBLIC affrescoui::affrescoui)
target_include_directories(affrescoui_capi PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src)
target_include_directories(affrescoui_capi SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)

# The shared library ctypes and other foreign function interfaces load: libaffresco, the whole library in one file,
# exporting the afr_ functions only. The static libraries it holds need position-independent code
# (AFFRESCOUI_BUILD_CAPI_SHARED sets that in CMakeLists.txt).
if(AFFRESCOUI_BUILD_CAPI_SHARED)
	add_library(affrescoui_capi_shared SHARED ${affrescoui_capi_sources})
	add_library(affrescoui::capi_shared ALIAS affrescoui_capi_shared)
	target_include_directories(affrescoui_capi_shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		PRIVATE src ${PROJECT_SOURCE_DIR}/modules/render/src)
	target_include_directories(affrescoui_capi_shared SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
	target_compile_definitions(affrescoui_capi_shared PUBLIC AFR_SHARED PRIVATE AFR_BUILDING)
	target_link_libraries(affrescoui_capi_shared PRIVATE affrescoui::affrescoui)
	target_compile_options(affrescoui_capi_shared PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AFFRESCOUI_WARNINGS}>)
	set_target_properties(affrescoui_capi_shared PROPERTIES OUTPUT_NAME affresco CXX_EXTENSIONS OFF
		CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
		VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
	# Only the afr_ functions: the static dependencies (FreeType, HarfBuzz, GLFW...) stay inside
	if(APPLE)
		target_link_options(affrescoui_capi_shared PRIVATE "LINKER:-exported_symbol,_afr_*")
	elseif(NOT WIN32)
		target_link_options(affrescoui_capi_shared PRIVATE "LINKER:--exclude-libs,ALL")
	endif()
endif()
if(AFFRESCOUI_BUILD_CAPI_SHARED AND SKBUILD)
	# The Python wheel (bindings/python): the library goes inside the package, where the binding looks first.
	install(TARGETS affrescoui_capi_shared LIBRARY DESTINATION affrescoui RUNTIME DESTINATION affrescoui
		COMPONENT python)
endif()
