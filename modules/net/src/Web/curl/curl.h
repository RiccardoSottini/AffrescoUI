#pragma once

/// @file
/// @brief The browser build's stand-in for libcurl's header (cmake/AffrescoUIWeb.cmake): the types and cleanups that
/// Curl.hpp and Worker.hpp name, and nothing that transfers. The browser's fetch and WebSocket do that
/// (WorkerWeb.cpp, WebSocketWeb.cpp), so no handle is ever made.

typedef void CURL;            ///< An easy handle (never made here).
typedef void CURLM;           ///< A multi handle (never made here).
typedef long long curl_off_t; ///< A size or an offset.

/// @brief A header list (never made here).
struct curl_slist {
	char* data;              ///< The header line.
	struct curl_slist* next; ///< The next one.
};

/// @brief How a transfer ended, as the browser's fetch reports it to the worker (WorkerWeb.cpp).
typedef enum {
	CURLE_OK = 0,                   ///< A whole response.
	CURLE_COULDNT_CONNECT = 7,      ///< The browser's fetch failed: the network, CORS, or a redirect.
	CURLE_OPERATION_TIMEDOUT = 28,  ///< The request's timeout passed.
	CURLE_ABORTED_BY_CALLBACK = 42, ///< The worker stopped it (a limit was passed).
} CURLcode;

/// @brief Cleans an easy handle up: there is none.
/// @param handle Unused.
static inline void curl_easy_cleanup(CURL* handle) { (void)handle; }
/// @brief Cleans a multi handle up: there is none.
/// @param handle Unused.
static inline void curl_multi_cleanup(CURLM* handle) { (void)handle; }
/// @brief Frees a header list: there is none.
/// @param list Unused.
static inline void curl_slist_free_all(struct curl_slist* list) { (void)list; }
