# export: panel trees written as files, for now vector PDF (README.md here). Fonts are subset with HarfBuzz's
# subsetter, built from the same pinned HarfBuzz release as the shaper (cmake/AffrescoUIText.cmake): no new dependency.
set(affrescoui_harfbuzz_subset_files graph/gsubgpos-context.cc hb-subset-cff-common.cc hb-subset-cff1.cc
	hb-subset-cff2.cc hb-subset-input.cc hb-subset-instancer-iup.cc hb-subset-instancer-solver.cc
	hb-subset-plan-layout.cc hb-subset-plan-var.cc hb-subset-plan.cc hb-subset-serialize.cc hb-subset-table-cff.cc
	hb-subset-table-color.cc hb-subset-table-layout.cc hb-subset-table-other.cc hb-subset-table-var.cc hb-subset.cc)
list(TRANSFORM affrescoui_harfbuzz_subset_files PREPEND ${harfbuzz_SOURCE_DIR}/src/)
add_library(affrescoui_harfbuzz_subset STATIC ${affrescoui_harfbuzz_subset_files})
target_link_libraries(affrescoui_harfbuzz_subset PUBLIC affrescoui_harfbuzz)
set_target_properties(affrescoui_harfbuzz_subset PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF
	COMPILE_WARNING_AS_ERROR OFF)

affrescoui_module(export
	src/PdfDocument.cpp
	src/PdfFonts.cpp
	src/PdfObjects.cpp
	src/PdfPainter.cpp
	src/PdfResources.cpp
	src/PdfShapes.cpp)
target_link_libraries(affrescoui_export PUBLIC affrescoui::render PRIVATE affrescoui_harfbuzz_subset)
