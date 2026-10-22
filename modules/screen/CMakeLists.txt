# screen: affresco::Screen, which puts the other modules together. Its target, affrescoui, is the whole library.
add_library(affrescoui
	src/Screen.cpp
	src/OffscreenImages.cpp
	src/FeaturesService.cpp
	src/ProposalEvidence.cpp
	src/Inspector.cpp
	src/Png.cpp
	src/EmbeddedScreen.cpp
	src/SecondWindow.cpp)
add_library(affrescoui::affrescoui ALIAS affrescoui)
target_include_directories(affrescoui PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/screen/include>)
set_target_properties(affrescoui PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(affrescoui PRIVATE ${AFFRESCOUI_WARNINGS})
# The features service proves compositions across apps (CompositionProof.hpp) and submits their install revisions
target_include_directories(affrescoui PRIVATE $<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>
	${PROJECT_SOURCE_DIR}/modules/vcs/src)
target_link_libraries(affrescoui PUBLIC affrescoui::core affrescoui::render affrescoui::format affrescoui::vcs
	affrescoui::channel affrescoui::net affrescoui::services)
