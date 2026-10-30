# The rendering check (README.md): affrescoui_render_runner plays the scenarios in scenarios/*.json with the automation
# module and compares every picture with its golden image; check.py runs it. The runner is built next to the other
# programs, where check.py looks.
# It grants Forge's workspace (examples/forge/json/app.json) with forge/folder (a small fixed folder) open, a shell and git,
# and trusts the example catalogue's publisher.
cmake_language(DEFER CALL affrescoui_hide_test_windows) # AFFRESCOUI_HIDDEN=1 for its tests
set(forge_dir ${PROJECT_SOURCE_DIR}/examples/forge/json)
add_executable(affrescoui_render_runner runner.cpp ${CMAKE_CURRENT_SOURCE_DIR}/surfaces/Surfaces.cpp)
# surfaces/Surfaces.hpp here
target_include_directories(affrescoui_render_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# The example surfaces, and a failing one, draw with OpenGL (surfaces.json)
target_include_directories(affrescoui_render_runner SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/glad/include)
target_compile_definitions(affrescoui_render_runner PRIVATE
	AFFRESCOUI_FORGE_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/forge/folder" AFFRESCOUI_FORGE_DIRECTORY="${forge_dir}")
target_link_libraries(affrescoui_render_runner PRIVATE affrescoui::automation)
target_compile_options(affrescoui_render_runner PRIVATE ${AFFRESCOUI_WARNINGS})
set_target_properties(affrescoui_render_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
	add_test(NAME rendering COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check.py --build ${PROJECT_BINARY_DIR}
	         WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

# The templates' own scenarios (in their folders, examples/templates.json), Forge's, Modello's, Sinopia's and Tempera's: what each one
# does, checked headless, without a window or golden images; and the rendering check's screens'
# (screens/*.scenarios.json): where layout, images, icons and partial redraws put each node, held while they get faster
affrescoui_template_folders(template_folders)
list(TRANSFORM template_folders PREPEND ${PROJECT_SOURCE_DIR}/examples/ OUTPUT_VARIABLE template_globs)
list(TRANSFORM template_globs APPEND /*.scenarios.json)
file(GLOB AFFRESCOUI_TEMPLATE_SCENARIOS CONFIGURE_DEPENDS ${template_globs}
	${forge_dir}/*.scenarios.json ${CMAKE_CURRENT_SOURCE_DIR}/screens/*.scenarios.json
	${PROJECT_SOURCE_DIR}/examples/modello/json/*.scenarios.json ${PROJECT_SOURCE_DIR}/examples/sinopia/json/*.scenarios.json
	${PROJECT_SOURCE_DIR}/examples/tempera/json/*.scenarios.json)
if(AFFRESCOUI_TEMPLATE_SCENARIOS)
	# Their captures (pictures they take for the gallery, not compared) go to the build folder
	file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/scenario-captures)
	# A test per file (scenarios/<folder>/<name>, all matched by ctest -R scenarios), so ctest --parallel spreads them:
	# under the sanitizers, one after another, they take longer than a test and the job may run
	foreach(file IN LISTS AFFRESCOUI_TEMPLATE_SCENARIOS)
		cmake_path(GET file PARENT_PATH folder)
		cmake_path(GET folder FILENAME name)
		if(name STREQUAL "json") # an example's JSON folder: named after the example
			cmake_path(GET folder PARENT_PATH folder)
		endif()
		cmake_path(GET folder FILENAME folder)
		string(REGEX REPLACE "\\.scenarios\\.json$" "" name "${file}")
		cmake_path(GET name FILENAME name)
		add_test(NAME scenarios/${folder}/${name} COMMAND affrescoui_render_runner --out
		         ${PROJECT_BINARY_DIR}/scenario-captures ${file})
	endforeach()
endif()
