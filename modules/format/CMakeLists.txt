# format: the JSON screen format (rules, the checker, expressions, building panels, events and steps)
affrescoui_module(format
	src/Expression.cpp
	src/Rules.cpp
	src/Runtime.cpp
	src/Build.cpp
	src/Properties.cpp
	src/Events.cpp
	src/Steps.cpp
	src/Checker.cpp
	src/CheckerNodes.cpp
	src/CheckerValues.cpp)
target_link_libraries(affrescoui_format PUBLIC affrescoui::core affrescoui::render nlohmann_json::nlohmann_json)
