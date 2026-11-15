/// @file
/// @brief Fuzzes a connection's nREPL protocol (modules/net/src/nrepl/EditorLink.hpp) with an editor's messages: the
/// input's first byte chooses how the person answers each pairing question (a bit a question: yes or no), the rest is
/// decoded as bencode, message after message, and each message goes to a peer whose console does nothing. It must not
/// crash, hang or keep more than the limits allow; once the person refused, the peer must answer nothing more.

#include "../../modules/net/src/nrepl/EditorLink.hpp" // by its path: clang-tidy reads this file without its include folders

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using affresco::net::bencode::Value;

/// @brief A link that counts what is sent and notes the close.
class CountingLink final : public affresco::net::EditorLink {
public:
	std::size_t sent = 0; ///< Messages sent.
	bool closed = false;  ///< close() was called.

	/// @brief Counts a message, and checks nothing follows a close. @param message It.
	void send(const Value& message) override {
		if(closed || !message.isDictionary()) std::abort();
		++sent;
	}
	/// @brief Notes the close. @param why Why.
	void close(std::string_view why) override {
		static_cast<void>(why);
		closed = true;
	}
};

/// @brief A console with a program that evaluates nothing and knows a few names.
class QuietConsole final : public affresco::ProgramConsole {
public:
	/// @brief It has a program. @return true.
	[[nodiscard]] bool hasProgram() const override { return true; }
	/// @brief Gives the text back as one value. @param request It. @return That.
	[[nodiscard]] affresco::ConsoleResult evaluate(const affresco::ConsoleRequest& request) override {
		if(request.text.size() % 3U == 0U) return {.values = {}, .error = "no", .line = 0, .column = 0, .proposal = {}};
		return {.values = {"nil"}, .error = {}, .line = 0, .column = 0, .proposal = {}};
	}
	/// @brief Gives up to @p most names. @param prefix Ignored. @param most The most. @return Them.
	[[nodiscard]] std::vector<affresco::ConsoleName> names(std::string_view prefix, std::size_t most) override {
		static_cast<void>(prefix);
		std::vector<affresco::ConsoleName> found(most < 300U ? most : 300U);
		for(auto& name : found) name.name = "a";
		return found;
	}
	/// @brief Knows one name. @param name It. @return It, or nothing.
	[[nodiscard]] std::optional<affresco::ConsoleName> describe(std::string_view name) override {
		if(name != "a") return std::nullopt;
		return affresco::ConsoleName{
		    .name = "a", .kind = "defn", .doc = "d", .file = "f", .line = 1, .arguments = {"[x]"}};
	}
	/// @brief No files. @return None.
	[[nodiscard]] std::vector<std::string> files() const override { return {}; }
	/// @brief Everything shows. @param text It. @return Nothing.
	[[nodiscard]] std::optional<std::string> unshowable(std::string_view text) const override {
		static_cast<void>(text);
		return std::nullopt;
	}
};

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const unsigned answers = data[0];
	unsigned question = 0;
	QuietConsole console;
	affresco::net::EditorServerOptions options;
	affresco::net::EditorShared shared;
	affresco::net::EditorHost host;
	std::vector<std::function<void(bool)>> pending;
	host.console = &console;
	host.pair = [&pending](std::string, std::function<void(bool)> answered) -> std::optional<std::string> {
		pending.push_back(std::move(answered));
		return std::nullopt;
	};
	host.cancelPairing = [&pending] { pending.clear(); };
	host.programFile = [](const std::string& path) { return path; };
	host.pathOf = [](const std::string& file) { return file; };
	CountingLink link;
	auto peer = affresco::net::makeEditorPeer(link, host, options, shared);
	const affresco::net::bencode::Limits limits{
	    .maxBytes = options.maxMessageBytes, .maxDepth = options.maxDepth, .maxKeys = options.maxKeys};
	std::string_view bytes(reinterpret_cast<const char*>(data + 1), size - 1U);
	while(!bytes.empty() && !link.closed) {
		const affresco::net::bencode::Decoded decoded = affresco::net::bencode::decode(bytes, limits);
		if(decoded.status != affresco::net::bencode::Read::Value || decoded.used == 0) break;
		bytes.remove_prefix(decoded.used);
		peer->receive(decoded.value);
		while(!pending.empty() && !link.closed) { // answer each question as the input's first byte says
			const std::function<void(bool)> answered = pending.back();
			pending.pop_back();
			answered(((answers >> (question++ % 8U)) & 1U) != 0U);
		}
	}
	peer->closed();
	if(shared.asking) std::abort(); // a question outlived its connection
	return 0;
}
