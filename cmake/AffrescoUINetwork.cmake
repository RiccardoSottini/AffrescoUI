# libcurl, the HTTP(S) transport of the net module (RFC 0003, decision D3), included by CMakeLists.txt.
#
# On macOS the system's libcurl is used: Apple maintains it with the system's TLS and trust store, and it is built for
# every architecture a universal binary needs. Elsewhere curl is downloaded as its release archive, pinned by its hash
# like the other dependencies, and built as a static library with HTTP and HTTPS only and the platform's TLS: Schannel
# on Windows, OpenSSL 3 on Linux (the system's, `libssl-dev`). Nothing optional is built in: no compression (no
# decompression bombs; requests send no Accept-Encoding), no cookie engine, no .netrc, no HTTP/2, IDN or public suffix
# list. AFFRESCOUI_SYSTEM_CURL=ON uses an installed libcurl anywhere.

if(APPLE)
	set(AFFRESCOUI_SYSTEM_CURL_DEFAULT ON)
else()
	set(AFFRESCOUI_SYSTEM_CURL_DEFAULT OFF)
endif()
option(AFFRESCOUI_SYSTEM_CURL "Use the system's libcurl instead of building the pinned one" ${AFFRESCOUI_SYSTEM_CURL_DEFAULT})

set(AFFRESCOUI_CURL_VERSION 8.22.0)
if(AFFRESCOUI_SYSTEM_CURL)
	find_package(CURL 7.85 REQUIRED) # CURLOPT_PROTOCOLS_STR
else()
	block(SCOPE_FOR VARIABLES) # curl's options stay curl's
		set(BUILD_SHARED_LIBS OFF)
		set(BUILD_STATIC_LIBS ON)
		set(BUILD_TESTING OFF)
		set(BUILD_CURL_EXE OFF)
		set(BUILD_LIBCURL_DOCS OFF)
		set(BUILD_MISC_DOCS OFF)
		set(ENABLE_CURL_MANUAL OFF)
		set(PICKY_COMPILER OFF)
		set(HTTP_ONLY ON)
		set(CURL_DISABLE_COOKIES ON)
		set(CURL_DISABLE_NETRC ON)
		set(CURL_DISABLE_ALTSVC ON)
		set(CURL_DISABLE_WEBSOCKETS ON) # the net module frames WebSocket itself, the same way on every platform
		foreach(dependency USE_NGHTTP2 USE_LIBIDN2 CURL_USE_LIBPSL CURL_USE_LIBSSH2 CURL_USE_LIBSSH CURL_USE_GSSAPI
				CURL_ZLIB CURL_BROTLI CURL_ZSTD ENABLE_ARES)
			set(${dependency} OFF)
		endforeach()
		if(WIN32)
			set(CURL_USE_SCHANNEL ON)
			set(CURL_USE_OPENSSL OFF)
		else()
			set(CURL_USE_OPENSSL ON)
		endif()
		if(NOT AFFRESCOUI_INSTALL)
			set(CURL_DISABLE_INSTALL ON)
		endif()
		FetchContent_Declare(curl URL https://curl.se/download/curl-${AFFRESCOUI_CURL_VERSION}.tar.xz
			URL_HASH SHA256=f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7 SYSTEM)
		FetchContent_MakeAvailable(curl)
	endblock()
	set_target_properties(libcurl_static PROPERTIES COMPILE_WARNING_AS_ERROR OFF) # its warnings are not ours to fix
endif()
