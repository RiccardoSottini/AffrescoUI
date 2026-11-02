# lisp: the language (README.md): reader, compiler, machine, its own functions, programs and their pure build
affrescoui_module(lisp
	src/Value.cpp
	src/Reader.cpp
	src/Compiler.cpp
	src/CompilerForms.cpp
	src/Definitions.cpp
	src/Machine.cpp
	src/CoreNumbers.cpp
	src/CoreCollections.cpp
	src/CoreText.cpp
	src/Prelude.cpp
	src/Tree.cpp
	src/Interpreter.cpp
	src/Program.cpp)
target_link_libraries(affrescoui_lisp PUBLIC affrescoui::core affrescoui::notation nlohmann_json::nlohmann_json)
