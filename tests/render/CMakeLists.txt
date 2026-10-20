# The rendering check (README.md): affrescoui_render_runner plays the scenarios in scenarios/*.json, and check.py
# compares the frames with the golden images. The runner is built next to the other programs, where check.py looks.
add_executable(affrescoui_render_runner runner.cpp ${PROJECT_SOURCE_DIR}/examples/CodeEditor.cpp)
# Headless.hpp (detail::drawFromScratch, what each capture is checked against) is render's, internal
target_include_directories(affrescoui_render_runner PRIVATE ${PROJECT_SOURCE_DIR}/examples
	${PROJECT_SOURCE_DIR}/modules/render/src)
target_link_libraries(affrescoui_render_runner PRIVATE affrescoui nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_render_runner PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(affrescoui_render_runner PRIVATE ${AFFRESCOUI_WARNINGS})
set_target_properties(affrescoui_render_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
	add_test(NAME rendering COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check.py --build ${PROJECT_BINARY_DIR}
	         WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endif()
