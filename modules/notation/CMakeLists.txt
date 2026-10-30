# notation: the notations a document is written in, JSON and Lisp (README.md): text into the JSON tree and back
affrescoui_module(notation
	src/Notation.cpp
	src/LispReader.cpp
	src/LispWriter.cpp)
target_link_libraries(affrescoui_notation PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
