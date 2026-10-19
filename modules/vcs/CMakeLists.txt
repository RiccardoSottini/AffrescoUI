# vcs: version control (commits, proposals, permission rules, review, change sets, the history on disk)
affrescoui_module(vcs
	src/Repository.cpp
	src/RepositoryHistory.cpp
	src/HistoryStore.cpp
	src/Permissions.cpp
	src/Sha256.cpp)
target_link_libraries(affrescoui_vcs PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
