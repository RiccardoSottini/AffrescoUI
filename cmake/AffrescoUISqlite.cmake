# SQLite 3.54.1, the record store of the data module (modules/data), included by CMakeLists.txt. Downloaded as its
# amalgamation (public domain), pinned by its hash like the other dependencies; sqlite3.c is compiled into the data
# module, which is the only one that opens databases, without the strict warnings. Built without what a screen's store
# never needs: no extension loading (no native code from a file), no shared cache, no deprecated or tracing
# interfaces; with the JSON functions (built in) that queries read records' fields with. The browser build has no
# threads, so it is built single-threaded there.

set(AFFRESCOUI_SQLITE_VERSION 3.54.1)
FetchContent_Declare(sqlite
	URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
	URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
	SOURCE_SUBDIR affrescoui-builds-its-one-file SYSTEM) # a folder it doesn't have: nothing of it is configured
FetchContent_MakeAvailable(sqlite)
set(AFFRESCOUI_SQLITE_DIR ${sqlite_SOURCE_DIR})
set(AFFRESCOUI_SQLITE_SOURCES ${AFFRESCOUI_SQLITE_DIR}/sqlite3.c)
set(AFFRESCOUI_SQLITE_DEFINITIONS SQLITE_OMIT_LOAD_EXTENSION SQLITE_OMIT_SHARED_CACHE SQLITE_OMIT_DEPRECATED
	SQLITE_OMIT_TRACE SQLITE_DQS=0 SQLITE_DEFAULT_FOREIGN_KEYS=0 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_LIKE_DOESNT_MATCH_BLOBS
	SQLITE_MAX_EXPR_DEPTH=200 SQLITE_USE_ALLOCA)
if(EMSCRIPTEN)
	list(APPEND AFFRESCOUI_SQLITE_DEFINITIONS SQLITE_THREADSAFE=0)
else()
	list(APPEND AFFRESCOUI_SQLITE_DEFINITIONS SQLITE_THREADSAFE=2) # one connection per thread: the store's own
endif()
# Its warnings are not ours to fix
set_source_files_properties(${AFFRESCOUI_SQLITE_SOURCES} PROPERTIES
	COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>
	COMPILE_DEFINITIONS "${AFFRESCOUI_SQLITE_DEFINITIONS}"
	SKIP_LINTING ON)
