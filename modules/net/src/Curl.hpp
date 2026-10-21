#pragma once

/// @file
/// @brief libcurl as the net module uses it: owners for its handles, starting it once, and the options every
/// connection shares (TLS, proxy, protocols).

#include "affresco/net/Client.hpp"

#include <curl/curl.h>

#include <memory>

namespace affresco::net::curl {

/// @brief Deletes an easy handle.
struct EasyDeleter {
	/// @brief Cleans it up.
	/// @param handle The handle.
	void operator()(CURL* handle) const noexcept { curl_easy_cleanup(handle); }
};
/// @brief Deletes a multi handle.
struct MultiDeleter {
	/// @brief Cleans it up.
	/// @param handle The handle.
	void operator()(CURLM* handle) const noexcept { curl_multi_cleanup(handle); }
};
/// @brief Deletes a header list.
struct ListDeleter {
	/// @brief Frees it.
	/// @param list The list.
	void operator()(curl_slist* list) const noexcept { curl_slist_free_all(list); }
};

using Easy = std::unique_ptr<CURL, EasyDeleter>;          ///< An owned easy handle.
using Multi = std::unique_ptr<CURLM, MultiDeleter>;       ///< An owned multi handle.
using List = std::unique_ptr<curl_slist, ListDeleter>;    ///< An owned header list.

/// @brief Starts libcurl once for the process (it is never cleaned up: handles may live until the process exits).
/// @return Whether it started.
[[nodiscard]] bool start() noexcept;

/// @brief Sets the options every connection shares: TLS 1.2 or later, peer and host verified, the CA file, the
/// proxy, HTTP/1.1, no signals, http and https only, no redirects followed by curl itself.
/// @param handle The easy handle.
/// @param options The client's options.
void applyCommonOptions(CURL* handle, const ClientOptions& options) noexcept;

/// @brief Appends a header to a list.
/// @param list The list (may be empty).
/// @param line "Name: value".
/// @return false if curl had no memory (the list is unchanged).
[[nodiscard]] bool appendHeader(List& list, const std::string& line);

} // namespace affresco::net::curl
