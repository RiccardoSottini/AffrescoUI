# automation: scenarios, the scripted checks a screen ships with (README.md here). It plays them through the public
# API of the library (affresco::Screen and Window), plus two internals it reaches on purpose: the render module's
# headless window and manual clock (src/Headless.hpp, not public yet), and the screen module's PNG encoder and scratch
# folder (src/Png.hpp, src/ScratchFolder.hpp).
affrescoui_module(automation
	src/Names.cpp
	src/ScenarioFile.cpp
	src/Player.cpp
	src/Expectations.cpp
	src/Pictures.cpp
	src/PngReader.cpp
	src/Run.cpp)
target_link_libraries(affrescoui_automation PUBLIC affrescoui::affrescoui)
target_include_directories(affrescoui_automation PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src
	${PROJECT_SOURCE_DIR}/modules/screen/src)
