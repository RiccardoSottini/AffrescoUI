# The rendering check (README.md): affrescoui_render_runner plays the scenarios in scenarios/*.json with the automation
# module and compares every picture with its golden image; check.py runs it. The runner is built next to the other
# programs, where check.py looks.
# It registers what affrescoui_host and affrescoui_forge register: the code editor and Forge's natives, which see
# forge/folder (a small fixed folder) open.
set(forge_dir ${PROJECT_SOURCE_DIR}/examples/forge)
add_executable(affrescoui_render_runner runner.cpp ${PROJECT_SOURCE_DIR}/examples/CodeEditor.cpp ${forge_dir}/Forge.cpp
	${forge_dir}/features/palette/FileIndex.cpp ${forge_dir}/features/themes/ThemeImport.cpp
	${forge_dir}/features/themes/IconTheme.cpp ${forge_dir}/features/explorer/Explorer.cpp
	${forge_dir}/features/terminal/Terminal.cpp ${forge_dir}/features/terminal/TerminalModel.cpp
	${forge_dir}/features/editor/Buffers.cpp ${forge_dir}/features/editor/Editor.cpp
	${forge_dir}/features/search/Search.cpp
	${forge_dir}/features/scm/Scm.cpp ${forge_dir}/features/scm/GitStatus.cpp
	${forge_dir}/features/scm/GitArguments.cpp ${forge_dir}/features/scm/LineDiff.cpp
	${PROJECT_SOURCE_DIR}/examples/surfaces/Surfaces.cpp)
target_include_directories(affrescoui_render_runner PRIVATE ${PROJECT_SOURCE_DIR}/examples ${forge_dir})
# The example surfaces, and a failing one, draw with OpenGL (surfaces.json)
target_include_directories(affrescoui_render_runner SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/glad/include)
target_compile_definitions(affrescoui_render_runner PRIVATE
	AFFRESCOUI_FORGE_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/forge/folder" AFFRESCOUI_FORGE_DIRECTORY="${forge_dir}")
target_link_libraries(affrescoui_render_runner PRIVATE affrescoui::automation affrescoui_vterm)
target_compile_options(affrescoui_render_runner PRIVATE ${AFFRESCOUI_WARNINGS})
set_target_properties(affrescoui_render_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
	add_test(NAME rendering COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check.py --build ${PROJECT_BINARY_DIR}
	         WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

# The templates' own scenarios (examples/assets/screens/*.scenarios.json) and Forge's (examples/forge): what each one
# does, checked headless, without a window or golden images; and the rendering check's screens'
# (screens/*.scenarios.json): where layout, images, icons and partial redraws put each node, held while they get faster
file(GLOB AFFRESCOUI_TEMPLATE_SCENARIOS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/examples/assets/screens/*.scenarios.json
	${forge_dir}/*.scenarios.json ${CMAKE_CURRENT_SOURCE_DIR}/screens/*.scenarios.json)
if(AFFRESCOUI_TEMPLATE_SCENARIOS)
	add_test(NAME scenarios COMMAND affrescoui_render_runner ${AFFRESCOUI_TEMPLATE_SCENARIOS})
endif()
