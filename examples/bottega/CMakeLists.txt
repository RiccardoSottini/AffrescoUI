# Bottega: Sinopia's slide sorter installed into Tempera through a contribution point, an adapter and a proof
# (examples/bottega/bottega.scenarios.json). Its program is Tempera's natives with affrescoui's composition commands.
set(AFFRESCOUI_BOTTEGA_TEMPERA ${PROJECT_SOURCE_DIR}/examples/tempera)
add_executable(affrescoui_bottega main.cpp ${PROJECT_SOURCE_DIR}/tools/affrescoui/Cli.cpp
	${PROJECT_SOURCE_DIR}/tools/affrescoui/ComposeCommand.cpp ${AFFRESCOUI_BOTTEGA_TEMPERA}/Session.cpp
	${AFFRESCOUI_BOTTEGA_TEMPERA}/Tools.cpp ${AFFRESCOUI_BOTTEGA_TEMPERA}/Natives.cpp
	${AFFRESCOUI_BOTTEGA_TEMPERA}/NativesEdit.cpp ${PROJECT_SOURCE_DIR}/examples/shared/CodeEditor.cpp)
target_link_libraries(affrescoui_bottega PRIVATE affrescoui::automation affrescoui::imaging)
target_include_directories(affrescoui_bottega PRIVATE ${PROJECT_SOURCE_DIR}/examples ${PROJECT_SOURCE_DIR}/examples/shared
	${PROJECT_SOURCE_DIR}/tools/affrescoui ${PROJECT_SOURCE_DIR}/modules/format/src ${PROJECT_SOURCE_DIR}/modules/vcs/src
	${PROJECT_SOURCE_DIR}/modules/screen/src)
target_compile_options(affrescoui_bottega PRIVATE ${AFFRESCOUI_WARNINGS})
target_compile_definitions(affrescoui_bottega PRIVATE AFFRESCOUI_CLI_EXAMPLE_TYPES
	AFFRESCOUI_TEMPERA_DIRECTORY="${AFFRESCOUI_BOTTEGA_TEMPERA}" AFFRESCOUI_BOTTEGA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(affrescoui_bottega PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
if(AFFRESCOUI_BUILD_TESTS)
	add_test(NAME bottega_scenarios COMMAND affrescoui_bottega)
endif()
