# lisp: the language (README.md): reader, compiler, machine, its own functions, programs and their pure build
affrescoui_module(lisp
	src/Value.cpp
	src/Reader.cpp
	src/Compiler.cpp
	src/CompilerForms.cpp
	src/CompilerBindings.cpp
	src/CompilerLowering.cpp
	src/Definitions.cpp
	src/Machine.cpp
	src/CoreNumbers.cpp
	src/CoreCollections.cpp
	src/CoreText.cpp
	src/CoreViews.cpp
	src/Prelude.cpp
	src/Tree.cpp
	src/Interpreter.cpp
	src/Forms.cpp
	src/FormCache.cpp
	src/Program.cpp
	src/Document.cpp)
target_link_libraries(affrescoui_lisp PUBLIC affrescoui::core affrescoui::notation nlohmann_json::nlohmann_json)
# A program that runs whatever links the language gets the 8 MiB main-thread stack of macOS and Linux, which the depth
# limits are set against: Windows gives 1 MiB, and MSVC's frames are larger, so 32 nested actions (a program calling
# a step that calls the program back) overflowed it before the limit stopped them
if(MSVC)
	target_link_options(affrescoui_lisp INTERFACE /STACK:8388608)
elseif(MINGW)
	target_link_options(affrescoui_lisp INTERFACE LINKER:--stack,8388608)
endif()
