/// @file
/// @brief The "images" section: the images a screen edits, each with its size, its fill, where its snapshot goes, the
/// tools it names after the library's gestures, and limits that only lower the library's. A capability: a revision
/// that adds an image, or changes one, is reviewed like any other.

#include "ImageDocuments.hpp"
#include "SectionRegistry.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <limits>

namespace affresco::format {

namespace editing {

namespace {

/// @brief A limit the section may lower: its key and the member it sets.
struct LimitKey {
	std::string_view name;                        ///< Its key in "limits".
	std::size_t (*read)(const imaging::Limits&);  ///< Its value.
	void (*write)(imaging::Limits&, std::size_t); ///< Sets it.
};

/// @brief Makes the LimitKey of an imaging::Limits member.
/// @tparam Member The member.
/// @param name Its key.
/// @return The key.
template <auto Member> [[nodiscard]] constexpr LimitKey limitKey(std::string_view name) {
	return {name, [](const imaging::Limits& limits) { return static_cast<std::size_t>(limits.*Member); },
	        [](imaging::Limits& limits, std::size_t value) {
		        limits.*Member = static_cast<std::remove_reference_t<decltype(limits.*Member)>>(value);
	        }};
}

/// @brief The limits a document may lower, by their keys.
constexpr std::array<LimitKey, 18> limitKeys{
    limitKey<&imaging::Limits::side>("side"),
    limitKey<&imaging::Limits::pixels>("pixels"),
    limitKey<&imaging::Limits::layers>("layers"),
    limitKey<&imaging::Limits::groupDepth>("groupDepth"),
    limitKey<&imaging::Limits::cacheBytes>("cacheBytes"),
    limitKey<&imaging::Limits::gpuBytes>("gpuBytes"),
    limitKey<&imaging::Limits::undoBytes>("undoBytes"),
    limitKey<&imaging::Limits::undoSteps>("undoSteps"),
    limitKey<&imaging::Limits::filterRadius>("filterRadius"),
    limitKey<&imaging::Limits::brushSize>("brushSize"),
    limitKey<&imaging::Limits::imageFileBytes>("imageFileBytes"),
    limitKey<&imaging::Limits::imageFileSide>("imageFileSide"),
    limitKey<&imaging::Limits::imageFilePixels>("imageFilePixels"),
    limitKey<&imaging::Limits::projectFileBytes>("projectFileBytes"),
    limitKey<&imaging::Limits::manifestBytes>("manifestBytes"),
    limitKey<&imaging::Limits::profileBytes>("profileBytes"),
    limitKey<&imaging::Limits::floodPixels>("floodPixels"),
    limitKey<&imaging::Limits::polygonPoints>("polygonPoints"),
};

/// @brief The keys of an image's entry.
constexpr std::array<std::string_view, 8> entryKeys{"width",  "height", "format", "background",
                                                    "into",   "tools",  "tool",   "limits"};

/// @brief Reads "limits": each a known key, a whole number from 1 to the library's own.
/// @param value The object.
/// @param at Its place.
/// @return The library's limits, lowered.
/// @throws DocumentError If a key is unknown or a value would raise a limit.
[[nodiscard]] imaging::Limits readLimits(const json& value, const std::string& at) {
	imaging::Limits limits;
	for(const auto& [key, limit] : expect::object(value, at).items()) {
		const std::string place = at + "." + key;
		const auto found = std::ranges::find(limitKeys, key, &LimitKey::name);
		if(found == limitKeys.end()) expect::fail(place, "unknown limit");
		const std::size_t library = found->read(imaging::Limits{});
		if(!limit.is_number_integer() || limit.get<std::int64_t>() < 1)
			expect::fail(place, "expected a whole number of at least 1");
		const auto lowered = limit.get<std::uint64_t>();
		if(lowered > library)
			expect::fail(place, std::format("a document only lowers the library's limit ({})", library));
		found->write(limits, static_cast<std::size_t>(lowered));
	}
	return limits;
}

/// @brief Reads one tool: {"gesture": <a gesture>, ...options}; the options are checked by setting them.
/// @param name The document's name for it.
/// @param value Its object.
/// @param limits The image's limits.
/// @param at Its place.
/// @return The tool.
/// @throws DocumentError If the gesture is unknown or an option is unknown or malformed.
[[nodiscard]] ImageTool readTool(const std::string& name, const json& value, const imaging::Limits& limits,
                                 const std::string& at) {
	ImageTool tool{name, Gesture::paint, json::object()};
	for(const auto& [key, option] : expect::object(value, at).items()) {
		if(key == "gesture") {
			const auto gesture = gestureOf(expect::string(option, at + ".gesture"));
			if(!gesture) expect::fail(at + ".gesture", "unknown gesture (e.g. \"paint\", \"select-rectangle\")");
			tool.gesture = *gesture;
		} else if(std::ranges::find(optionKeys(), key) == optionKeys().end())
			expect::fail(at + "." + key, "unknown tool option");
		else
			tool.settings[key] = option;
	}
	if(!value.contains("gesture")) expect::fail(at, "a tool needs a \"gesture\"");
	ToolSettings scratch;
	std::optional<imaging::Point> offset;
	try {
		setOptions(scratch, offset, limits, tool.settings);
	} catch(const DocumentError& error) {
		expect::fail(at, error.what());
	}
	return tool;
}

/// @brief Reads "tools", or names every gesture after itself when there is none.
/// @param entry The image's entry.
/// @param limits Its limits.
/// @param at Its place.
/// @return The tools.
/// @throws DocumentError If one is malformed.
[[nodiscard]] std::vector<ImageTool> readTools(const json& entry, const imaging::Limits& limits,
                                               const std::string& at) {
	std::vector<ImageTool> tools;
	if(!entry.contains("tools")) {
		for(const std::string_view name : gestureNames())
			tools.push_back({std::string(name), *gestureOf(name), json::object()});
		return tools;
	}
	const json& list = expect::object(entry["tools"], at + ".tools");
	if(list.empty()) expect::fail(at + ".tools", "expected at least one tool");
	if(list.size() > 64) expect::fail(at + ".tools", "at most 64 tools");
	for(const auto& [name, tool] : list.items())
		tools.push_back(readTool(name, tool, limits, at + ".tools." + name));
	return tools;
}

/// @brief Reads a side: a whole number from 1 to the side limit.
/// @param entry The image's entry.
/// @param key "width" or "height".
/// @param limits Its limits.
/// @param at Its place.
/// @return The side.
/// @throws DocumentError If it is missing or out of bounds.
[[nodiscard]] int readSide(const json& entry, const char* key, const imaging::Limits& limits, const std::string& at) {
	if(!entry.contains(key)) expect::fail(at, std::format("an image needs a \"{}\"", key));
	const json& side = entry[key];
	const std::string place = at + "." + key;
	if(!side.is_number_integer() || side.get<std::int64_t>() < 1 || side.get<std::int64_t>() > limits.side)
		expect::fail(place, std::format("expected a whole number from 1 to {}", limits.side));
	return side.get<int>();
}

/// @brief Reads the parts of an entry besides its tools and limits: format, background, into.
/// @param entry The entry.
/// @param definition Filled in.
/// @param at Its place.
/// @throws DocumentError If one is malformed.
void readLooks(const json& entry, ImageDefinition& definition, const std::string& at) {
	if(entry.contains("format")) {
		const std::string format = expect::string(entry["format"], at + ".format");
		if(format != "rgba8" && format != "rgba16f") expect::fail(at + ".format", "expected \"rgba8\" or \"rgba16f\"");
		definition.format = format == "rgba8" ? imaging::PixelFormat::rgba8 : imaging::PixelFormat::rgba16f;
	}
	if(entry.contains("background")) {
		const std::string text = expect::string(entry["background"], at + ".background");
		if(text == "transparent") definition.background = {0.0f, 0.0f, 0.0f, 0.0f};
		else if(const auto colour = colorOf(text)) definition.background = *colour;
		else expect::fail(at + ".background", "expected #rrggbb or \"transparent\"");
	}
	if(entry.contains("into")) {
		definition.into = expect::string(entry["into"], at + ".into");
		if(!definition.into.starts_with("state.") || definition.into.contains("{{"))
			expect::fail(at + ".into", "expected a literal path under state, e.g. \"state.image\"");
	}
}

} // namespace

ImageDefinition readImageDefinition(const json& value, const std::string& at) {
	for(const auto& [key, unused] : expect::object(value, at).items())
		if(std::ranges::find(entryKeys, key) == entryKeys.end())
			expect::fail(at + "." + key, "unknown property (expected width, height, format, background, into, tools, "
			                             "tool or limits)");
	ImageDefinition definition;
	if(value.contains("limits")) definition.limits = readLimits(value["limits"], at + ".limits");
	definition.width = readSide(value, "width", definition.limits, at);
	definition.height = readSide(value, "height", definition.limits, at);
	if(static_cast<std::size_t>(definition.width) * static_cast<std::size_t>(definition.height) >
	   definition.limits.pixels)
		expect::fail(at, std::format("at most {} pixels", definition.limits.pixels));
	readLooks(value, definition, at);
	definition.tools = readTools(value, definition.limits, at);
	definition.tool = definition.tools.front().name;
	for(const ImageTool& tool : definition.tools)
		if(tool.gesture == Gesture::paint) {
			definition.tool = tool.name;
			break;
		}
	if(value.contains("tool")) {
		definition.tool = expect::string(value["tool"], at + ".tool");
		if(std::ranges::find(definition.tools, definition.tool, &ImageTool::name) == definition.tools.end())
			expect::fail(at + ".tool", "not one of the tools");
	}
	return definition;
}

std::vector<std::pair<std::string_view, std::size_t>> libraryLimits() {
	const imaging::Limits library;
	std::vector<std::pair<std::string_view, std::size_t>> limits;
	for(const LimitKey& key : limitKeys) limits.emplace_back(key.name, key.read(library));
	return limits;
}

} // namespace editing

namespace {

/// @brief Checks the "images" section: an object of at most maxImages images, each read as the runtime reads it.
/// @param value The section.
/// @param name "images".
/// @throws DocumentError If it is malformed.
void checkImages(const json& value, const std::string& name) {
	if(expect::object(value, name).size() > editing::maxImages)
		expect::fail(name, std::format("at most {} images", editing::maxImages));
	for(const auto& [image, definition] : value.items()) {
		if(image.empty() || image.size() > 64 || image.contains('/') || image.contains('.'))
			expect::fail(name + "." + image, "an image's name has 1 to 64 bytes and no \"/\" or \".\"");
		static_cast<void>(editing::readImageDefinition(definition, name + "." + image));
	}
}

} // namespace

SectionType imagesSection() { return {.name = "images", .validate = checkImages, .capability = true}; }

} // namespace affresco::format
