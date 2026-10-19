# channel: communication (local sockets, process and stdio channels, the live protocol of a peer)
affrescoui_module(channel
	src/Channels.cpp
	src/Session.cpp
	src/Sockets.cpp)
target_link_libraries(affrescoui_channel PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
if(WIN32)
	target_link_libraries(affrescoui_channel PRIVATE ws2_32 advapi32) # Winsock; the socket files' access lists
endif()
