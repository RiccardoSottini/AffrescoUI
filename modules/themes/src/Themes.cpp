/// @file
/// @brief The shipped kits and themes (Themes.hpp): the embedded JSON files, read into one ComponentLibrary.

#include "affresco/themes/Themes.hpp"

#include "Embedded.hpp"

#include <map>
#include <string>

namespace affresco::themes {

namespace {

/// @brief Adds a file's section to the library's, refusing a name another file already defines.
/// @param into The library's section.
/// @param file The file's contents.
/// @param section "components" or "tokens".
/// @param path The file's path, for errors.
void merge(json& into, const json& file, const char* section, std::string_view path) {
	const auto found = file.find(section);
	if(found == file.end()) return;
	for(const auto& [name, value] : expect::object(*found, std::string(path) + "." + section).items()) {
		if(into.contains(name)) throw DocumentError(std::string(path) + ": '" + name + "' is defined twice");
		into[name] = value;
	}
}

/// @brief The library made of the embedded files: "components/*.json" give components, "tokens/*.json" tokens.
class EmbeddedLibrary final : public ComponentLibrary {
public:
	/// @brief Reads every embedded file.
	/// @throws DocumentError If one is malformed or two define one name.
	EmbeddedLibrary() {
		for(const EmbeddedFile& file : embeddedFiles()) {
			const std::string_view text(reinterpret_cast<const char*>(file.bytes.data()), file.bytes.size());
			const json parsed = parseUntrusted(text);
			if(!parsed.is_object()) throw DocumentError(std::string(file.path) + ": expected an object");
			merge(components_, parsed, "components", file.path);
			merge(tokens_, parsed, "tokens", file.path);
		}
		for(const auto& [name, definition] : components_.items()) byName_.emplace(name, &definition);
		for(const auto& [name, token] : tokens_.items()) {
			const bool colour = token.contains("dark");
			plain_[name] = colour ? token["dark"] : token.value("value", json(0));
			themed_[name] =
			    colour ? json{{"dark", token["dark"]}, {"light", token.value("light", token["dark"])}} : plain_[name];
		}
	}

	/// @copydoc ComponentLibrary::component
	[[nodiscard]] const json* component(std::string_view name) const override {
		const auto found = byName_.find(name);
		return found == byName_.end() ? nullptr : found->second;
	}

	/// @copydoc ComponentLibrary::components
	[[nodiscard]] const json& components() const override { return components_; }
	/// @copydoc ComponentLibrary::tokens
	[[nodiscard]] const json& tokens() const override { return tokens_; }
	/// @copydoc ComponentLibrary::tokenDefaults
	[[nodiscard]] const json& tokenDefaults(bool themed) const override { return themed ? themed_ : plain_; }

private:
	json components_ = json::object(); ///< Name → definition.
	json tokens_ = json::object();     ///< Name → {dark, light, about} or {value, about}.
	json plain_ = json::object();      ///< tokenDefaults(false).
	json themed_ = json::object();     ///< tokenDefaults(true).
	/// Each component by name, found in log time (a use looks its component up each time it is built).
	std::map<std::string, const json*, std::less<>> byName_;
};

} // namespace

const ComponentLibrary& library() {
	static const EmbeddedLibrary made;
	return made;
}

void install() { installComponentLibrary(&library()); }

} // namespace affresco::themes
