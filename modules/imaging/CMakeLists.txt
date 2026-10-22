# imaging: images as sparse copy-on-write tiles, layers, blending, selections, brushes, filters, colour management and
# undo by tile differences, for image editors (README.md here). Uses render (GpuSurface, for compositing on the GPU).
# Dependencies: stb_image (cmake/AffrescoUIImages.cmake) to decode, stb_image_write to encode, lcms2
# (cmake/AffrescoUIColor.cmake) for ICC profiles.
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIImages.cmake) # stb_image, and the stb commit
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIColor.cmake)
# stb_image_write 1.16, from the same pinned stb commit as stb_image: PNG and JPEG encoding, and the deflate of
# project tiles. One header, downloaded and checked by its hash
FetchContent_Declare(stb_image_write
	URL https://raw.githubusercontent.com/nothings/stb/${AFFRESCOUI_STB_IMAGE_COMMIT}/stb_image_write.h
	URL_HASH SHA256=cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05
	DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(stb_image_write)
affrescoui_module(imaging
	# tiles: sparse copy-on-write tiles, the Image grid of them, and GPU residency of their textures
	src/tiles/Tile.cpp src/tiles/Image.cpp src/tiles/Residency.cpp
	# document: layers, groups and masks, and undo by tile differences
	src/document/Document.cpp src/document/History.cpp
	# compose: blend modes, adjustment layers, and the CPU compositor
	src/compose/Blend.cpp src/compose/Adjustment.cpp src/compose/Composite.cpp
	# paint: rasterized shapes, selections, brush strokes and gradients
	src/paint/Rasterize.cpp src/paint/Selection.cpp src/paint/Brush.cpp src/paint/Gradient.cpp
	# filters: convolution kernels and the filters built on them
	src/filters/Kernels.cpp src/filters/Filter.cpp
	# io: colour profiles, PNG and JPEG codecs (stb), and the project file format
	src/io/Color.cpp src/io/Stb.cpp src/io/Codec.cpp src/io/ProjectJson.cpp src/io/Project.cpp
	# gpu: the canvas shaders and the CanvasSurface that composites on the GPU
	src/gpu/CanvasShaders.cpp src/gpu/Canvas.cpp
	${AFFRESCOUI_LCMS2_SOURCES})
target_include_directories(affrescoui_imaging SYSTEM PRIVATE ${stb_image_SOURCE_DIR} ${stb_image_write_SOURCE_DIR}
	${AFFRESCOUI_LCMS2_INCLUDE_DIR} ${AFFRESCOUI_GLAD_INCLUDE_DIRS})
target_link_libraries(affrescoui_imaging PUBLIC affrescoui::core affrescoui::render)
