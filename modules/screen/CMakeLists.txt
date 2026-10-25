# screen: affresco::Screen, which puts the other modules together. Its target, affrescoui, is the whole library.
add_library(affrescoui
	# The screen itself (its modules and peers, its channels, its version control), and the PNG encoder other modules
	# share
	src/Screen.cpp
	src/ScreenImpl.cpp
	src/ScreenChannels.cpp
	src/ScreenVersions.cpp
	src/ScreenData.cpp
	src/Png.cpp
	# pictures: drawing off screen, the evidence a proposal carries, and the inspector's picture of a screen
	src/pictures/OffscreenImages.cpp
	src/pictures/ProposalEvidence.cpp
	src/pictures/Inspector.cpp
	# windows: a screen inside another, and a screen's second window
	src/windows/EmbeddedScreen.cpp
	src/windows/SecondWindow.cpp
	# features: the service that proves and installs compositions across apps
	src/features/FeaturesService.cpp)
add_library(affrescoui::affrescoui ALIAS affrescoui)
target_include_directories(affrescoui PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/screen/include>)
set_target_properties(affrescoui PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(affrescoui PRIVATE ${AFFRESCOUI_WARNINGS})
# The features service proves compositions across apps (CompositionProof.hpp) and submits their install revisions
target_include_directories(affrescoui PRIVATE $<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>
	${PROJECT_SOURCE_DIR}/modules/vcs/src
	${PROJECT_SOURCE_DIR}/modules/screen/src) # its own private headers, from every sub-folder
target_link_libraries(affrescoui PUBLIC affrescoui::core affrescoui::render affrescoui::format affrescoui::vcs
	affrescoui::channel affrescoui::net affrescoui::services affrescoui::data affrescoui::themes)
# The audio engine screens play on (Screen::audio()), when the audio module is built
if(AFFRESCOUI_AUDIO)
	target_sources(affrescoui PRIVATE src/ScreenAudio.cpp)
	target_link_libraries(affrescoui PUBLIC affrescoui::audio)
else()
	target_sources(affrescoui PRIVATE src/ScreenAudioNone.cpp)
endif()
