# The OpenGL backend of the render module (modules/render/README.md, "Backends"):
#
#   gl41   OpenGL 4.1 core through the platform's driver (the default)
#   gles3  OpenGL ES 3.0 through EGL: ANGLE on macOS (Metal) and Windows (Direct3D 11), Mesa on Linux, WebGL2 in a
#          browser (an Emscripten build always uses it)
#
# The renderer calls only what both share, and its shaders start with gl::shaderPrelude(): the backend picks the glad
# loader (external/glad or external/glad/gles3), the context GLFW asks for, and that prelude.
#
# ANGLE needs Chromium's tools to build, so the build doesn't fetch or build it: a developer who has one sets
# AFFRESCOUI_ANGLE_DIR to the folder with its libEGL and libGLESv2, and GLFW loads those two at run time. Without it,
# GLFW loads the system's EGL (Mesa's on Linux).
set(AFFRESCOUI_GL_BACKEND gl41 CACHE STRING "The OpenGL backend: gl41 (OpenGL 4.1 core) or gles3 (OpenGL ES 3.0)")
set_property(CACHE AFFRESCOUI_GL_BACKEND PROPERTY STRINGS gl41 gles3)
if(EMSCRIPTEN)
	set(AFFRESCOUI_GL_BACKEND gles3 CACHE STRING "WebGL2 is OpenGL ES 3.0" FORCE)
endif()
if(NOT AFFRESCOUI_GL_BACKEND MATCHES "^(gl41|gles3)$")
	message(FATAL_ERROR "AFFRESCOUI_GL_BACKEND is '${AFFRESCOUI_GL_BACKEND}': it must be gl41 or gles3")
endif()
# The folders of the backend's glad/glad.h, for code outside the render module that includes render's gl/ headers
# (the unit and window tests). gles3/include comes first: its glad/glad.h; include/ has the KHR header both share.
if(AFFRESCOUI_GL_BACKEND STREQUAL "gles3")
	set(AFFRESCOUI_GLAD_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/external/glad/gles3/include ${PROJECT_SOURCE_DIR}/external/glad/include)
else()
	set(AFFRESCOUI_GLAD_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/external/glad/include)
endif()
set(AFFRESCOUI_ANGLE_DIR "" CACHE PATH "With AFFRESCOUI_GL_BACKEND=gles3: the folder with ANGLE's libEGL and libGLESv2")

# Gives a target the OpenGL loader of the backend, and AFFRESCOUI_GLES3 to the code that must know (src/gl/Shader.cpp,
# src/gl/Context.cpp, src/platform/GlfwPlatform.cpp).
# target: the render module.
function(affrescoui_gl_backend target)
	set(glad ${PROJECT_SOURCE_DIR}/external/glad)
	if(AFFRESCOUI_GL_BACKEND STREQUAL "gles3")
		target_sources(${target} PRIVATE ${glad}/gles3/src/glad.c)
		target_compile_definitions(${target} PRIVATE AFFRESCOUI_GLES3)
	else()
		target_sources(${target} PRIVATE ${glad}/src/glad.c)
	endif()
	target_include_directories(${target} SYSTEM PRIVATE ${AFFRESCOUI_GLAD_INCLUDE_DIRS})
endfunction()

if(AFFRESCOUI_GL_BACKEND STREQUAL "gles3" AND AFFRESCOUI_ANGLE_DIR AND NOT EMSCRIPTEN)
	if(WIN32)
		set(angle_egl libEGL.dll)
		set(angle_gles libGLESv2.dll)
	elseif(APPLE)
		set(angle_egl libEGL.dylib)
		set(angle_gles libGLESv2.dylib)
	else()
		set(angle_egl libEGL.so)
		set(angle_gles libGLESv2.so)
	endif()
	foreach(library ${angle_egl} ${angle_gles})
		if(NOT EXISTS ${AFFRESCOUI_ANGLE_DIR}/${library})
			message(FATAL_ERROR "AFFRESCOUI_ANGLE_DIR has no ${library}: it must be the folder of ANGLE's libraries")
		endif()
	endforeach()
	# GLFW opens EGL and the OpenGL ES library by name at run time; these take the place of the names it would try
	target_compile_definitions(glfw PRIVATE _GLFW_EGL_LIBRARY="${AFFRESCOUI_ANGLE_DIR}/${angle_egl}"
		_GLFW_GLESV2_LIBRARY="${AFFRESCOUI_ANGLE_DIR}/${angle_gles}")
	message(STATUS "AffrescoUI: OpenGL ES 3.0 through ANGLE in ${AFFRESCOUI_ANGLE_DIR}")
endif()
