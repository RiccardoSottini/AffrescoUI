# net: the network (HTTP(S) requests, server-sent events and WebSocket connections over libcurl, and remote peers
# over WebSocket, RFC 0003)
affrescoui_module(net
	src/Client.cpp
	src/Curl.cpp
	src/Http.cpp
	src/LoopbackRedirect.cpp
	src/OAuth.cpp
	src/OAuthBrowser.cpp
	src/OAuthDevice.cpp
	src/OAuthSignIn.cpp
	src/OAuthTimer.cpp
	src/PeerConnection.cpp
	src/PeerHandshake.cpp
	src/RemotePeers.cpp
	src/Streams.cpp
	src/Tcp.cpp
	src/Url.cpp
	src/WebSocketClient.cpp
	src/WebSocketFrames.cpp
	src/Worker.cpp)
# nlohmann_json is public: SignIn.hpp includes core's Identity.hpp, whose json is nlohmann's
target_link_libraries(affrescoui_net PUBLIC affrescoui::core nlohmann_json::nlohmann_json PRIVATE CURL::libcurl)
# The system's secret store (SignIn's refresh tokens) and its random bytes
if(WIN32)
	target_sources(affrescoui_net PRIVATE src/SecretStoreWindows.cpp)
	target_link_libraries(affrescoui_net PRIVATE ws2_32 advapi32 bcrypt shell32) # Winsock, Credential Manager, random
elseif(APPLE)
	target_sources(affrescoui_net PRIVATE src/SecretStoreApple.cpp)
	target_link_libraries(affrescoui_net PRIVATE "-framework Security" "-framework CoreFoundation")
else()
	target_sources(affrescoui_net PRIVATE src/SecretStoreLinux.cpp)
	target_link_libraries(affrescoui_net PRIVATE ${CMAKE_DL_LIBS}) # libsecret, loaded at run time if installed
endif()
