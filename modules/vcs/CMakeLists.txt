# vcs: version control (commits, proposals, permission rules, review, change sets, the history on disk)
affrescoui_module(vcs
	# revisions: the repository's commits, what a revision changes, and the size limits on it
	src/revisions/Repository.cpp
	src/revisions/RepositoryDocuments.cpp
	src/revisions/Deltas.cpp
	src/revisions/Budget.cpp
	# history: the history on disk (snapshots, refs, packs) and loading it back
	src/history/RepositoryHistory.cpp
	src/history/HistoryStore.cpp
	# review: proposals, the three-way merge and repairing change sets
	src/review/RepositoryReview.cpp
	src/review/Merge.cpp
	src/review/ChangeSetRepair.cpp
	# permissions: permission rules, data policies and roles
	src/permissions/Permissions.cpp
	src/permissions/Policy.cpp
	src/permissions/PolicyRepository.cpp
	src/permissions/Roles.cpp
	# peers: leases on parts of a screen and who is connected
	src/peers/Leases.cpp
	src/peers/LeasesRepository.cpp
	src/peers/Presence.cpp
	# The revision a composition proof installs, shared with the features service and the command line
	src/CompositionProofRevision.cpp)
target_link_libraries(affrescoui_vcs PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
