# vcs: version control (commits, proposals, permission rules, review, change sets, the history on disk)
affrescoui_module(vcs
	src/Repository.cpp
	src/RepositoryReview.cpp
	src/Merge.cpp
	src/Budget.cpp
	src/RepositoryHistory.cpp
	src/RepositoryDocuments.cpp
	src/Deltas.cpp
	src/HistoryStore.cpp
	src/Permissions.cpp
	src/Leases.cpp
	src/LeasesRepository.cpp
	src/Presence.cpp
	src/Roles.cpp
	src/Policy.cpp
	src/PolicyRepository.cpp)
target_link_libraries(affrescoui_vcs PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
