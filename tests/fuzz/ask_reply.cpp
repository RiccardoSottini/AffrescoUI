/// @file
/// @brief Fuzzes the replies peers send to the screen's own requests (format's Asks, through Runtime::receiveReply()):
/// any JSON, one reply or a list of them, read against a request the screen just sent.
///
/// Besides not crashing, the "requests" variable must stay within its bounds (format/src/assistant/Asks.hpp): its
/// texts cut to their size at a character boundary, so the state that keeps them still writes as UTF-8.

#include "Fuzz.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Panel.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

using affresco::json;

namespace {

/// The most replies one input plays: past a request's 256, so the oldest go.
constexpr std::size_t mostReplies = 300;

/// @brief A screen that asks, and copies the "requests" variable into its state.
/// @return The document.
json askingScreen() {
	return json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "asked": "", "seen": null },
		"actions": {
			"send": [{ "ask": "Make the title blue", "into": "state.asked" }],
			"look": [{ "set": "state.seen", "to": "{{requests}}" }]
		},
		"root": { "type": "panel" }
	})json");
}

/// @brief Plays one reply: one without a "request" answers the screen's.
/// @param runtime The runtime.
/// @param reply The reply.
/// @param id The request the screen sent.
void play(affresco::format::Runtime& runtime, json reply, const std::string& id) {
	if(reply.is_object() && !reply.contains("request")) reply["request"] = id;
	(void)runtime.receiveReply(reply);
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const json input = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(input.is_discarded()) return 0;

	static const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, askingScreen());
	affresco::format::Runtime runtime(repository, {});
	affresco::Panel sender;
	runtime.runAction("send", sender, json::object());
	const std::string id = repository.state()["asked"].get<std::string>();
	if(id.empty()) affresco::fuzz::broken("an ask step gives its request an id");

	if(input.is_array()) {
		std::size_t played = 0;
		for(const json& reply : input)
			if(played++ < mostReplies) play(runtime, reply, id);
	} else {
		play(runtime, input, id);
	}

	runtime.runAction("look", sender, json::object());
	const json& seen = repository.state()["seen"];
	if(!seen.is_array() || seen.size() != 1) affresco::fuzz::broken("the request stays listed");
	for(const json& reply : seen[0]["replies"])
		if(reply["text"].get<std::string>().size() > 65536 || reply["from"].get<std::string>().size() > 256)
			affresco::fuzz::broken("a reply's text and replier are cut to their size");
	try {
		(void)repository.state().dump(); // throws on a text cut inside a character
	} catch(const json::type_error&) {
		affresco::fuzz::broken("a reply cut to its size is still UTF-8");
	}
	return 0;
}
