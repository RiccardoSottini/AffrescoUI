/// @file
/// @brief What the adapters share (Layout.hpp): the screen they write, its limits, safe text and placeholders.

#include "Layout.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>

namespace affresco::format::adapters::detail {

using expect::fail;

namespace {

/// @brief The colours every converted screen names.
/// @return `{name: "#rrggbb"}`.
[[nodiscard]] json palette() {
	return {{"background", "#f8fafc"},  {"card", "#ffffff"},        {"text", "#0f172a"},
	        {"muted", "#64748b"},       {"accent", "#2563eb"},      {"onAccent", "#ffffff"},
	        {"fieldBorder", "#cbd5e1"}, {"divider", "#e2e8f0"},     {"placeholder", "#fef3c7"},
	        {"placeholderText", "#92400e"}, {"good", "#16a34a"},    {"warning", "#d97706"},
	        {"attention", "#dc2626"}};
}

/// @brief A text size's font name.
/// @param size The size.
/// @return "small", "default", "large" or "title".
[[nodiscard]] std::string_view fontName(TextSize size) noexcept {
	switch(size) {
	case TextSize::Small: return "small";
	case TextSize::Large: return "large";
	case TextSize::Title: return "title";
	case TextSize::Normal: break;
	}
	return "default";
}

/// @brief Whether a path ends with one of the image extensions, in any case.
/// @param path The path.
/// @return Whether it ends with .png, .jpg, .jpeg or .svg.
[[nodiscard]] bool imageExtension(std::string_view path) noexcept {
	constexpr std::array<std::string_view, 4> extensions{".png", ".jpg", ".jpeg", ".svg"};
	return std::ranges::any_of(extensions, [path](std::string_view extension) {
		if(path.size() < extension.size()) return false;
		const std::string_view end(path.end() - static_cast<std::ptrdiff_t>(extension.size()), path.end());
		return std::ranges::equal(end, extension, [](char c, char wanted) {
			return std::tolower(static_cast<unsigned char>(c)) == static_cast<unsigned char>(wanted);
		});
	});
}

} // namespace

Output::Output(Options options) : options_(std::move(options)) { newBuild(); }

void Output::enter(const std::string& at, int depth) {
	if(depth > maxDepth) fail(at, "nests deeper than " + std::to_string(maxDepth) + " elements");
	if(++elements_ > maxElements) fail(at, "more than " + std::to_string(maxElements) + " elements");
}

void Output::warn(const std::string& at, const std::string& message) {
	std::string warning = at + ": " + message;
	if(warnings_.size() < maxElements && std::ranges::find(warnings_, warning) == warnings_.end()) // once per update
		warnings_.push_back(std::move(warning));
}

std::string Output::uniqueId(std::string_view base) {
	const std::string key = stateKey(base);
	std::string id = key;
	for(int n = 2; ids_.contains(id); ++n) id = key + "-" + std::to_string(n);
	ids_.insert(id);
	return id;
}

json Output::literal(const std::string& text) {
	if(!text.contains("{{")) return text;
	const std::string key = "k" + std::to_string(++literals_);
	state_["literals"][key] = text;
	return "{{state.literals." + key + "}}";
}

Built Output::label(const std::string& text, float width, TextSize size, std::string_view color) {
	return boundLabel(literal(text).get<std::string>(), text.size(), width, size, color);
}

Built Output::boundLabel(const std::string& binding, std::size_t characters, float width, TextSize size,
                         std::string_view color) const {
	const float px = pixels(size);
	const float perLine = std::max(1.0f, std::floor(std::max(width, px) / (px * 0.55f)));
	const float lines = std::max(1.0f, std::ceil(static_cast<float>(characters) / perLine));
	Built built{{{"type", "label"}, {"text", binding}, {"textColor", std::string(color)}, {"wrap", true}},
	            std::ceil(lines * px * 1.35f) + 4};
	if(!options_.fontFile.empty()) built.node["font"] = fontName(size);
	built.node["height"] = built.height;
	return built;
}

Built Output::placeholder(const std::string& what, const std::string& at, float width) {
	warn(at, what + " has no equivalent: shown as a placeholder");
	Built built = label("Unsupported: " + what, width - 16, TextSize::Small, "@placeholderText");
	built.height = std::max(32.0f, built.height + 12);
	built.node.update({{"id", uniqueId("placeholder")}, {"height", built.height}, {"padding", 6},
	                   {"color", "@placeholder"}, {"radius", 6}, {"borderWidth", 1}, {"borderColor", "@warning"},
	                   {"tooltip", at}});
	return built;
}

float Output::contentWidth() const noexcept { return std::max(64.0f, options_.width - 32); }

json Output::root(Built body) {
	body.node["id"] = "content";
	return {{"id", "root"},
	        {"color", "@background"},
	        {"children",
	         json::array({{{"type", "column"},
	                       {"id", "surface"},
	                       {"fill", 16},
	                       {"alignItems", "stretch"},
	                       {"scroll", "vertical"},
	                       {"children", json::array({std::move(body.node)})}}})}};
}

json Output::screen(Built body, std::string_view title, const std::string& note) const {
	json document = {{"format", "affrescoui-screen"},
	                 {"version", 2},
	                 {"note", note},
	                 {"window",
	                  {{"title", options_.title.empty() ? std::string(title) : options_.title},
	                   {"width", std::lround(options_.width)},
	                   {"height", std::lround(options_.height)}}},
	                 {"colors", palette()},
	                 {"state", state_}};
	if(!options_.fontFile.empty()) {
		json& fonts = document["fonts"];
		for(const TextSize size : {TextSize::Normal, TextSize::Small, TextSize::Large, TextSize::Title})
			fonts[std::string(fontName(size))] = {{"file", options_.fontFile}, {"size", pixels(size)}};
	}
	document["root"] = root(std::move(body));
	return document;
}

float pixels(TextSize size) noexcept {
	switch(size) {
	case TextSize::Small: return 12;
	case TextSize::Large: return 18;
	case TextSize::Title: return 22;
	case TextSize::Normal: break;
	}
	return 14;
}

Built column(const std::string& id, std::vector<Built> children, float gap) {
	Built built{{{"type", "column"}, {"id", id}, {"gap", gap}, {"alignItems", "stretch"}, {"children", json::array()}},
	            0};
	for(Built& child : children) {
		built.height += child.height;
		built.node["children"].push_back(std::move(child.node));
	}
	if(children.size() > 1) built.height += gap * static_cast<float>(children.size() - 1);
	built.node["height"] = built.height;
	return built;
}

Built row(const std::string& id, std::vector<Built> children, float gap) {
	Built built{{{"type", "row"}, {"id", id}, {"gap", gap}, {"alignItems", "start"}, {"children", json::array()}}, 0};
	for(Built& child : children) {
		built.height = std::max(built.height, child.height);
		if(child.node.contains("width")) { // keeps its own width (a checkbox, an icon)
			built.node["children"].push_back(std::move(child.node));
			continue;
		}
		if(!child.node.contains("grow")) child.node["grow"] = 1;
		child.node["basis"] = 0;
		built.node["children"].push_back(std::move(child.node));
	}
	built.node["height"] = built.height;
	return built;
}

std::string stateKey(std::string_view name) {
	std::string key;
	for(const char c : name.substr(0, 64)) key += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
	if(key.empty() || std::isdigit(static_cast<unsigned char>(key.front()))) key.insert(0, "_");
	return key;
}

std::string statePath(const std::vector<std::string>& segments) {
	std::string path = "state";
	for(const std::string& segment : segments) path += "." + stateKey(segment);
	return path;
}

bool allowedUrl(std::string_view url) noexcept {
	constexpr std::string_view scheme = "https://";
	if(!url.starts_with(scheme) || url.size() <= scheme.size() || url.size() > 2048) return false;
	std::string_view rest = url;
	rest.remove_prefix(scheme.size());
	// "https://example.com@other.example/" goes to other.example: a user name before '@' only hides the host
	std::string_view authority = rest;
	if(const std::size_t end = rest.find_first_of("/?#"); end != std::string_view::npos)
		authority.remove_suffix(rest.size() - end);
	if(rest.front() == '/' || authority.contains('@')) return false;
	return std::ranges::none_of(url, [](char c) {
		const auto byte = static_cast<unsigned char>(c);
		return byte <= 0x20 || byte == 0x7f || std::string_view("\\{}<>\"").contains(c);
	});
}

bool relativeFile(std::string_view path) noexcept {
	if(path.empty() || path.size() > 512 || path.front() == '/' || path.contains("..")) return false;
	const bool plain = std::ranges::none_of(path, [](char c) {
		const auto byte = static_cast<unsigned char>(c);
		return byte < 0x20 || byte == 0x7f || std::string_view("\\:{}<>\"").contains(c);
	});
	return plain && imageExtension(path);
}

std::string textOf(const json& value, const std::string& fallback) {
	if(value.is_string()) return value.get<std::string>();
	if(value.is_number_integer()) return std::to_string(value.get<long long>());
	if(value.is_number()) return value.dump();
	if(value.is_boolean()) return value.get<bool>() ? "true" : "false";
	return fallback;
}

} // namespace affresco::format::adapters::detail
