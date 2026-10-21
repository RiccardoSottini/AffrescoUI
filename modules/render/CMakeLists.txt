# render: the rendering engine. Vendored in external/: glad (OpenGL 4.1 core loader). Text: FreeType and HarfBuzz
# (cmake/AffrescoUIText.cmake). Images: stb_image (cmake/AffrescoUIImages.cmake).
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIImages.cmake)
affrescoui_module(render
	src/Application.cpp
	src/Bitmap.cpp
	src/ImageCache.cpp
	src/ImageView.cpp
	src/TextEditor.cpp
	src/Color.cpp
	src/Controls.cpp
	src/Split.cpp
	src/Tabs.cpp
	src/RowWindow.cpp
	src/ListView.cpp
	src/TreeView.cpp
	src/TableView.cpp
	src/Font.cpp
	src/FontCache.cpp
	src/text/FontFace.cpp
	src/text/GlyphAtlas.cpp
	src/text/LookupSpans.cpp
	src/text/PlainText.cpp
	src/text/Shaping.cpp
	src/Label.cpp
	src/RichText.cpp
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
	src/gl/ImageTextures.cpp
	src/gl/QuadRenderer.cpp
	src/gl/Shader.cpp
	${PROJECT_SOURCE_DIR}/external/glad/src/glad.c)
target_include_directories(affrescoui_render SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/glad/include
	${stb_image_SOURCE_DIR})
target_link_libraries(affrescoui_render PUBLIC affrescoui::core PRIVATE glfw freetype affrescoui_harfbuzz)
target_compile_definitions(affrescoui_render PRIVATE GLFW_INCLUDE_NONE)
# stb_image's JPEG decoder lets signed integers overflow on crafted input (found by the image fuzz target): wrapping
# is what it expects, so the one file that compiles it wraps, and the sanitizers don't stop there
if(NOT MSVC)
	set_source_files_properties(src/Bitmap.cpp PROPERTIES COMPILE_OPTIONS "-fwrapv;-fno-sanitize=signed-integer-overflow")
endif()
