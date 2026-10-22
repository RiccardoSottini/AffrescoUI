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
	src/Tile.cpp
	src/Image.cpp
	src/History.cpp src/Blend.cpp src/Adjustment.cpp src/Document.cpp src/Composite.cpp src/Rasterize.cpp src/Selection.cpp src/Kernels.cpp src/Filter.cpp src/Brush.cpp
	src/Gradient.cpp src/Color.cpp src/Stb.cpp src/Codec.cpp src/ProjectJson.cpp src/Project.cpp
	${AFFRESCOUI_LCMS2_SOURCES})
target_include_directories(affrescoui_imaging SYSTEM PRIVATE ${stb_image_SOURCE_DIR} ${stb_image_write_SOURCE_DIR}
	${AFFRESCOUI_LCMS2_INCLUDE_DIR} ${AFFRESCOUI_GLAD_INCLUDE_DIRS})
target_link_libraries(affrescoui_imaging PUBLIC affrescoui::core affrescoui::render)
