# render: the rendering engine. Vendored in external/: glad (OpenGL 4.1 core loader). Text: FreeType and HarfBuzz
# (cmake/AffrescoUIText.cmake).
affrescoui_module(render
	src/Application.cpp
	src/TextEditor.cpp
	src/Color.cpp
	src/Controls.cpp
	src/Font.cpp
	src/FontCache.cpp
	src/text/FontFace.cpp
	src/text/GlyphAtlas.cpp
	src/text/Shaping.cpp
	src/Label.cpp
	src/Panel.cpp
	src/Stack.cpp
	src/layout/Flex.cpp
	src/layout/Grid.cpp
	src/TextArea.cpp
	src/Window.cpp
	src/Headless.cpp
	src/GeometryCache.cpp
	src/PaintRecorder.cpp
	src/platform/GlfwPlatform.cpp
	src/gl/OpenGlRenderer.cpp
	src/gl/Context.cpp
	src/gl/QuadRenderer.cpp
	src/gl/Shader.cpp
	${PROJECT_SOURCE_DIR}/external/glad/src/glad.c)
target_include_directories(affrescoui_render SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/glad/include)
target_link_libraries(affrescoui_render PUBLIC affrescoui::core PRIVATE glfw freetype affrescoui_harfbuzz)
target_compile_definitions(affrescoui_render PRIVATE GLFW_INCLUDE_NONE)
