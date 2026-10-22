# services: folders, programs and JSON-RPC servers a screen calls by method name (RFC 0002)
affrescoui_module(services
	src/Common.cpp
	src/Dap.cpp
	src/DapProtocol.cpp
	src/DapTransport.cpp
	src/FileProvider.cpp
	src/Framing.cpp
	src/JsonRpc.cpp
	src/Process.cpp
	src/ProcessProvider.cpp
	src/Pty.cpp
	src/PtyProvider.cpp
	src/Registry.cpp
	src/Schema.cpp
	src/DirectoryModel.cpp
	src/SearchEngine.cpp
	src/SearchGlob.cpp
	src/SearchProvider.cpp
	src/WatchProvider.cpp
	src/Watcher.cpp)
target_link_libraries(affrescoui_services PUBLIC affrescoui::core nlohmann_json::nlohmann_json affrescoui_regex)
# The watcher's backend: FSEvents, inotify or ReadDirectoryChangesW.
if(APPLE)
	target_sources(affrescoui_services PRIVATE src/WatcherMac.cpp)
	target_link_libraries(affrescoui_services PRIVATE "-framework CoreServices")
elseif(WIN32)
	target_sources(affrescoui_services PRIVATE src/WatcherWindows.cpp)
else()
	target_sources(affrescoui_services PRIVATE src/WatcherLinux.cpp)
endif()
# forkpty lives in libutil, except on Apple (in libc) and Windows (a pseudo console instead).
if(UNIX AND NOT APPLE)
	target_link_libraries(affrescoui_services PRIVATE util)
endif()
