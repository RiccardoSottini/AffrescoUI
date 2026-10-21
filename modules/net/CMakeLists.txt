# net: the network (HTTP(S) requests, server-sent events and WebSocket connections over libcurl, and remote peers
# over WebSocket, RFC 0003)
affrescoui_module(net
	src/Client.cpp
	src/Curl.cpp
	src/Http.cpp
	src/PeerConnection.cpp
	src/PeerHandshake.cpp
	src/RemotePeers.cpp
	src/Streams.cpp
	src/Tcp.cpp
	src/Url.cpp
	src/WebSocketClient.cpp
	src/WebSocketFrames.cpp
	src/Worker.cpp)
target_link_libraries(affrescoui_net PUBLIC affrescoui::core PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
if(WIN32)
	target_link_libraries(affrescoui_net PRIVATE ws2_32) # Winsock (the loopback listener)
endif()
