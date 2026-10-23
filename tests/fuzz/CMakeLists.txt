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
		AFFRESCOUI_FUZZ_TEMPLATE="${PROJECT_SOURCE_DIR}/examples/flow/kanban.screen.json")
endfunction()

affrescoui_fuzzer(json affrescoui::core nlohmann_json::nlohmann_json)
affrescoui_fuzzer(expression affrescoui::format)
target_include_directories(affrescoui_fuzz_expression PRIVATE
	$<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>) # Expression.hpp
affrescoui_fuzzer(revision affrescoui::format affrescoui::vcs)
affrescoui_fuzzer(migrate affrescoui::format) # version 1 files (SCREEN_FORMAT.md, "Migrating from version 1")
affrescoui_fuzzer(session affrescoui::format affrescoui::vcs affrescoui::channel)
affrescoui_fuzzer(delta affrescoui::vcs)
affrescoui_fuzzer(policy affrescoui::format affrescoui::vcs)
affrescoui_fuzzer(text affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_text PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src) # detail/Utf8.hpp
# Where its fonts are: not from __FILE__, which a compiler cache (CCACHE_BASEDIR in CI) makes relative to the build
target_compile_definitions(affrescoui_fuzz_text PRIVATE AFFRESCOUI_FUZZ_CHECKOUT="${PROJECT_SOURCE_DIR}")
affrescoui_fuzzer(richtext affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_compile_definitions(affrescoui_fuzz_richtext PRIVATE AFFRESCOUI_FUZZ_CHECKOUT="${PROJECT_SOURCE_DIR}")
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
target_include_directories(affrescoui_fuzz_actions PRIVATE
	$<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>) # Expression.hpp
# The replies peers send to the screen's own requests ("ask" steps)
affrescoui_fuzzer(ask_reply affrescoui::format affrescoui::vcs)
affrescoui_fuzzer(lookups affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_lookups PRIVATE ${PROJECT_SOURCE_DIR}/modules/render/src) # LookupSpans.hpp
affrescoui_fuzzer(image affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
affrescoui_fuzzer(path affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
affrescoui_fuzzer(text_model affrescoui::render nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
affrescoui_fuzzer(find_pattern affrescoui::regex affrescoui::core nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
affrescoui_fuzzer(search_glob affrescoui::services nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_search_glob PRIVATE ${PROJECT_SOURCE_DIR}/modules/services/src) # SearchGlob.hpp
affrescoui_fuzzer(scenarios affrescoui::automation)
affrescoui_fuzzer(png affrescoui::automation)
target_include_directories(affrescoui_fuzz_png PRIVATE ${PROJECT_SOURCE_DIR}/modules/automation/src) # PngReader.hpp
affrescoui_fuzzer(http_response affrescoui::net nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_http_response PRIVATE ${PROJECT_SOURCE_DIR}/modules/net/src) # Http.hpp
affrescoui_fuzzer(sse affrescoui::net nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_sse PRIVATE ${PROJECT_SOURCE_DIR}/modules/net/src) # Streams.hpp
affrescoui_fuzzer(websocket affrescoui::net nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_websocket PRIVATE ${PROJECT_SOURCE_DIR}/modules/net/src) # WebSocketFrames.hpp
affrescoui_fuzzer(peer_handshake affrescoui::net nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_peer_handshake PRIVATE ${PROJECT_SOURCE_DIR}/modules/net/src) # PeerHandshake.hpp
affrescoui_fuzzer(frames affrescoui::services nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_frames PRIVATE ${PROJECT_SOURCE_DIR}/modules/services/src) # Framing.hpp
affrescoui_fuzzer(dap affrescoui::services nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
target_include_directories(affrescoui_fuzz_dap PRIVATE ${PROJECT_SOURCE_DIR}/modules/services/src) # DapProtocol.hpp
# Feature packages and catalogues (RFC 0004): FeatureManifest.hpp, FeatureCatalog.hpp and FeatureSignatures.hpp
affrescoui_fuzzer(feature_manifest affrescoui::format)
target_include_directories(affrescoui_fuzz_feature_manifest PRIVATE
	$<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>)
affrescoui_fuzzer(catalog_index affrescoui::format)
target_include_directories(affrescoui_fuzz_catalog_index PRIVATE
	$<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>)
affrescoui_fuzzer(signature affrescoui::format)
target_include_directories(affrescoui_fuzz_signature PRIVATE $<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>)
# Composition across apps: a feature's needs, its adapter and the host's points (CompositionProof.hpp)
affrescoui_fuzzer(composition affrescoui::format)
target_include_directories(affrescoui_fuzz_composition PRIVATE $<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>)
# The 1.0 security audit (SECURITY.md, README.md "Inventory of untrusted inputs"): URLs, sign-in responses, the token
# file, what git prints, imported VS Code themes and history files on disk
affrescoui_fuzzer(url affrescoui::net nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
affrescoui_fuzzer(oauth affrescoui::net nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_fuzz_oauth PRIVATE ${PROJECT_SOURCE_DIR}/modules/net/src) # OAuth.hpp
affrescoui_fuzzer(history affrescoui::format affrescoui::vcs)
# The apps' readers (apps/server, examples/forge): their own sources, linked only into the fuzzer (the apps' targets
# compile them otherwise)
affrescoui_fuzzer(token_file affrescoui::net nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_fuzz_token_file PRIVATE ${PROJECT_SOURCE_DIR}/apps/server) # TokenFile.hpp
affrescoui_fuzzer(git_output affrescoui::core nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_fuzz_git_output PRIVATE ${PROJECT_SOURCE_DIR}/examples/forge)
affrescoui_fuzzer(vscode_theme affrescoui::core nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_fuzz_vscode_theme PRIVATE ${PROJECT_SOURCE_DIR}/examples/forge)
if(AFFRESCOUI_BUILD_FUZZERS)
	target_sources(affrescoui_fuzz_token_file PRIVATE ${PROJECT_SOURCE_DIR}/apps/server/TokenFile.cpp)
	target_sources(affrescoui_fuzz_git_output PRIVATE ${PROJECT_SOURCE_DIR}/examples/forge/features/scm/GitStatus.cpp
		${PROJECT_SOURCE_DIR}/examples/forge/features/scm/LineDiff.cpp)
	target_sources(affrescoui_fuzz_vscode_theme PRIVATE
		${PROJECT_SOURCE_DIR}/examples/forge/features/themes/ThemeImport.cpp)
endif()
affrescoui_fuzzer(sinopia_deck affrescoui::render nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_fuzz_sinopia_deck PRIVATE ${PROJECT_SOURCE_DIR}/examples/sinopia) # deck/Deck.hpp
if(AFFRESCOUI_BUILD_FUZZERS)
	target_sources(affrescoui_fuzz_sinopia_deck PRIVATE ${PROJECT_SOURCE_DIR}/examples/sinopia/deck/Deck.cpp
		${PROJECT_SOURCE_DIR}/examples/sinopia/deck/Write.cpp)
endif()
# Other formats (SCREEN_FORMAT.md): the A2UI and Adaptive Cards adapters
affrescoui_fuzzer(adapters affrescoui::format affrescoui::vcs)
affrescoui_fuzzer(syntax affrescoui::services nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
# Modello imports glTF files (examples/modello)
affrescoui_fuzzer(gltf affrescoui::scene3d nlohmann_json::nlohmann_json) # Fuzz.hpp reads JSON
# The image editor's inputs (examples/tempera): ICC profiles and project files
affrescoui_fuzzer(icc affrescoui::imaging)
affrescoui_fuzzer(tempera_project affrescoui::imaging)
