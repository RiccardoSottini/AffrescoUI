# The browser build (Emscripten, web/README.md), included by CMakeLists.txt in place of GLFW's own build and libcurl.
#
#   emcmake cmake -S . -B build-web -G Ninja && cmake --build build-web     (or tools/web/build.sh, which makes the site)
#
# - Windows, input and the WebGL2 context: GLFW's Emscripten port (contrib.glfw3, the GLFW 3.4 API on a canvas, with
#   the page's devicePixelRatio and clipboard), which emcc fetches and builds itself. The backend is always gles3.
# - The network: the browser's fetch and WebSocket (modules/net/src/*/*Web.cpp), not libcurl. What needs a socket of its
#   own (a peer server, a loopback redirect) fails as it would on a machine that can't listen.
# - One thread: the browser build has no -pthread (no SharedArrayBuffer, so no cross-origin isolation for the page to
#   ask for). What a desktop build does on worker threads runs on the main one, or reports itself missing.
# - What a browser can't do (processes, pseudo terminals, watching folders, local sockets) reports itself missing
#   through the services registry; files live in the page's own memory (MEMFS), with the assets preloaded.
#
# The emsdk release the site is built and checked with: tools/web/build.sh and .github/workflows/web.yml install it.
set(AFFRESCOUI_EMSDK_VERSION 6.0.10)
if(NOT EMSCRIPTEN)
	return()
endif()

# The tests, benchmarks, fuzzers and installed package are the desktop build's; the browser's checks are the
# scenarios, played by the command-line tool under Node (web.yml)
set(AFFRESCOUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AFFRESCOUI_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(AFFRESCOUI_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(AFFRESCOUI_INSTALL OFF CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig ON) # no D-Bus: the render module's file dialogs are left out

# glfw: GLFW's port, as a target with the name the modules link
add_library(glfw INTERFACE)
target_compile_options(glfw INTERFACE --use-port=contrib.glfw3)
target_link_options(glfw INTERFACE --use-port=contrib.glfw3 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2)

# CURL::libcurl: nothing, so the net module's link line stays the same (its curl sources aren't compiled here)
add_library(CURL::libcurl INTERFACE IMPORTED)

# The net module's libcurl sources, and what takes their place in a browser: the same Client (its checks, the
# allowlist) over the browser's fetch (WorkerWeb.cpp) and WebSocket (WebSocketWeb.cpp), and the page's own window for
# a sign-in (OAuthBrowserWeb.cpp). src/Web/curl/curl.h names
# libcurl's types for the headers that mention them; nothing calls libcurl.
set(AFFRESCOUI_WEB_NET_CURL_SOURCES src/http/Curl.cpp src/http/Worker.cpp src/websocket/WebSocketClient.cpp
	src/oauth/OAuthBrowser.cpp)

# Gives the net module the browser's network in place of libcurl.
# target: affrescoui_net.
function(affrescoui_web_net target)
	get_target_property(sources ${target} SOURCES)
	list(REMOVE_ITEM sources ${AFFRESCOUI_WEB_NET_CURL_SOURCES})
	set_property(TARGET ${target} PROPERTY SOURCES ${sources} src/http/WorkerWeb.cpp
		src/websocket/WebSocketWeb.cpp src/oauth/OAuthBrowserWeb.cpp)
	target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/modules/net/src/Web)
endfunction()

# Links a program for the browser (or Node, which plays the scenarios): its memory grows, exceptions work (the
# library reports errors with them), and the main loop gives way to the browser between frames.
# target: the executable. environment: where it runs (web, or node).
function(affrescoui_web_program target environment)
	target_link_options(${target} PRIVATE -fexceptions -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB
		-sENVIRONMENT=${environment} -sEXPORTED_RUNTIME_METHODS=ccall,callMain)
endfunction()

# Makes the programs the site and the checks use, once the root CMakeLists.txt has defined them: the host for the
# pages, the Studio's too (the templates' folders, the Studio among them, and examples/shared preloaded under /assets,
# without their C++ sources, so no path of the machine that built them is in the site), and the command-line tool for
# Node, on the machine's own files (NODERAWFS), to play the scenarios. Forge and the document server need processes,
# a terminal and a listening socket: they aren't built for a browser.
function(affrescoui_web_programs)
	if(TARGET affrescoui_host)
		affrescoui_template_folders(folders)
		list(REMOVE_DUPLICATES folders)
		foreach(program affrescoui_host)
			affrescoui_web_program(${program} web)
			get_target_property(definitions ${program} COMPILE_DEFINITIONS)
			list(FILTER definitions EXCLUDE REGEX "^AFFRESCOUI_EXAMPLES_ASSETS=")
			set_property(TARGET ${program} PROPERTY COMPILE_DEFINITIONS ${definitions}
				[[AFFRESCOUI_EXAMPLES_ASSETS="/assets"]])
			foreach(folder ${folders} shared)
				target_link_options(${program} PRIVATE
					"SHELL:--preload-file ${PROJECT_SOURCE_DIR}/examples/${folder}@/assets/${folder}")
			endforeach()
			target_link_options(${program} PRIVATE "SHELL:--exclude-file *.cpp" "SHELL:--exclude-file *.hpp")
		endforeach()
		set_target_properties(affrescoui_forge affrescoui_server PROPERTIES EXCLUDE_FROM_ALL ON)
	endif()
	if(TARGET affrescoui_cli)
		affrescoui_web_program(affrescoui_cli node)
		target_link_options(affrescoui_cli PRIVATE -sNODERAWFS=1 -sEXIT_RUNTIME=1)
	endif()
endfunction()
cmake_language(DEFER DIRECTORY ${PROJECT_SOURCE_DIR} CALL affrescoui_web_programs)

# The library reports errors with exceptions: every target catches them (Emscripten's default is to abort on a throw)
add_compile_options(-fexceptions)
