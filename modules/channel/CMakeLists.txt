# channel: communication (local sockets, process and stdio channels, the live protocol of a peer)
affrescoui_module(channel
	# transports: the bytes in and out (local sockets, child processes, stdio) and the line framing they share
	src/transports/Channels.cpp
	src/transports/Sockets.cpp
	# session: the live protocol a peer speaks over a channel
	src/session/Session.cpp)
target_link_libraries(affrescoui_channel PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
if(WIN32)
	target_link_libraries(affrescoui_channel PRIVATE ws2_32 advapi32) # Winsock; the socket files' access lists
endif()
