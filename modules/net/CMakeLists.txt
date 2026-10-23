# net: the network (HTTP(S) requests, server-sent events and WebSocket connections over libcurl, and remote peers
# over WebSocket, RFC 0003)
affrescoui_module(net
	# The client, URLs, TCP sockets and random bytes
	src/Client.cpp
	src/Random.cpp
	src/Tcp.cpp
	src/Url.cpp
	# http/: requests over libcurl, on the worker thread, and the streams they read
	src/http/Curl.cpp
	src/http/Http.cpp
	src/http/Streams.cpp
	src/http/Worker.cpp
	# oauth/: sign-in (OAuth 2 and OpenID Connect): the browser, the loopback redirect and the device flow
	src/oauth/LoopbackRedirect.cpp
	src/oauth/OAuth.cpp
	src/oauth/OAuthBrowser.cpp
	src/oauth/OAuthDevice.cpp
	src/oauth/OAuthSignIn.cpp
	src/oauth/OAuthTimer.cpp
	# peers/: remote peers over WebSocket (RFC 0003)
	src/peers/PeerConnection.cpp
	src/peers/PeerHandshake.cpp
	src/peers/RemotePeers.cpp
	# websocket/: WebSocket connections and their frames
	src/websocket/WebSocketClient.cpp
	src/websocket/WebSocketFrames.cpp)
if(EMSCRIPTEN) # the browser's fetch and WebSocket in place of libcurl (cmake/AffrescoUIWeb.cmake)
	affrescoui_web_net(affrescoui_net)
endif()
# nlohmann_json is public: SignIn.hpp includes core's Identity.hpp, whose json is nlohmann's
target_link_libraries(affrescoui_net PUBLIC affrescoui::core nlohmann_json::nlohmann_json PRIVATE CURL::libcurl)
# The system's secret store (SignIn's refresh tokens) and its random bytes
if(WIN32)
	target_sources(affrescoui_net PRIVATE src/secrets/SecretStoreWindows.cpp)
	target_link_libraries(affrescoui_net PRIVATE ws2_32 advapi32 bcrypt shell32) # Winsock, Credential Manager, random
	# curl's headers bring in windows.h, whose min and max macros break std::min and std::max
	target_compile_definitions(affrescoui_net PRIVATE NOMINMAX)
elseif(APPLE)
	target_sources(affrescoui_net PRIVATE src/secrets/SecretStoreApple.cpp)
	target_link_libraries(affrescoui_net PRIVATE "-framework Security" "-framework CoreFoundation")
else()
	target_sources(affrescoui_net PRIVATE src/secrets/SecretStoreLinux.cpp)
	target_link_libraries(affrescoui_net PRIVATE ${CMAKE_DL_LIBS}) # libsecret, loaded at run time if installed
endif()
