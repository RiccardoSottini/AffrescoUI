/// @file
/// @brief Starting libcurl and the options every connection shares.

#include "http/Curl.hpp"

namespace affresco::net::curl {

bool start() noexcept {
	static const bool started = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	return started;
}

void applyCommonOptions(CURL* handle, const ClientOptions& options) noexcept {
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L); // the worker follows redirects within the origin itself
	curl_easy_setopt(handle, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L); // a proxy's CONNECT answer isn't the response
	curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
	curl_easy_setopt(handle, CURLOPT_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2));
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "AffrescoUI");
	if(!options.caFile.empty()) curl_easy_setopt(handle, CURLOPT_CAINFO, options.caFile.c_str());
	if(options.proxy) curl_easy_setopt(handle, CURLOPT_PROXY, options.proxy->c_str()); // "" turns the environment's off
}

bool appendHeader(List& list, const std::string& line) {
	curl_slist* appended = curl_slist_append(list.get(), line.c_str());
	if(!appended) return false;
	if(!list) list.reset(appended); // otherwise curl_slist_append returned the same list, now longer
	return true;
}

} // namespace affresco::net::curl
