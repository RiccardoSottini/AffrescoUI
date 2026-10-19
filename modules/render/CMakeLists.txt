# render: the rendering engine. Vendored in external/: glad (OpenGL 4.1 core loader) and stb_truetype (font rasteriser).
affrescoui_module(render
	src/Application.cpp
	src/TextEditor.cpp
	src/Color.cpp
	src/Controls.cpp
	src/Font.cpp
	src/Label.cpp
	src/Panel.cpp
	src/Stack.cpp
	src/TextArea.cpp
	src/Window.cpp
	src/Headless.cpp
	src/platform/GlfwPlatform.cpp
	src/gl/OpenGlRenderer.cpp
	src/gl/Context.cpp
	src/gl/QuadRenderer.cpp
	src/gl/Shader.cpp
	${PROJECT_SOURCE_DIR}/external/glad/src/glad.c
	${PROJECT_SOURCE_DIR}/external/stb/stb_truetype.c)
target_include_directories(affrescoui_render SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/glad/include
	${PROJECT_SOURCE_DIR}/external/stb)
target_link_libraries(affrescoui_render PUBLIC affrescoui::core PRIVATE glfw)
target_compile_definitions(affrescoui_render PRIVATE GLFW_INCLUDE_NONE)
