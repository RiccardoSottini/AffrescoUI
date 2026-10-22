# render: the rendering engine. Vendored in external/: glad (the OpenGL 4.1 core and OpenGL ES 3.0 loaders, one per
# backend: cmake/AffrescoUIAngle.cmake). Text: FreeType and HarfBuzz (cmake/AffrescoUIText.cmake). Images: stb_image
# (cmake/AffrescoUIImages.cmake).
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIImages.cmake)
affrescoui_module(render
	src/Bitmap.cpp
	src/ImageCache.cpp
	src/text/PieceTree.cpp
	src/Color.cpp
	src/ColorSpaces.cpp
	src/Font.cpp
	src/FontCache.cpp
	src/text/FontFace.cpp
	src/text/FontOutlines.cpp
	src/text/GlyphAtlas.cpp
	src/text/LookupSpans.cpp
	src/text/PlainText.cpp
	src/text/Shaping.cpp
	src/RichText.cpp
	src/layout/Flex.cpp
	src/layout/Grid.cpp
	src/WindowImage.cpp
	src/Headless.cpp
	src/GeometryCache.cpp
	src/PaintRecorder.cpp
	src/vector/VectorPath.cpp
	src/vector/PathFill.cpp
	src/vector/PathStroke.cpp
	src/vector/Svg.cpp
	src/vector/SvgStyle.cpp
	src/vector/SvgXml.cpp
	src/vector/MeshRasterizer.cpp
	src/SpatialIndex.cpp
	src/Snapping.cpp
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

# widgets: the views, the windows and the platform, one line per sub-folder of src/
# The window, the application loop and the accessibility tree they publish
target_sources(affrescoui_render PRIVATE src/window/Accessibility.cpp src/window/Application.cpp src/window/Window.cpp)
# The panel tree: layout, hit tests, transforms; and the stack
target_sources(affrescoui_render PRIVATE src/panel/Panel.cpp src/panel/Stack.cpp)
# The small widgets: labels, fields, controls, dates, tabs, splits, images and paths
target_sources(affrescoui_render PRIVATE src/widgets/Calendar.cpp src/widgets/Carousel.cpp src/widgets/CivilDate.cpp
	src/widgets/CodeInput.cpp src/widgets/Controls.cpp src/widgets/ImageView.cpp src/widgets/Label.cpp
	src/widgets/PathView.cpp src/widgets/Split.cpp src/widgets/Tabs.cpp src/widgets/TextArea.cpp)
# The virtualised rows: lists, trees and tables
target_sources(affrescoui_render PRIVATE src/lists/ListView.cpp src/lists/RowWindow.cpp src/lists/TableView.cpp
	src/lists/TreeView.cpp)
# The code editor, its text model and the diff view
target_sources(affrescoui_render PRIVATE src/editor/DiffView.cpp src/editor/TextEditor.cpp
	src/editor/TextEditorCursors.cpp src/editor/TextEditorFind.cpp src/editor/TextEditorMinimap.cpp
	src/editor/TextEditorPaint.cpp src/editor/TextEditorPaintView.cpp src/editor/TextEditorView.cpp
	src/editor/TextModel.cpp)
# The colour picker, the curves and the gradient editors
target_sources(affrescoui_render PRIVATE src/creative/ColorPicker.cpp src/creative/ColorPickerPaint.cpp
	src/creative/CurvesEditor.cpp src/creative/GradientEditor.cpp)
# The infinite canvas and its rulers
target_sources(affrescoui_render PRIVATE src/canvas/CanvasView.cpp src/canvas/CanvasViewInput.cpp src/canvas/Rulers.cpp)
# The node graph
target_sources(affrescoui_render PRIVATE src/graph/GraphView.cpp)
# The timeline
target_sources(affrescoui_render PRIVATE src/timeline/TimelineView.cpp src/timeline/TimelineInput.cpp
	src/timeline/TimelineKeys.cpp src/timeline/TimelineMath.cpp src/timeline/TimelinePaint.cpp)
# The desktop (GLFW) and embedded platforms; the per-OS parts are above
target_sources(affrescoui_render PRIVATE src/platform/GlfwPlatform.cpp src/platform/EmbeddedPlatform.cpp
	src/platform/EmbeddedPlatformGl.cpp)
