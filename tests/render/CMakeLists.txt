# The rendering check (README.md): affrescoui_render_runner plays the scenarios in scenarios/*.json with the automation
# module and compares every picture with its golden image; check.py runs it. The runner is built next to the other
# programs, where check.py looks.
# It registers what affrescoui_launcher and affrescoui_forge register: the code editor and Forge's natives, which see
# forge/folder (a small fixed folder) open.
set(forge_dir ${PROJECT_SOURCE_DIR}/examples/forge)
add_executable(affrescoui_render_runner runner.cpp ${PROJECT_SOURCE_DIR}/examples/shared/CodeEditor.cpp ${forge_dir}/Forge.cpp
	${forge_dir}/features/palette/FileIndex.cpp ${forge_dir}/features/themes/ThemeImport.cpp
	${forge_dir}/features/themes/IconTheme.cpp ${forge_dir}/features/explorer/Explorer.cpp
	${forge_dir}/features/terminal/Terminal.cpp ${forge_dir}/features/terminal/TerminalModel.cpp
	${forge_dir}/features/editor/Buffers.cpp ${forge_dir}/features/editor/Editor.cpp
	${forge_dir}/features/search/Search.cpp
	${forge_dir}/features/scm/Scm.cpp ${forge_dir}/features/scm/GitStatus.cpp
	${forge_dir}/features/scm/GitArguments.cpp ${forge_dir}/features/scm/LineDiff.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/surfaces/Surfaces.cpp
	${forge_dir}/features/debug/Debug.cpp)
target_sources(affrescoui_render_runner PRIVATE ${forge_dir}/features/lsp/Lsp.cpp ${forge_dir}/features/lsp/LspCall.cpp
	${forge_dir}/features/lsp/LspEdits.cpp ${forge_dir}/features/lsp/LspProtocol.cpp ${forge_dir}/features/lsp/LspText.cpp)
target_sources(affrescoui_render_runner PRIVATE ${forge_dir}/features/terminal/TerminalSessions.cpp
	${forge_dir}/features/terminal/TerminalView.cpp)
# surfaces/Surfaces.hpp here, CodeEditor.hpp and the shared headers in examples/shared
target_include_directories(affrescoui_render_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/examples
	${PROJECT_SOURCE_DIR}/examples/shared ${forge_dir})
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

# The templates' own scenarios (in their folders, examples/templates.json), Forge's, Modello's, Sinopia's and Tempera's: what each one
# does, checked headless, without a window or golden images; and the rendering check's screens'
# (screens/*.scenarios.json): where layout, images, icons and partial redraws put each node, held while they get faster
affrescoui_template_folders(template_folders)
list(TRANSFORM template_folders PREPEND ${PROJECT_SOURCE_DIR}/examples/ OUTPUT_VARIABLE template_globs)
list(TRANSFORM template_globs APPEND /*.scenarios.json)
file(GLOB AFFRESCOUI_TEMPLATE_SCENARIOS CONFIGURE_DEPENDS ${template_globs}
	${forge_dir}/*.scenarios.json ${CMAKE_CURRENT_SOURCE_DIR}/screens/*.scenarios.json
	${PROJECT_SOURCE_DIR}/examples/modello/*.scenarios.json ${PROJECT_SOURCE_DIR}/examples/sinopia/*.scenarios.json
	${PROJECT_SOURCE_DIR}/examples/tempera/*.scenarios.json)
if(AFFRESCOUI_TEMPLATE_SCENARIOS)
	# Their captures (pictures they take for the gallery, not compared) go to the build folder
	file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/scenario-captures)
	add_test(NAME scenarios COMMAND affrescoui_render_runner --out ${PROJECT_BINARY_DIR}/scenario-captures
	         ${AFFRESCOUI_TEMPLATE_SCENARIOS})
endif()
