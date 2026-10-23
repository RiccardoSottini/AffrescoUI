/// @file
/// @brief `affrescoui import --from a2ui|adaptive-cards`: a screen file written from another format's document
/// (SCREEN_FORMAT.md, "Other formats"); without `--from`, `import` applies a change set (HistoryCommands.cpp).

#include "Cli.hpp"
#include "JsonText.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/format/Adapters.hpp>

#include <algorithm>
#include <array>
#include <iostream>

namespace affresco::cli {

namespace {

namespace adapters = format::adapters;

/// The options `import --from` takes.
constexpr std::array<std::string_view, 6> foreignOptions = {"--from", "-o", "--font", "--title", "--size", "--revisions"};

/// @brief The window size of `--size WIDTHxHEIGHT`.
/// @param size The value.
/// @param options Where it goes.
/// @throws UsageError If it isn't two numbers from 64 to 8192.
void readSize(const std::string& size, adapters::Options& options) {
	const std::size_t times = size.find('x');
	try {
		std::size_t used = 0;
		options.width = std::stof(size.substr(0, times), &used);
		if(times == std::string::npos || used != times) throw UsageError("");
		options.height = std::stof(size.substr(times + 1), &used);
		if(used != size.size() - times - 1) throw UsageError("");
	} catch(const std::exception&) {
		throw UsageError("--size expects WIDTHxHEIGHT, such as 480x640");
	}
	if(std::min(options.width, options.height) < 64 || std::max(options.width, options.height) > 8192)
		throw UsageError("--size expects each side from 64 to 8192");
}

/// @brief Converts a document in another format.
/// @param from "a2ui" or "adaptive-cards".
/// @param file The document.
/// @param options How the screen is written.
/// @return The conversion.
/// @throws UsageError For another format's name; FileProblem if the document can't be converted.
adapters::Converted convert(const std::string& from, const std::filesystem::path& file, const adapters::Options& options) {
	if(from != "a2ui" && from != "adaptive-cards") throw UsageError("--from expects a2ui or adaptive-cards");
	const std::string text = readFile(file);
	try {
		if(from == "a2ui") return adapters::fromA2ui(text, options);
		if(text.size() > adapters::maxInputBytes)
			throw FileProblem(file.string() + ": larger than " + std::to_string(adapters::maxInputBytes) + " bytes");
		return adapters::fromAdaptiveCard(parseUntrusted(text), options);
	} catch(const json::parse_error& error) {
		throw FileProblem(file.string() + ": not JSON: " + error.what());
	} catch(const DocumentError& error) {
		throw FileProblem(file.string() + ": " + error.what());
	}
}

/// @brief Writes JSON the way AffrescoUI files are written.
/// @param path The file.
/// @param value The value.
void writeJson(const std::filesystem::path& path, const json& value) { writeFile(path, printText(parseText(value.dump()))); }

/// @brief `import --from`: writes the screen, and says what had no equivalent.
/// @param args The arguments.
/// @return Success.
Exit importForeign(std::span<const std::string_view> args) {
	const Arguments arguments(args, foreignOptions, {});
	const std::filesystem::path source = arguments.file();
	adapters::Options options;
	options.fontFile = arguments.value("--font").value_or("");
	options.title = arguments.value("--title").value_or("");
	if(const auto size = arguments.value("--size")) readSize(*size, options);
	const adapters::Converted converted = convert(arguments.value("--from").value_or(""), source, options);
	std::filesystem::path out = arguments.value("-o").value_or("");
	if(out.empty()) out = std::filesystem::path(source).replace_extension(".screen.json");
	writeJson(out, converted.screen);
	for(const std::string& warning : converted.warnings) std::cout << "warning: " << warning << '\n';
	if(const auto revisions = arguments.value("--revisions"))
		writeJson(*revisions, json(converted.revisions));
	else if(!converted.revisions.empty())
		std::cout << converted.revisions.size() << " later updates left out (--revisions FILE writes them)\n";
	std::cout << "wrote " << out.string() << " (" << converted.warnings.size() << " warnings)\n";
	return Exit::Success;
}

} // namespace

Exit importFile(std::span<const std::string_view> args) {
	const bool foreign = std::ranges::any_of(args, [](std::string_view arg) { return arg == "--from"; });
	return foreign ? importForeign(args) : importChanges(args);
}

} // namespace affresco::cli
