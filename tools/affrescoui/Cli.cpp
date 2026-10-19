/// @file
/// @brief What the commands share: arguments, files and the application types (Cli.hpp).

#include "Cli.hpp"

#include <affresco/affresco.hpp>

#ifdef AFFRESCOUI_CLI_EXAMPLE_TYPES
#include "CodeEditor.hpp"
#endif

#include <algorithm>
#include <fstream>
#include <sstream>

namespace affresco::cli {

Arguments::Arguments(std::span<const std::string_view> args, std::span<const std::string_view> valued,
                     std::span<const std::string_view> flags) {
	for(std::size_t i = 0; i < args.size(); ++i) {
		const std::string_view arg = args[i];
		if(!arg.starts_with('-') || arg == "-") {
			positional_.emplace_back(arg);
		} else if(std::ranges::find(valued, arg) != valued.end()) {
			if(i + 1 >= args.size()) throw UsageError(std::string(arg) + " needs a value");
			values_[std::string(arg)] = args[++i];
		} else if(std::ranges::find(flags, arg) != flags.end()) {
			values_[std::string(arg)] = "";
		} else {
			throw UsageError("unknown option " + std::string(arg));
		}
	}
}

std::optional<std::string> Arguments::value(std::string_view name) const {
	const auto it = values_.find(name);
	return it != values_.end() ? std::optional(it->second) : std::nullopt;
}

bool Arguments::flag(std::string_view name) const { return values_.contains(name); }

std::filesystem::path Arguments::file() const {
	if(positional_.size() != 1) throw UsageError("expected one screen file");
	return positional_[0];
}

std::string readFile(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	if(!file) throw std::runtime_error("can't read " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

void writeFile(const std::filesystem::path& path, std::string_view content) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if(!file.write(content.data(), static_cast<std::streamsize>(content.size())))
		throw std::runtime_error("can't write " + path.string());
}

void registerApplicationTypes([[maybe_unused]] Screen& screen) {
	screen.setPlaceholderTypes(true); // other apps' types show as plain panels
#ifdef AFFRESCOUI_CLI_EXAMPLE_TYPES
	examples::registerCodeEditor(screen); // the Forge template's "code" type (application layer)
#endif
}

} // namespace affresco::cli
