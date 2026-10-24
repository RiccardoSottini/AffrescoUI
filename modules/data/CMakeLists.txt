# data: the record store: typed models, records in SQLite, queries, the journal, migrations and CSV import (README.md)
affrescoui_module(data
	src/Changes.cpp
	src/Csv.cpp
	src/Migration.cpp
	src/Model.cpp
	src/Query.cpp
	src/Sqlite.cpp
	src/Store.cpp
	src/StoreFollow.cpp
	src/StoreJournal.cpp
	${AFFRESCOUI_SQLITE_SOURCES})
target_link_libraries(affrescoui_data PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
target_include_directories(affrescoui_data SYSTEM PRIVATE ${AFFRESCOUI_SQLITE_DIR})
target_compile_definitions(affrescoui_data PRIVATE ${AFFRESCOUI_SQLITE_DEFINITIONS})
if(NOT WIN32 AND NOT EMSCRIPTEN)
	find_package(Threads REQUIRED)
	target_link_libraries(affrescoui_data PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
