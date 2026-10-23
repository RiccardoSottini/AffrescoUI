/// @file
/// @brief The `syntax` provider (Providers.hpp): the syntax engine (affresco/services/Syntax.hpp) for a screen, on
/// worker threads. Each call parses the text it gives; natives keep a SyntaxDocument and follow edits instead.

#include "Providers.hpp"
#include "affresco/services/Syntax.hpp"

namespace affresco::services {

namespace {

constexpr std::size_t mostAtOnce = 4;                     ///< Calls running at once, per service.
constexpr std::size_t largestText = std::size_t{4} << 20; ///< A call's text, at most 4 MiB.

/// @brief Reads a text kept whole, in one chunk.
/// @param text The text.
SyntaxReader readerOf(const std::string& text) {
	return [&text](std::size_t byte, SyntaxPoint) { return std::string_view(text).substr(byte); };
}

/// @brief Parses a call's text in its language, or fails the call.
/// @param call The call: {language, text}.
/// @param text Gets the text.
/// @return The parsed document, or nullptr (the call has failed).
std::unique_ptr<SyntaxDocument> parsed(Call& call, std::string& text) {
	const json& with = call.arguments();
	const std::string language = with.value("language", std::string());
	text = with.value("text", std::string());
	std::unique_ptr<SyntaxDocument> document = SyntaxDocument::create(language);
	if(!document) {
		call.fail("invalid", "no grammar for '" + language + "'");
		return nullptr;
	}
	if(!isUtf8(text)) {
		call.fail("notText", "the text isn't UTF-8");
		return nullptr;
	}
	const SyntaxParse result = document->parse(readerOf(text), text.size());
	if(result != SyntaxParse::Parsed) {
		call.fail(result == SyntaxParse::TooLarge ? "tooLarge" : "overBudget", "the text wasn't parsed");
		return nullptr;
	}
	return document;
}

/// @brief Runs a `highlight` call: {styles, spans: [[line, start, end, style]], complete}.
/// @param call The call.
void highlight(Call& call) {
	std::string text;
	const std::unique_ptr<SyntaxDocument> document = parsed(call, text);
	if(!document) return;
	const json& with = call.arguments();
	std::vector<SyntaxSpan> spans;
	const bool complete = document->highlight(readerOf(text), with.value("first", 0u),
	                                          with.value("last", std::numeric_limits<std::uint32_t>::max() - 1), spans);
	json list = json::array();
	for(const SyntaxSpan& span : spans) list.push_back({span.row, span.start, span.end, span.style});
	call.finish({{"styles", document->styles()}, {"spans", std::move(list)}, {"complete", complete}});
}

/// @brief Runs a `folds` call: {folds: [[first, last]]}.
/// @param call The call.
void folds(Call& call) {
	std::string text;
	const std::unique_ptr<SyntaxDocument> document = parsed(call, text);
	if(!document) return;
	std::vector<SyntaxFold> found;
	document->folds(0, std::numeric_limits<std::uint32_t>::max() - 1, found);
	json list = json::array();
	for(const SyntaxFold& fold : found) list.push_back({fold.first, fold.last});
	call.finish({{"folds", std::move(list)}});
}

/// @brief One `syntax` service: its worker threads.
class SyntaxService final : public Service {
public:
	/// @brief Starts with no calls.
	/// @param now Whether each call's work runs before invoke() returns (ServiceContext::now).
	explicit SyntaxService(bool now) : tasks_(mostAtOnce, now) {}

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		if(method.name == "languages") {
			call->finish({{"languages", SyntaxDocument::languages()}});
			return std::nullopt;
		}
		const bool started = tasks_.run([name = method.name, call](const std::stop_token&) {
			if(name == "highlight") highlight(*call);
			else folds(*call);
		});
		return started ? std::nullopt : std::optional<std::string>("the service is busy (4 calls at once)");
	}

private:
	Tasks tasks_; ///< The calls running.
};

/// @brief The `syntax` provider.
class SyntaxProvider final : public Provider {
public:
	std::string_view name() const override { return "syntax"; }
	std::string_view about() const override { return "text parsed with tree-sitter: highlights and folds"; }
	json configurationSchema() const override {
		return {{"type", "object"}, {"additionalProperties", false}, {"properties", json::object()}};
	}
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		const json text = {{"type", "string"}, {"maxLength", largestText}};
		const json language = {{"type", "string"}, {"maxLength", 64}};
		const json row = {{"type", "integer"}, {"minimum", 0}, {"maximum", 100'000'000}};
		const json parse = {{"type", "object"},
		                    {"required", {"language", "text"}},
		                    {"properties", {{"language", language}, {"text", text}}},
		                    {"additionalProperties", false}};
		json lines = parse;
		lines["properties"].update({{"first", row}, {"last", row}});
		return {{"languages",
		         Effect::Read,
		         true,
		         false,
		         {{"type", "object"}, {"additionalProperties", false}},
		         "the languages with a grammar: {languages}"},
		        {"highlight", Effect::Read, true, false, lines,
		         "a text's highlights, by line (columns in bytes; later runs paint over earlier): {styles, spans: "
		         "[[line, start, end, style]], complete}"},
		        {"folds", Effect::Read, true, false, parse, "a text's folding ranges: {folds: [[first, last]]}"}};
	}
	std::unique_ptr<Service> create(const json& /*configuration*/, ServiceContext context) override {
		return std::make_unique<SyntaxService>(context.now);
	}
};

} // namespace

std::unique_ptr<Provider> makeSyntaxProvider() { return std::make_unique<SyntaxProvider>(); }

} // namespace affresco::services
