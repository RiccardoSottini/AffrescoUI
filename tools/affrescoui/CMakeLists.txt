# affrescoui: the command-line tool for screen files (README.md here). The target is affrescoui_cli, the program
# affrescoui (the library already has the target name affrescoui).

# The screen format's schema, built into the tool so that `check` works wherever it is installed
set(AFFRESCOUI_CLI_SCHEMA ${PROJECT_SOURCE_DIR}/schema/affrescoui-screen.schema.json)
set(AFFRESCOUI_CLI_SCHEMA_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/EmbeddedSchema.cpp)
add_custom_command(OUTPUT ${AFFRESCOUI_CLI_SCHEMA_SOURCE}
	COMMAND ${CMAKE_COMMAND} -DINPUT=${AFFRESCOUI_CLI_SCHEMA} -DOUTPUT=${AFFRESCOUI_CLI_SCHEMA_SOURCE}
		-DFUNCTION=affresco::cli::embeddedSchema -P ${CMAKE_CURRENT_SOURCE_DIR}/Embed.cmake
	DEPENDS ${AFFRESCOUI_CLI_SCHEMA} ${CMAKE_CURRENT_SOURCE_DIR}/Embed.cmake
	COMMENT "Embedding the screen schema in affrescoui")

# The entry point, the commands' shared helpers (Cli.hpp), JSON text and the schema checker; ComposeCommand.cpp stays
# here beside Cli.cpp because affrescoui_bottega builds both from this folder
add_executable(affrescoui_cli main.cpp Cli.cpp ComposeCommand.cpp JsonText.cpp Schema.cpp ${AFFRESCOUI_CLI_SCHEMA_SOURCE}
	${PROJECT_SOURCE_DIR}/modules/screen/src/Png.cpp) # the library's PNG encoder, which it keeps private
# commands/: one file per command, or per family of commands (main.cpp lists them)
target_sources(affrescoui_cli PRIVATE commands/CheckCommand.cpp commands/FormatCommands.cpp commands/WindowCommands.cpp
	commands/HistoryCommands.cpp commands/TestCommand.cpp commands/PolicyCommand.cpp commands/SchemaCommand.cpp
	commands/SignCommand.cpp commands/ImportCommand.cpp commands/MigrateCommand.cpp)
target_include_directories(affrescoui_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # for commands/: Cli.hpp
set_target_properties(affrescoui_cli PROPERTIES OUTPUT_NAME affrescoui CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
target_link_libraries(affrescoui_cli PRIVATE affrescoui::automation) # and through it, the whole library
# SocketDir.hpp (the socket folder's checks) is shared with affrescoui_host; Png.hpp and FeatureSignatures.hpp are the
# library's
target_include_directories(affrescoui_cli PRIVATE ${PROJECT_SOURCE_DIR}/examples/shared ${PROJECT_SOURCE_DIR}/modules/screen/src
	${PROJECT_SOURCE_DIR}/modules/format/src ${PROJECT_SOURCE_DIR}/modules/vcs/src)
target_include_directories(affrescoui_cli SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(affrescoui_cli PRIVATE ${AFFRESCOUI_WARNINGS})
target_compile_definitions(affrescoui_cli PRIVATE AFFRESCOUI_VERSION="${PROJECT_VERSION}")
if(AFFRESCOUI_BUILD_EXAMPLES)
	# The example apps' own types, as affrescoui_host has them, so their templates run and render the same
	target_sources(affrescoui_cli PRIVATE ${PROJECT_SOURCE_DIR}/examples/shared/CodeEditor.cpp)
	target_sources(affrescoui_cli PRIVATE ${PROJECT_SOURCE_DIR}/examples/tempo/Visualiser.cpp) # Tempo's visualiser surface
	target_include_directories(affrescoui_cli PRIVATE ${PROJECT_SOURCE_DIR}/examples)
	target_include_directories(affrescoui_cli SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/glad/include)
	target_compile_definitions(affrescoui_cli PRIVATE AFFRESCOUI_CLI_EXAMPLE_TYPES)
endif()

if(AFFRESCOUI_INSTALL)
	install(TARGETS affrescoui_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(AFFRESCOUI_BUILD_TESTS)
	# Every command on every template (check.py here); `render` needs a window, so a display or Xvfb (on Windows,
	# whose CI runners have no OpenGL 4.1, it isn't checked). The programs are in the folder of the build's
	# configuration (Release/ with Visual Studio's generator).
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_FOUND)
		set(headless "")
		if(WIN32)
			set(headless --headless) # not a generator expression: elsewhere it would pass an empty argument
		endif()
		add_test(NAME cli COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check.py
			--build $<TARGET_FILE_DIR:affrescoui_cli> ${headless})
	endif()
	# The schema and SCREEN_FORMAT.md's tables are generated from the registries: fails if they are out of date
	add_test(NAME cli-reference COMMAND affrescoui_cli schema -o schema/affrescoui-screen.schema.json --types examples
		--doc SCREEN_FORMAT.md --check WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
