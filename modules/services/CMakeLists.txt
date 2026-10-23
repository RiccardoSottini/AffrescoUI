# services: folders, programs and JSON-RPC servers a screen calls by method name (RFC 0002)
affrescoui_module(services
	# The registry, the providers' shared helpers and the schema check
	src/Common.cpp
	src/Registry.cpp
	src/Schema.cpp
	# grants/: what a host grants from its command line (programs on the PATH, the login shell, git's policy)
	src/grants/GitPolicy.cpp
	src/grants/Grants.cpp
	# dap/: the debug adapter service (Debug Adapter Protocol)
	src/dap/Dap.cpp
	src/dap/DapProtocol.cpp
	src/dap/DapTransport.cpp
	# files/: the file and watch services, the directory model and the watcher
	src/files/DirectoryModel.cpp
	src/files/FileEntries.cpp
	src/files/FileProvider.cpp
	src/files/WatchProvider.cpp
	src/files/Watcher.cpp
	# process/: programs, run once or kept, and terminals
	src/process/Process.cpp
	src/process/ProcessProvider.cpp
	src/process/Pty.cpp
	src/process/PtyProvider.cpp
	# rpc/: JSON-RPC servers (LSP and the like) and the framing of what they write
	src/rpc/Framing.cpp
	src/rpc/JsonRpc.cpp
	# search/: the search engine, its globs and the search service
	src/search/SearchEngine.cpp
	src/search/SearchGlob.cpp
	src/search/SearchProvider.cpp
	# syntax/: the syntax engine (tree-sitter), its languages and the syntax service
	src/syntax/Syntax.cpp
	src/syntax/SyntaxLanguages.cpp
	src/syntax/SyntaxProvider.cpp)
target_link_libraries(affrescoui_services PUBLIC affrescoui::core nlohmann_json::nlohmann_json affrescoui_regex)
# The syntax engine: tree-sitter, its grammars and their queries (cmake/AffrescoUISyntax.cmake)
target_link_libraries(affrescoui_services PRIVATE affrescoui_syntax)
target_include_directories(affrescoui_services PRIVATE ${AFFRESCOUI_SYNTAX_GENERATED})
# The watcher's backend: FSEvents, inotify or ReadDirectoryChangesW.
if(APPLE)
	target_sources(affrescoui_services PRIVATE src/files/WatcherMac.cpp)
	target_link_libraries(affrescoui_services PRIVATE "-framework CoreServices")
elseif(WIN32)
	target_sources(affrescoui_services PRIVATE src/files/WatcherWindows.cpp)
elseif(EMSCRIPTEN) # a browser can't watch folders (cmake/AffrescoUIWeb.cmake)
	target_sources(affrescoui_services PRIVATE src/files/WatcherWeb.cpp)
else()
	target_sources(affrescoui_services PRIVATE src/files/WatcherLinux.cpp)
endif()
# forkpty lives in libutil, except on Apple (in libc) and Windows (a pseudo console instead).
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
	target_link_libraries(affrescoui_services PRIVATE util)
endif()
