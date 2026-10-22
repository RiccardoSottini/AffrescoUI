# services: folders, programs and JSON-RPC servers a screen calls by method name (RFC 0002)
affrescoui_module(services
	src/Common.cpp
	src/FileProvider.cpp
	src/Framing.cpp
	src/JsonRpc.cpp
	src/Process.cpp
	src/ProcessProvider.cpp
	src/Registry.cpp
	src/Schema.cpp)
target_link_libraries(affrescoui_services PUBLIC affrescoui::core nlohmann_json::nlohmann_json PRIVATE affrescoui_regex)
