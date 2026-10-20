# libFuzzer targets for what comes from outside the app (README.md). Added by the top-level CMakeLists.txt when
# AFFRESCOUI_BUILD_FUZZERS is on, which also instruments the whole build.

# One target: tests/fuzz/<name>.cpp, linked with libFuzzer's main and the given libraries. With only
# AFFRESCOUI_COMPILE_FUZZ_SOURCES on, an object library instead: compiled, so tools that read the build see it, never
# linked
function(affrescoui_fuzzer name)
	if(AFFRESCOUI_BUILD_FUZZERS)
		add_executable(affrescoui_fuzz_${name} ${name}.cpp)
		target_link_options(affrescoui_fuzz_${name} PRIVATE -fsanitize=fuzzer)
	else()
		add_library(affrescoui_fuzz_${name} OBJECT ${name}.cpp)
	endif()
	target_link_libraries(affrescoui_fuzz_${name} PRIVATE ${ARGN})
	target_compile_features(affrescoui_fuzz_${name} PRIVATE cxx_std_23)
	set_target_properties(affrescoui_fuzz_${name} PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_fuzz_${name} PRIVATE ${AFFRESCOUI_WARNINGS})
	# The template the targets load (kanbanTemplate() in Fuzz.hpp)
	target_compile_definitions(affrescoui_fuzz_${name} PRIVATE
		AFFRESCOUI_FUZZ_TEMPLATE="${PROJECT_SOURCE_DIR}/examples/assets/screens/kanban.screen.json")
endfunction()

affrescoui_fuzzer(json affrescoui::core nlohmann_json::nlohmann_json)
affrescoui_fuzzer(expression affrescoui::format)
target_include_directories(affrescoui_fuzz_expression PRIVATE ${PROJECT_SOURCE_DIR}/modules/format/src) # Expression.hpp
affrescoui_fuzzer(revision affrescoui::format affrescoui::vcs)
affrescoui_fuzzer(session affrescoui::format affrescoui::vcs affrescoui::channel)
affrescoui_fuzzer(delta affrescoui::vcs)
affrescoui_fuzzer(text affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_text PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src) # detail/Utf8.hpp
affrescoui_fuzzer(layout affrescoui)
target_include_directories(affrescoui_fuzz_layout PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src) # Headless.hpp
# What the affrescoui tool reads (tools/affrescoui): its own sources, linked only into the fuzzer (the tool's target
# compiles them otherwise), and the schema it builds in
affrescoui_fuzzer(cli affrescoui::format)
if(AFFRESCOUI_BUILD_FUZZERS)
	target_sources(affrescoui_fuzz_cli PRIVATE ${PROJECT_SOURCE_DIR}/tools/affrescoui/JsonText.cpp
		${PROJECT_SOURCE_DIR}/tools/affrescoui/Schema.cpp)
endif()
target_compile_definitions(affrescoui_fuzz_cli PRIVATE
	AFFRESCOUI_FUZZ_SCHEMA="${PROJECT_SOURCE_DIR}/schema/affrescoui-screen.schema.json")
affrescoui_fuzzer(actions affrescoui::format affrescoui::vcs)
target_include_directories(affrescoui_fuzz_actions PRIVATE ${PROJECT_SOURCE_DIR}/modules/format/src) # Expression.hpp
affrescoui_fuzzer(lookups affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_lookups PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src) # LookupSpans.hpp
affrescoui_fuzzer(image affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
