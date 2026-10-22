/// @file
/// @brief Fuzzes what a debug adapter writes (modules/services/src/DapProtocol.hpp): one message body, read, then
/// bounded and placed as the `dap` service gives it to a screen; and the same bytes as a request's arguments.
///
/// A message is read or refused with a reason; what a screen gets is UTF-8, its lists within their limits, and every
/// source it names says whether it is inside the folder.

#include "../../modules/services/src/DapProtocol.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

namespace {

/// @brief Checks the limits of what a screen gets, at any depth, and that every source it names is placed: a source
/// with a path says whether it is inside the folder, and one inside gives its path under it.
/// @param value The value.
/// @param depth How deep it is.
void checkBounded(const affresco::json& value, int depth) {
	namespace dap = affresco::services::dap;
	if(depth > 64) return; // the reader refuses deeper JSON
	if(value.is_array()) {
		if(value.size() > dap::mostBreakpoints) affresco::fuzz::broken("a list is within its limit");
		for(const affresco::json& item : value) checkBounded(item, depth + 1);
		return;
	}
	if(!value.is_object()) return;
	const auto source = value.find("source");
	if(source != value.end() && source->is_object() && source->contains("inside")) {
		const affresco::json& inside = (*source)["inside"];
		if(!inside.is_boolean()) affresco::fuzz::broken("a placed source says whether it is inside");
		if(inside == true && !source->value("relative", affresco::json()).is_string())
			affresco::fuzz::broken("a source inside gives its path under the folder");
	}
	for(const auto& [key, item] : value.items()) checkBounded(item, depth + 1);
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace dap = affresco::services::dap;
	const std::filesystem::path root("/work");
	const std::string_view body = affresco::fuzz::text(data, size);
	dap::Message message = dap::readMessage(body);
	affresco::json given;
	if(auto* response = std::get_if<dap::Response>(&message)) {
		if(!affresco::fuzz::isUtf8(affresco::json(response->message))) affresco::fuzz::broken("a reason is UTF-8");
		if(response->success) given = dap::boundResponse(response->command, std::move(response->body), root);
	} else if(auto* event = std::get_if<dap::Event>(&message)) {
		given = dap::boundEvent(event->event, std::move(event->body), root);
	} else if(auto* request = std::get_if<dap::Request>(&message)) {
		affresco::json arguments = request->arguments;
		(void)dap::prepareArguments(request->command, arguments, root);
		(void)dap::responseMessage(1, *request, std::nullopt, "not supported").dump();
	} else if(std::get<std::string>(message).empty()) {
		affresco::fuzz::broken("a refusal says why");
	}
	if(!affresco::fuzz::isUtf8(given)) affresco::fuzz::broken("what a screen gets is UTF-8");
	checkBounded(given, 0);
	return 0;
}
