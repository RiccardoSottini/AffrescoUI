# The rendering check (README.md): affrescoui_render_runner plays the scenarios in scenarios/*.json with the automation
# module and compares every picture with its golden image; check.py runs it. The runner is built next to the other
# programs, where check.py looks.
add_executable(affrescoui_render_runner runner.cpp ${PROJECT_SOURCE_DIR}/examples/CodeEditor.cpp)
target_include_directories(affrescoui_render_runner PRIVATE ${PROJECT_SOURCE_DIR}/examples)
target_link_libraries(affrescoui_render_runner PRIVATE affrescoui::automation)
target_compile_options(affrescoui_render_runner PRIVATE ${AFFRESCOUI_WARNINGS})
set_target_properties(affrescoui_render_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
	add_test(NAME rendering COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check.py --build ${PROJECT_BINARY_DIR}
	         WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

# The templates' own scenarios (examples/assets/screens/*.scenarios.json): what each template does, checked headless,
# without a window or golden images
file(GLOB AFFRESCOUI_TEMPLATE_SCENARIOS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/examples/assets/screens/*.scenarios.json)
if(AFFRESCOUI_TEMPLATE_SCENARIOS)
	add_test(NAME scenarios COMMAND affrescoui_render_runner ${AFFRESCOUI_TEMPLATE_SCENARIOS})
endif()
