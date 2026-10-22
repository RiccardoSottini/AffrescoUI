# render: the rendering engine. Vendored in external/: glad (the OpenGL 4.1 core and OpenGL ES 3.0 loaders, one per
# backend: cmake/AffrescoUIAngle.cmake). Text: FreeType and HarfBuzz (cmake/AffrescoUIText.cmake). Images: stb_image
# (cmake/AffrescoUIImages.cmake).
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIImages.cmake)
affrescoui_module(render
	src/Accessibility.cpp
	src/Application.cpp
	src/Bitmap.cpp
	src/ImageCache.cpp
	src/ImageView.cpp
	src/DiffView.cpp
	src/TextEditor.cpp
	src/TextEditorCursors.cpp
	src/TextEditorFind.cpp
	src/TextEditorPaint.cpp
	src/TextEditorPaintView.cpp
	src/TextEditorView.cpp
	src/TextModel.cpp
	src/text/PieceTree.cpp
	src/Color.cpp
	src/ColorSpaces.cpp
	src/ColorPicker.cpp
	src/ColorPickerPaint.cpp
	src/CurvesEditor.cpp
	src/GradientEditor.cpp
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
	src/text/FontOutlines.cpp
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
	src/WindowImage.cpp
	src/Headless.cpp
	src/GeometryCache.cpp
	src/PaintRecorder.cpp
	src/PathView.cpp
	src/GraphView.cpp
	src/TimelineView.cpp
	src/vector/VectorPath.cpp
	src/vector/PathFill.cpp
	src/vector/PathStroke.cpp
	src/vector/Svg.cpp
	src/vector/SvgStyle.cpp
	src/vector/SvgXml.cpp
	src/vector/MeshRasterizer.cpp
	src/CanvasView.cpp
	src/CanvasViewInput.cpp
	src/SpatialIndex.cpp
	src/Snapping.cpp
	src/Rulers.cpp
	src/platform/GlfwPlatform.cpp
	src/platform/EmbeddedPlatform.cpp
	src/platform/EmbeddedPlatformGl.cpp
	src/gl/OpenGlRenderer.cpp
	src/gl/Context.cpp
	src/gl/ImageTextures.cpp
	src/gl/QuadRenderer.cpp
	src/gl/QuadRendererCache.cpp
	src/gl/SubtreeTextures.cpp
	src/gl/Shader.cpp
	src/gl/Stencil.cpp
	src/gl/SurfaceGuard.cpp
	src/gl/SurfacePrelude.cpp
	src/gl/Surfaces.cpp
	src/gl/SurfaceView.cpp)
target_include_directories(affrescoui_render SYSTEM PRIVATE ${stb_image_SOURCE_DIR})
affrescoui_gl_backend(affrescoui_render) # the glad loader of AFFRESCOUI_GL_BACKEND (cmake/AffrescoUIAngle.cmake)
# nlohmann_json is public: GpuSurface.hpp hands a surface its document parameters as JSON
target_link_libraries(affrescoui_render PUBLIC affrescoui::core nlohmann_json::nlohmann_json PRIVATE glfw freetype
	affrescoui_harfbuzz affrescoui_regex)
target_compile_definitions(affrescoui_render PRIVATE GLFW_INCLUDE_NONE)
# What GLFW doesn't do (the colour scheme, native menu bars), per OS: Cocoa on macOS, Win32 on Windows, none elsewhere
if(APPLE)
	target_sources(affrescoui_render PRIVATE src/platform/DesktopMac.mm)
	set_source_files_properties(src/platform/DesktopMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
	target_link_libraries(affrescoui_render PRIVATE "-framework Cocoa")
elseif(WIN32)
	target_sources(affrescoui_render PRIVATE src/platform/DesktopWin32.cpp)
	target_link_libraries(affrescoui_render PRIVATE user32 advapi32)
else()
	target_sources(affrescoui_render PRIVATE src/platform/DesktopOther.cpp)
endif()
# The accessibility tree for screen readers, through AccessKit (cmake/AffrescoUIAccessibility.cmake), or none
if(TARGET affrescoui_accesskit)
	target_sources(affrescoui_render PRIVATE src/platform/AccessKitTree.cpp)
	target_link_libraries(affrescoui_render PRIVATE affrescoui_accesskit)
	if(APPLE)
		target_sources(affrescoui_render PRIVATE src/platform/AccessibilityMac.mm)
		set_source_files_properties(src/platform/AccessibilityMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
	elseif(WIN32)
		target_sources(affrescoui_render PRIVATE src/platform/AccessibilityWindows.cpp)
	else()
		target_sources(affrescoui_render PRIVATE src/platform/AccessibilityLinux.cpp)
	endif()
else()
	target_sources(affrescoui_render PRIVATE src/platform/AccessibilityNone.cpp)
endif()
# stb_image's JPEG decoder lets signed integers overflow on crafted input (found by the image fuzz target): wrapping
# is what it expects, so the one file that compiles it wraps, and the sanitizers don't stop there
if(NOT MSVC)
	set_source_files_properties(src/Bitmap.cpp PROPERTIES COMPILE_OPTIONS "-fwrapv;-fno-sanitize=signed-integer-overflow")
endif()
# File dialogs (open, save, choose a folder): nativefiledialog-extended, compiled in (cmake/AffrescoUIDialogs.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIDialogs.cmake)
target_sources(affrescoui_render PRIVATE src/platform/FileDialogs.cpp)
affrescoui_file_dialogs(affrescoui_render)
