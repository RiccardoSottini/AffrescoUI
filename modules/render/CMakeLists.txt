# render: the rendering engine. Vendored in external/: glad (the OpenGL 4.1 core and OpenGL ES 3.0 loaders, one per
# backend: cmake/AffrescoUIAngle.cmake). Text: FreeType and HarfBuzz (cmake/AffrescoUIText.cmake). Images: stb_image
# (cmake/AffrescoUIImages.cmake).
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIImages.cmake)
affrescoui_module(render
	# paint/: what a frame records per panel and replays, behind the Renderer interface
	src/paint/PaintRecorder.cpp
	src/paint/GeometryCache.cpp
	src/paint/WindowImage.cpp
	# gl/: the OpenGL backend that replays recorded geometry as batched quads
	src/gl/OpenGlRenderer.cpp
	src/gl/Context.cpp
	src/gl/ImageTextures.cpp
	src/gl/QuadRenderer.cpp
	src/gl/QuadRendererCache.cpp
	src/gl/SubtreeTextures.cpp
	src/gl/Shader.cpp
	src/gl/Stencil.cpp
	# gl/surfaces/: panels that draw with their own GL code, inside guarded state
	src/gl/surfaces/SurfaceGuard.cpp
	src/gl/surfaces/SurfacePrelude.cpp
	src/gl/surfaces/Surfaces.cpp
	src/gl/surfaces/SurfaceView.cpp
	# font/: font files, faces, glyph lookups and outlines, the glyph atlas and the font cache
	src/font/Font.cpp
	src/font/FontCache.cpp
	src/font/FontFace.cpp
	src/font/FontOutlines.cpp
	src/font/GlyphAtlas.cpp
	src/font/LookupSpans.cpp
	# text/: shaping, line layout, rich text and the piece tree that stores long texts
	src/text/PieceTree.cpp
	src/text/PlainText.cpp
	src/text/Shaping.cpp
	src/text/RichText.cpp
	# vector/: paths, their fill and stroke, SVG and the mesh rasterizer
	src/vector/VectorPath.cpp
	src/vector/PathFill.cpp
	src/vector/PathStroke.cpp
	src/vector/Svg.cpp
	src/vector/SvgStyle.cpp
	src/vector/SvgXml.cpp
	src/vector/MeshRasterizer.cpp
	src/vector/GradientFill.cpp
	# layout/: the flex and grid layouts of panels
	src/layout/Flex.cpp
	src/layout/Grid.cpp
	# image/: image decoding and caching, colours and colour spaces
	src/image/Bitmap.cpp
	src/image/ImageCache.cpp
	src/image/Color.cpp
	src/image/ColorSpaces.cpp
	# spatial/: the spatial index and snapping of canvases
	src/spatial/SpatialIndex.cpp
	src/spatial/Snapping.cpp
	# headless rendering: a window drawn offscreen, for tests, screenshots and agents
	src/Headless.cpp)
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
	set_source_files_properties(src/image/Bitmap.cpp PROPERTIES COMPILE_OPTIONS "-fwrapv;-fno-sanitize=signed-integer-overflow")
endif()
# File dialogs (open, save, choose a folder): nativefiledialog-extended, compiled in (cmake/AffrescoUIDialogs.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIDialogs.cmake)
target_sources(affrescoui_render PRIVATE src/platform/FileDialogs.cpp)
affrescoui_file_dialogs(affrescoui_render)

# widgets: the views, the windows and the platform, one line per sub-folder of src/
# The window, the application loop and the accessibility tree they publish
target_sources(affrescoui_render PRIVATE src/window/Accessibility.cpp src/window/Application.cpp src/window/Window.cpp
	src/window/WindowInput.cpp)
# The panel tree: layout, hit tests, transforms; and the stack
target_sources(affrescoui_render PRIVATE src/panel/Panel.cpp src/panel/PanelLayout.cpp src/panel/Stack.cpp)
# The small widgets: labels, fields, controls, dates, tabs, splits, images and paths
target_sources(affrescoui_render PRIVATE src/widgets/Calendar.cpp src/widgets/Carousel.cpp src/widgets/CivilDate.cpp
	src/widgets/CodeInput.cpp src/widgets/Controls.cpp src/widgets/ImageView.cpp src/widgets/Label.cpp
	src/widgets/PathView.cpp src/widgets/Split.cpp src/widgets/Tabs.cpp src/widgets/TextArea.cpp)
# The virtualised rows: lists, trees and tables
target_sources(affrescoui_render PRIVATE src/lists/ListView.cpp src/lists/RowWindow.cpp src/lists/TableView.cpp
	src/lists/TreeView.cpp)
# The code editor, its text model and the diff view
target_sources(affrescoui_render PRIVATE src/editor/DiffView.cpp src/editor/TextEditor.cpp
	src/editor/TextEditorCursors.cpp src/editor/TextEditorFind.cpp src/editor/TextEditorInput.cpp
	src/editor/TextEditorMinimap.cpp src/editor/TextEditorPaint.cpp src/editor/TextEditorPaintView.cpp
	src/editor/TextEditorView.cpp src/editor/TextModel.cpp)
# The colour picker, the curves and the gradient editors
target_sources(affrescoui_render PRIVATE src/creative/ColorPicker.cpp src/creative/ColorPickerPaint.cpp
	src/creative/CurvesEditor.cpp src/creative/GradientEditor.cpp)
# The infinite canvas and its rulers
target_sources(affrescoui_render PRIVATE src/canvas/CanvasView.cpp src/canvas/CanvasViewInput.cpp src/canvas/Rulers.cpp)
# The node graph
target_sources(affrescoui_render PRIVATE src/graph/GraphView.cpp src/graph/GraphModel.cpp src/graph/GraphInput.cpp
	src/graph/GraphPaint.cpp)
# The timeline
target_sources(affrescoui_render PRIVATE src/timeline/TimelineView.cpp src/timeline/TimelineInput.cpp
	src/timeline/TimelineKeys.cpp src/timeline/TimelineMath.cpp src/timeline/TimelinePaint.cpp)
# The desktop (GLFW) and embedded platforms; the per-OS parts are above
target_sources(affrescoui_render PRIVATE src/platform/GlfwPlatform.cpp src/platform/EmbeddedPlatform.cpp
	src/platform/EmbeddedPlatformGl.cpp)
