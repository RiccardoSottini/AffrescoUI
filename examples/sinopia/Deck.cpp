/// @file
/// @brief Sinopia's deck model: reading a deck from JSON within its limits, writing it back, and the computed values
/// (slide and page sizes, text fit, slides from layouts).

#include "Deck.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>

namespace sinopia {

using affresco::json;

namespace {

/// @brief Reads one deck: its limits, and the ids seen in the slide being read.
class Reader {
public:
	/// @brief A reader.
	/// @param limits What the deck may hold.
	explicit Reader(const DeckLimits& limits) noexcept : limits_(limits) {}

	/// @brief Reads a deck.
	/// @param value The JSON.
	/// @return The deck.
	[[nodiscard]] Deck deck(const json& value);

private:
	/// @brief Throws a deck error.
	/// @param where Where, e.g. "slides[2].objects[0].x".
	/// @param problem What is wrong.
	[[noreturn]] static void fail(const std::string& where, std::string_view problem) {
		throw std::runtime_error("Sinopia deck: " + where + ": " + std::string(problem));
	}
	/// @brief The object at a key, or an empty one when it is missing.
	/// @param value The parent.
	/// @param key The key.
	/// @param where Where the parent is.
	/// @return It.
	[[nodiscard]] static const json& nested(const json& value, const char* key, const std::string& where);
	/// @brief The list at a key, or an empty one when it is missing.
	/// @param value The parent.
	/// @param key The key.
	/// @param where Where the parent is.
	/// @param most How many items it may have.
	/// @return It.
	[[nodiscard]] static const json& list(const json& value, const char* key, const std::string& where,
	                                      std::size_t most);
	/// @brief A number, clamped to a range; its default when missing.
	/// @param value The parent.
	/// @param key The key.
	/// @param fallback The default.
	/// @param low The least.
	/// @param high The most.
	/// @param where Where the parent is.
	/// @return It.
	[[nodiscard]] static float number(const json& value, const char* key, float fallback, float low, float high,
	                                  const std::string& where);
	/// @brief A flag; its default when missing.
	/// @param value The parent.
	/// @param key The key.
	/// @param fallback The default.
	/// @param where Where the parent is.
	/// @return It.
	[[nodiscard]] static bool flag(const json& value, const char* key, bool fallback, const std::string& where);
	/// @brief A text of at most a number of bytes; its default when missing.
	/// @param value The parent.
	/// @param key The key.
	/// @param fallback The default.
	/// @param most The most bytes.
	/// @param where Where the parent is.
	/// @return It.
	[[nodiscard]] static std::string text(const json& value, const char* key, std::string fallback, std::size_t most,
	                                      const std::string& where);
	/// @brief A colour: empty, "#rgb", "#rrggbb", "#rrggbbaa" or a theme token "@name".
	/// @param value The parent.
	/// @param key The key.
	/// @param where Where the parent is.
	/// @return It.
	[[nodiscard]] std::string color(const json& value, const char* key, const std::string& where) const;
	/// @brief One of a set of words; the first when missing.
	/// @param value The parent.
	/// @param key The key.
	/// @param words The words allowed.
	/// @param where Where the parent is.
	/// @return It.
	[[nodiscard]] static std::string word(const json& value, const char* key, std::span<const std::string_view> words,
	                                      const std::string& where);
	/// @brief A unique id in the current scope: its own if free, else one made from it.
	/// @param wanted The id asked for (may be empty).
	/// @param prefix What a made id starts with.
	/// @return It.
	[[nodiscard]] std::string uniqueId(std::string wanted, std::string_view prefix);

	/// @brief Reads an object (and a group's children).
	/// @param value The JSON.
	/// @param where Where it is.
	/// @param inGroup Whether it is a group's child.
	/// @return It.
	[[nodiscard]] SlideObject slideObject(const json& value, const std::string& where, bool inGroup);
	/// @brief Reads an object's text, rich or plain.
	/// @param value The JSON.
	/// @param where Where it is.
	/// @param object Where it goes.
	void objectText(const json& value, const std::string& where, SlideObject& object) const;
	/// @brief Reads a list of objects, in a new id scope.
	/// @param value The parent.
	/// @param where Where the parent is.
	/// @return Them.
	[[nodiscard]] std::vector<SlideObject> objects(const json& value, const std::string& where);
	/// @brief Reads a slide.
	/// @param value The JSON.
	/// @param where Where it is.
	/// @return It.
	[[nodiscard]] Slide slide(const json& value, const std::string& where);
	/// @brief Reads a master or a layout.
	/// @param value The JSON.
	/// @param where Where it is.
	/// @return It.
	[[nodiscard]] Master master(const json& value, const std::string& where);

	const DeckLimits& limits_;            ///< What the deck may hold.
	std::set<std::string> ids_;           ///< Ids taken in the current scope.
	std::size_t objectCount_ = 0;         ///< Objects read in the current scope.
	std::vector<std::string> tokenNames_; ///< The theme's token names, for "@name" colours.
};

constexpr std::array<std::string_view, 10> objectKinds{"rect",  "ellipse", "polygon", "star", "line",
                                                       "arrow", "path",    "image",   "text", "group"};
constexpr std::array<std::string_view, 5> sizeKinds{"16:9", "4:3", "a4", "letter", "custom"};
constexpr std::array<std::string_view, 5> transitionKinds{"none", "fade", "push", "wipe", "zoom"};
constexpr std::array<std::string_view, 4> buildKinds{"none", "appear", "fade", "fly"};
constexpr std::array<std::string_view, 3> alignments{"left", "center", "right"};
constexpr std::array<std::string_view, 4> placeholders{"", "title", "subtitle", "body"};
constexpr float coordinateLimit = 100000.0f; ///< How far from the slide an object may be, in slide units.

const json& Reader::nested(const json& value, const char* key, const std::string& where) {
	static const json empty = json::object();
	const auto found = value.find(key);
	if(found == value.end() || found->is_null()) return empty;
	if(!found->is_object()) fail(where + "." + key, "not an object");
	return *found;
}

const json& Reader::list(const json& value, const char* key, const std::string& where, std::size_t most) {
	static const json empty = json::array();
	const auto found = value.find(key);
	if(found == value.end() || found->is_null()) return empty;
	if(!found->is_array()) fail(where + "." + key, "not a list");
	if(found->size() > most) fail(where + "." + key, "more than " + std::to_string(most) + " items");
	return *found;
}

float Reader::number(const json& value, const char* key, float fallback, float low, float high,
                     const std::string& where) {
	const auto found = value.find(key);
	if(found == value.end() || found->is_null()) return fallback;
	if(!found->is_number()) fail(where + "." + key, "not a number");
	const double read = found->get<double>();
	if(!std::isfinite(read)) fail(where + "." + key, "not a finite number");
	return static_cast<float>(std::clamp(read, static_cast<double>(low), static_cast<double>(high)));
}

bool Reader::flag(const json& value, const char* key, bool fallback, const std::string& where) {
	const auto found = value.find(key);
	if(found == value.end() || found->is_null()) return fallback;
	if(!found->is_boolean()) fail(where + "." + key, "not true or false");
	return found->get<bool>();
}

std::string Reader::text(const json& value, const char* key, std::string fallback, std::size_t most,
                         const std::string& where) {
	const auto found = value.find(key);
	if(found == value.end() || found->is_null()) return fallback;
	if(!found->is_string()) fail(where + "." + key, "not a text");
	if(found->get_ref<const std::string&>().size() > most)
		fail(where + "." + key, "longer than " + std::to_string(most) + " bytes");
	return found->get<std::string>();
}

std::string Reader::color(const json& value, const char* key, const std::string& where) const {
	std::string read = text(value, key, {}, limits_.idBytes, where);
	if(read.empty() || read == "transparent") return read;
	if(read.front() == '@') {
		if(!std::ranges::contains(tokenNames_, read.substr(1))) fail(where + "." + key, "no theme token " + read);
		return read;
	}
	const bool hex = read.front() == '#' && (read.size() == 4 || read.size() == 7 || read.size() == 9) &&
	                 std::all_of(read.begin() + 1, read.end(), [](char c) { return std::isxdigit(c) != 0; });
	if(!hex) fail(where + "." + key, "not a colour");
	return read;
}

std::string Reader::word(const json& value, const char* key, std::span<const std::string_view> words,
                         const std::string& where) {
	const std::string read = text(value, key, std::string(words.front()), 32, where);
	if(!std::ranges::contains(words, std::string_view(read))) fail(where + "." + key, "unknown: " + read);
	return read;
}

std::string Reader::uniqueId(std::string wanted, std::string_view prefix) {
	// Ids name panels in the workspace: letters, digits, '-' and '_' only
	std::ranges::replace_if(
	    wanted, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_'; }, '-');
	if(wanted.empty()) wanted = std::string(prefix) + std::to_string(ids_.size() + 1);
	std::string id = wanted;
	for(std::size_t n = 2; ids_.contains(id); ++n) id = wanted + "-" + std::to_string(n);
	ids_.insert(id);
	return id;
}

void Reader::objectText(const json& value, const std::string& where, SlideObject& object) const {
	object.text = text(value, "text", {}, limits_.textBytes, where);
	std::size_t bytes = object.text.size();
	const json& runs = list(value, "runs", where, limits_.runs);
	for(std::size_t i = 0; i < runs.size(); ++i) {
		const std::string at = where + ".runs[" + std::to_string(i) + "]";
		if(!runs[i].is_object()) fail(at, "not an object");
		TextRun run{text(runs[i], "text", {}, limits_.textBytes, at), flag(runs[i], "bold", false, at),
		            flag(runs[i], "italic", false, at), number(runs[i], "size", 0.0f, 0.0f, 256.0f, at),
		            color(runs[i], "color", at)};
		bytes += run.text.size();
		object.runs.push_back(std::move(run));
	}
	if(bytes > limits_.textBytes) fail(where, "text longer than " + std::to_string(limits_.textBytes) + " bytes");
	object.textSize = number(value, "textSize", 24.0f, 4.0f, 256.0f, where);
	object.textColor = color(value, "textColor", where);
	object.bold = flag(value, "bold", false, where);
	object.italic = flag(value, "italic", false, where);
	object.align = word(value, "align", alignments, where);
	object.autofit = flag(value, "autofit", true, where);
}

SlideObject Reader::slideObject(const json& value, const std::string& where, bool inGroup) {
	if(!value.is_object()) fail(where, "not an object");
	if(++objectCount_ > limits_.objects) fail(where, "more than " + std::to_string(limits_.objects) + " objects");
	SlideObject object;
	object.kind = word(value, "kind", objectKinds, where);
	object.id = uniqueId(text(value, "id", {}, limits_.idBytes, where), object.kind);
	object.x = number(value, "x", 0.0f, -coordinateLimit, coordinateLimit, where);
	object.y = number(value, "y", 0.0f, -coordinateLimit, coordinateLimit, where);
	object.w = number(value, "w", 100.0f, 1.0f, coordinateLimit, where);
	object.h = number(value, "h", 100.0f, 1.0f, coordinateLimit, where);
	object.rotate = number(value, "rotate", 0.0f, -360.0f, 360.0f, where);
	object.fill = color(value, "fill", where);
	if(const json& gradient = nested(value, "gradient", where); !gradient.empty())
		object.gradient =
		    Gradient{color(gradient, "from", where + ".gradient"), color(gradient, "to", where + ".gradient"),
			         number(gradient, "angle", 90.0f, -360.0f, 360.0f, where + ".gradient")};
	object.stroke = color(value, "stroke", where);
	object.strokeWidth = number(value, "strokeWidth", 0.0f, 0.0f, 100.0f, where);
	object.radius = number(value, "radius", 0.0f, 0.0f, coordinateLimit, where);
	if(const json& shadow = nested(value, "shadow", where); !shadow.empty()) {
		const std::string at = where + ".shadow";
		object.shadow =
		    Shadow{number(shadow, "x", 0.0f, -100.0f, 100.0f, at), number(shadow, "y", 4.0f, -100.0f, 100.0f, at),
			       number(shadow, "blur", 8.0f, 0.0f, 100.0f, at), color(shadow, "color", at)};
	}
	object.sides = static_cast<int>(number(value, "sides", 5.0f, 3.0f, 64.0f, where));
	object.inner = number(value, "inner", 0.5f, 0.05f, 0.95f, where);
	object.d = text(value, "d", {}, limits_.pathBytes, where);
	const json& view = nested(value, "view", where);
	object.view = {number(view, "w", 100.0f, 1.0f, coordinateLimit, where + ".view"),
	               number(view, "h", 100.0f, 1.0f, coordinateLimit, where + ".view")};
	object.image = text(value, "image", {}, 1024, where);
	if(const std::filesystem::path image(object.image);
	   image.is_absolute() || image.has_root_name() || std::ranges::contains(image, std::filesystem::path("..")))
		fail(where + ".image", "not a file inside the deck's folder");
	objectText(value, where, object);
	object.locked = flag(value, "locked", false, where);
	const json& build = nested(value, "build", where);
	object.build = {word(build, "kind", buildKinds, where + ".build"),
	                static_cast<int>(number(build, "order", 0.0f, 0.0f, 1000.0f, where + ".build"))};
	object.placeholder = word(value, "placeholder", placeholders, where);
	const json& children = list(value, "children", where, limits_.objects);
	if(!children.empty() && (object.kind != "group" || inGroup))
		fail(where + ".children", "only a group has objects, "
		                          "and groups don't nest");
	for(std::size_t i = 0; i < children.size(); ++i)
		object.children.push_back(slideObject(children[i], where + ".children[" + std::to_string(i) + "]", true));
	fitText(object);
	return object;
}

std::vector<SlideObject> Reader::objects(const json& value, const std::string& where) {
	ids_.clear();
	objectCount_ = 0;
	std::vector<SlideObject> read;
	const json& items = list(value, "objects", where, limits_.objects);
	for(std::size_t i = 0; i < items.size(); ++i)
		read.push_back(slideObject(items[i], where + ".objects[" + std::to_string(i) + "]", false));
	return read;
}

Slide Reader::slide(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "not an object");
	Slide slide;
	slide.layout = text(value, "layout", {}, limits_.idBytes, where);
	slide.section = text(value, "section", {}, limits_.idBytes, where);
	slide.skip = flag(value, "skip", false, where);
	slide.notes = text(value, "notes", {}, limits_.notesBytes, where);
	slide.background = color(value, "background", where);
	const json& transition = nested(value, "transition", where);
	slide.transition = {word(transition, "kind", transitionKinds, where + ".transition"),
	                    static_cast<int>(number(transition, "ms", 500.0f, 0.0f, 10000.0f, where + ".transition"))};
	slide.objects = objects(value, where);
	return slide;
}

Master Reader::master(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "not an object");
	Master master;
	master.name = text(value, "name", {}, limits_.idBytes, where);
	master.master = text(value, "master", {}, limits_.idBytes, where);
	master.background = color(value, "background", where);
	master.objects = objects(value, where);
	return master;
}

Deck Reader::deck(const json& value) {
	if(!value.is_object()) fail("deck", "not an object");
	Deck deck;
	deck.title = text(value, "title", "Untitled", 256, "deck");
	deck.size = word(value, "size", sizeKinds, "deck");
	const json& page = nested(value, "page", "deck");
	deck.page = {number(page, "w", 960.0f, 72.0f, 14400.0f, "deck.page"),
	             number(page, "h", 540.0f, 72.0f, 14400.0f, "deck.page")};
	if(deck.size == "16:9") deck.page = {960.0f, 540.0f};
	else if(deck.size == "4:3") deck.page = {960.0f, 720.0f};
	else if(deck.size == "a4") deck.page = {595.0f, 842.0f};
	else if(deck.size == "letter") deck.page = {612.0f, 792.0f};
	const json& theme = nested(value, "theme", "deck");
	if(theme.size() > limits_.tokens) fail("deck.theme", "more than " + std::to_string(limits_.tokens) + " tokens");
	for(const auto& [name, token] : theme.items()) {
		if(name.empty() || name.size() > limits_.idBytes) fail("deck.theme", "a token's name is empty or too long");
		deck.theme.emplace_back(name, text(theme, name.c_str(), {}, limits_.idBytes, "deck.theme"));
		tokenNames_.push_back(name);
	}
	const auto readMasters = [&](const char* key, std::vector<Master>& into) {
		const json& items = list(value, key, "deck", limits_.masters);
		std::set<std::string> taken;
		for(std::size_t i = 0; i < items.size(); ++i) {
			const std::string at = std::string("deck.") + key + "[" + std::to_string(i) + "]";
			into.push_back(master(items[i], at));
			ids_ = std::move(taken);
			into.back().id = uniqueId(text(items[i], "id", {}, limits_.idBytes, at), key);
			taken = std::move(ids_);
		}
	};
	readMasters("masters", deck.masters);
	readMasters("layouts", deck.layouts);
	std::set<std::string> taken;
	const json& sections = list(value, "sections", "deck", limits_.sections);
	for(std::size_t i = 0; i < sections.size(); ++i) {
		const std::string at = "deck.sections[" + std::to_string(i) + "]";
		if(!sections[i].is_object()) fail(at, "not an object");
		ids_ = std::move(taken);
		deck.sections.push_back({uniqueId(text(sections[i], "id", {}, limits_.idBytes, at), "section"),
		                         text(sections[i], "title", {}, 256, at)});
		taken = std::move(ids_);
	}
	taken.clear();
	const json& slides = list(value, "slides", "deck", limits_.slides);
	for(std::size_t i = 0; i < slides.size(); ++i) {
		const std::string at = "deck.slides[" + std::to_string(i) + "]";
		deck.slides.push_back(slide(slides[i], at));
		ids_ = std::move(taken);
		deck.slides.back().id = uniqueId(text(slides[i], "id", {}, limits_.idBytes, at), "slide");
		taken = std::move(ids_);
	}
	return deck;
}

/// @brief Writes a list of objects.
/// @param objects The objects.
/// @return Them as JSON.
json writeObjects(const std::vector<SlideObject>& objects) {
	json list = json::array();
	for(const SlideObject& object : objects) list.push_back(writeObject(object));
	return list;
}

/// @brief Writes masters or layouts.
/// @param masters They.
/// @return Them as JSON.
json writeMasters(const std::vector<Master>& masters) {
	json list = json::array();
	for(const Master& master : masters)
		list.push_back({{"id", master.id},
		                {"name", master.name},
		                {"master", master.master},
		                {"background", master.background},
		                {"objects", writeObjects(master.objects)}});
	return list;
}

} // namespace

json writeObject(const SlideObject& object) {
	json runs = json::array();
	for(const TextRun& run : object.runs)
		runs.push_back(
		    {{"text", run.text}, {"bold", run.bold}, {"italic", run.italic}, {"size", run.size}, {"color", run.color}});
	json children = json::array();
	for(const SlideObject& child : object.children) children.push_back(writeObject(child));
	json value = {{"id", object.id},
	              {"kind", object.kind},
	              {"x", object.x},
	              {"y", object.y},
	              {"w", object.w},
	              {"h", object.h},
	              {"rotate", object.rotate},
	              {"fill", object.fill},
	              {"gradient", nullptr},
	              {"stroke", object.stroke},
	              {"strokeWidth", object.strokeWidth},
	              {"radius", object.radius},
	              {"shadow", nullptr},
	              {"sides", object.sides},
	              {"inner", object.inner},
	              {"d", object.d},
	              {"view", {{"w", object.view.first}, {"h", object.view.second}}},
	              {"image", object.image},
	              {"text", object.text},
	              {"runs", std::move(runs)},
	              {"textSize", object.textSize},
	              {"textColor", object.textColor},
	              {"bold", object.bold},
	              {"italic", object.italic},
	              {"align", object.align},
	              {"autofit", object.autofit},
	              {"fit", object.fit},
	              {"overflow", object.overflow},
	              {"locked", object.locked},
	              {"build", {{"kind", object.build.kind}, {"order", object.build.order}}},
	              {"placeholder", object.placeholder},
	              {"children", std::move(children)}};
	if(object.gradient)
		value["gradient"] = {
		    {"from", object.gradient->from}, {"to", object.gradient->to}, {"angle", object.gradient->angle}};
	if(object.shadow)
		value["shadow"] = {{"x", object.shadow->x},
		                   {"y", object.shadow->y},
		                   {"blur", object.shadow->blur},
		                   {"color", object.shadow->color}};
	return value;
}

Deck readDeck(const json& value, const DeckLimits& limits) { return Reader(limits).deck(value); }

json writeDeck(const Deck& deck) {
	json theme = json::object();
	for(const auto& [name, value] : deck.theme) theme[name] = value;
	json sections = json::array();
	for(const Section& section : deck.sections) sections.push_back({{"id", section.id}, {"title", section.title}});
	json slides = json::array();
	for(const Slide& slide : deck.slides)
		slides.push_back({{"id", slide.id},
		                  {"layout", slide.layout},
		                  {"section", slide.section},
		                  {"skip", slide.skip},
		                  {"notes", slide.notes},
		                  {"background", slide.background},
		                  {"transition", {{"kind", slide.transition.kind}, {"ms", slide.transition.ms}}},
		                  {"objects", writeObjects(slide.objects)}});
	const auto [width, height] = slideSize(deck);
	return {{"title", deck.title},
	        {"size", deck.size},
	        {"page", {{"w", deck.page.first}, {"h", deck.page.second}}},
	        {"width", width},
	        {"height", height},
	        {"theme", std::move(theme)},
	        {"masters", writeMasters(deck.masters)},
	        {"layouts", writeMasters(deck.layouts)},
	        {"sections", std::move(sections)},
	        {"slides", std::move(slides)}};
}

std::pair<float, float> slideSize(const Deck& deck) noexcept {
	const auto [width, height] = deck.page;
	if(deck.size == "16:9" || deck.size == "4:3") return {800.0f, std::round(800.0f * height / width)};
	const float scale = std::min(800.0f / width, 600.0f / height);
	return {std::round(width * scale), std::round(height * scale)};
}

void fitText(SlideObject& object) noexcept {
	object.fit = 1.0f;
	object.overflow = false;
	if(object.kind == "group" || object.kind == "line" || object.kind == "path" || object.kind == "image") return;
	// Lines of the text: its own breaks, and wraps where a line is wider than the frame
	std::vector<std::pair<std::size_t, float>> lines{{0, object.textSize}}; // characters and the tallest size
	const auto add = [&lines](std::string_view text, float size) {
		for(const char c : text) {
			if(c == '\n') lines.emplace_back(0, size);
			else if((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++lines.back().first; // UTF-8 starts only
			lines.back().second = std::max(lines.back().second, size);
		}
	};
	if(object.runs.empty()) add(object.text, object.textSize);
	for(const TextRun& run : object.runs) add(run.text, run.size > 0.0f ? run.size : object.textSize);
	if(lines.size() == 1 && lines.front().first == 0) return;
	const auto heightAt = [&lines, &object](float scale) {
		float height = 0.0f;
		for(const auto& [characters, size] : lines) {
			const float width = static_cast<float>(characters) * size * scale * 0.5f;
			height += std::max(1.0f, std::ceil(width / std::max(1.0f, object.w - 8.0f))) * size * scale * 1.2f;
		}
		return height;
	};
	const float room = std::max(1.0f, object.h - 8.0f);
	if(object.autofit)
		while(object.fit > 0.25f && heightAt(object.fit) > room) object.fit = std::max(0.25f, object.fit - 0.05f);
	object.overflow = heightAt(object.fit) > room;
}

Slide slideFromLayout(const Deck& deck, const std::string& layout, std::string id) {
	Slide slide;
	slide.id = std::move(id);
	const auto found = std::ranges::find(deck.layouts, layout, &Master::id);
	const Master* chosen = found != deck.layouts.end() ? &*found
	                       : deck.layouts.empty()      ? nullptr
	                                                   : &deck.layouts.front();
	if(!chosen) return slide;
	slide.layout = chosen->id;
	for(const SlideObject& placeholder : chosen->objects) {
		SlideObject object = placeholder;
		object.id = slide.id + "-" + placeholder.id;
		slide.objects.push_back(std::move(object));
	}
	return slide;
}

} // namespace sinopia
