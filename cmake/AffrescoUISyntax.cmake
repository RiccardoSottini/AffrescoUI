# The syntax service's parser (modules/services/src/Syntax*.cpp), included by the root CMakeLists.txt: tree-sitter and
# seven grammars, each downloaded as the archive of a tagged commit pinned by its hash and compiled into one small static
# library (their own build files aren't run). The grammars ship their generated parser.c, so no generator runs either.
# Their highlight queries, and ours for folds and brackets (modules/services/syntax/queries), are embedded in the
# library as string literals (SyntaxQueries.inc in the build folder).
set(AFFRESCOUI_TREE_SITTER_VERSION 0.26.13)

# Name, GitHub repository, commit (its release tag in the comment) and the archive's SHA-256
set(affrescoui_syntax_pins
	"tree_sitter|tree-sitter/tree-sitter|d97971e24500218865c05ed1febdee2acf41bae1|92acd713740a0e053837ed326156fb5317a48a8c86045a92d58092d1b727fc4f" # v0.26.13
	"ts_c|tree-sitter/tree-sitter-c|b780e47fc780ddc8da13afa35a3f4ed5c157823d|dc3979e40fce678c27273ba8ab93b48b5acd6b91d1dfde6a4451222d366d689f" # v0.24.2
	"ts_cpp|tree-sitter/tree-sitter-cpp|f41e1a044c8a84ea9fa8577fdd2eab92ec96de02|ad4b9b56602e9b14082637988f0042521006819960ed8c308bc24ded9ceb9331" # v0.23.4
	"ts_json|tree-sitter/tree-sitter-json|ee35a6ebefcef0c5c416c0d1ccec7370cfca5a24|9e4a7391cc3a9f4b87b382956dd01bc1f99fb1afcdc86b2c152c8a1bd14444d5" # v0.24.8
	"ts_python|tree-sitter/tree-sitter-python|293fdc02038ee2bf0e2e206711b69c90ac0d413f|b74d8ba6730535354175436208fe1452fd464f235263959ca40b8bf32cb23dc6" # v0.25.0
	"ts_javascript|tree-sitter/tree-sitter-javascript|44c892e0be055ac465d5eeddae6d3e194424e7de|a13fa28148e41bb939b2c2b8cd73c4b4295273b70cbac55a7a589020f52b9611" # v0.25.0
	"ts_typescript|tree-sitter/tree-sitter-typescript|f975a621f4e7f532fe322e13c4f79495e0a7b2e7|4de2e82e557810eecb93cb3b31fbb3bd28ba4c91ba9a6c6164bcaa074125b7b5" # v0.23.2
	"ts_markdown|tree-sitter-grammars/tree-sitter-markdown|f969cd3ae3f9fbd4e43205431d0ae286014c05b5|45ec28324b75ec80788775a6aed26888f55b306981fede9cbe35d7348ae80469") # v0.5.3
foreach(pin ${affrescoui_syntax_pins})
	string(REPLACE "|" ";" fields "${pin}")
	list(GET fields 0 name)
	list(GET fields 1 repository)
	list(GET fields 2 commit)
	list(GET fields 3 hash)
	FetchContent_Declare(${name}
		URL https://github.com/${repository}/archive/${commit}.tar.gz
		URL_HASH SHA256=${hash}
		SOURCE_SUBDIR affrescoui-compiles-its-sources SYSTEM) # a folder they don't have: their own builds aren't run
	FetchContent_MakeAvailable(${name})
endforeach()

set(syntax_sources
	${tree_sitter_SOURCE_DIR}/lib/src/lib.c
	${ts_c_SOURCE_DIR}/src/parser.c
	${ts_cpp_SOURCE_DIR}/src/parser.c
	${ts_cpp_SOURCE_DIR}/src/scanner.c
	${ts_json_SOURCE_DIR}/src/parser.c
	${ts_python_SOURCE_DIR}/src/parser.c
	${ts_python_SOURCE_DIR}/src/scanner.c
	${ts_javascript_SOURCE_DIR}/src/parser.c
	${ts_javascript_SOURCE_DIR}/src/scanner.c
	${ts_typescript_SOURCE_DIR}/typescript/src/parser.c
	${ts_typescript_SOURCE_DIR}/typescript/src/scanner.c
	${ts_markdown_SOURCE_DIR}/tree-sitter-markdown/src/parser.c
	${ts_markdown_SOURCE_DIR}/tree-sitter-markdown/src/scanner.c)
add_library(affrescoui_syntax STATIC ${syntax_sources})
# Each grammar finds its own src/tree_sitter/parser.h next to its parser.c
target_include_directories(affrescoui_syntax SYSTEM PUBLIC $<BUILD_INTERFACE:${tree_sitter_SOURCE_DIR}/lib/include>)
target_include_directories(affrescoui_syntax SYSTEM PRIVATE ${tree_sitter_SOURCE_DIR}/lib/src)
target_compile_definitions(affrescoui_syntax PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE _BSD_SOURCE
	_DARWIN_C_SOURCE)
set_target_properties(affrescoui_syntax PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
# Their warnings are not ours to fix
if(MSVC)
	set_source_files_properties(${syntax_sources} PROPERTIES COMPILE_OPTIONS "/w" SKIP_LINTING ON)
else()
	set_source_files_properties(${syntax_sources} PROPERTIES COMPILE_OPTIONS "-w" SKIP_LINTING ON)
endif()
set_target_properties(affrescoui_syntax PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
# TypeScript's shared scanner (common/scanner.h) looks for tree_sitter/parser.h beside it
set_source_files_properties(${ts_typescript_SOURCE_DIR}/typescript/src/scanner.c PROPERTIES INCLUDE_DIRECTORIES
	${ts_typescript_SOURCE_DIR}/typescript/src)

# The queries, as raw string literals (a name, then its files): upstream's highlights (composed as each grammar's tree-sitter.json does), and
# ours for folds and brackets
set(affrescoui_syntax_own ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/syntax/queries)
set(affrescoui_syntax_queries
	"c_highlights|${ts_c_SOURCE_DIR}/queries/highlights.scm"
	"cpp_highlights|${ts_c_SOURCE_DIR}/queries/highlights.scm|${ts_cpp_SOURCE_DIR}/queries/highlights.scm"
	"json_highlights|${ts_json_SOURCE_DIR}/queries/highlights.scm"
	"python_highlights|${ts_python_SOURCE_DIR}/queries/highlights.scm"
	"javascript_highlights|${ts_javascript_SOURCE_DIR}/queries/highlights.scm|${ts_javascript_SOURCE_DIR}/queries/highlights-jsx.scm|${ts_javascript_SOURCE_DIR}/queries/highlights-params.scm"
	"typescript_highlights|${ts_typescript_SOURCE_DIR}/queries/highlights.scm|${ts_javascript_SOURCE_DIR}/queries/highlights.scm"
	"markdown_highlights|${ts_markdown_SOURCE_DIR}/tree-sitter-markdown/queries/highlights.scm")
foreach(language c cpp json python javascript typescript markdown)
	foreach(kind folds brackets)
		list(APPEND affrescoui_syntax_queries "${language}_${kind}|${affrescoui_syntax_own}/${language}-${kind}.scm")
	endforeach()
endforeach()
set(affrescoui_syntax_inc "// Generated by cmake/AffrescoUISyntax.cmake: the syntax service's queries\n")
foreach(query ${affrescoui_syntax_queries})
	string(REPLACE "|" ";" files "${query}")
	list(POP_FRONT files name)
	set(text "")
	foreach(file ${files})
		file(READ ${file} part)
		string(APPEND text "${part}\n")
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${file})
	endforeach()
	string(APPEND affrescoui_syntax_inc "constexpr std::string_view ${name} = R\"tsq(${text})tsq\";\n")
endforeach()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/syntax/SyntaxQueries.inc.new "${affrescoui_syntax_inc}")
file(COPY_FILE ${CMAKE_CURRENT_BINARY_DIR}/generated/syntax/SyntaxQueries.inc.new
	${CMAKE_CURRENT_BINARY_DIR}/generated/syntax/SyntaxQueries.inc ONLY_IF_DIFFERENT)
set(AFFRESCOUI_SYNTAX_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated/syntax)
