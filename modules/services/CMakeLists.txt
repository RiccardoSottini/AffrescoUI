# services: folders, programs and JSON-RPC servers a screen calls by method name (RFC 0002)
affrescoui_module(services
	src/Common.cpp
	src/FileProvider.cpp
	src/Framing.cpp
	src/JsonRpc.cpp
	src/Process.cpp
	src/ProcessProvider.cpp
	src/Registry.cpp
	src/Schema.cpp
	src/DirectoryModel.cpp
	src/WatchProvider.cpp
	src/Watcher.cpp)
target_link_libraries(affrescoui_services PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
# The watcher's backend: FSEvents, inotify or ReadDirectoryChangesW.
if(APPLE)
	target_sources(affrescoui_services PRIVATE src/WatcherMac.cpp)
	target_link_libraries(affrescoui_services PRIVATE "-framework CoreServices")
elseif(WIN32)
	target_sources(affrescoui_services PRIVATE src/WatcherWindows.cpp)
else()
	target_sources(affrescoui_services PRIVATE src/WatcherLinux.cpp)
endif()
