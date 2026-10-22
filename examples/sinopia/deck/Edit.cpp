/// @file
/// @brief The edits the workspace makes to a deck (`sinopia.edit`).

#include "Edit.hpp"

#include "Arrange.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sinopia {

using affresco::json;

namespace {

/// @brief An edit being made.
struct Editing {
	const json& edit;                   ///< What was asked.
	Deck deck;                          ///< The deck, being changed.
	std::size_t slide = 0;              ///< The slide it is on.
	std::vector<std::string> ids;       ///< The selected objects.
	std::vector<std::string> selection; ///< What to select afterwards.

	/// @brief The slide's objects.
	/// @return They.
	[[nodiscard]] std::vector<SlideObject>& objects() { return deck.slides.at(slide).objects; }
	/// @brief A number the edit gives.
	/// @param key Its key.
	/// @param fallback When it gives none.
	/// @return It.
	[[nodiscard]] float number(const char* key, float fallback) const {
		const json& value = edit.contains(key) ? edit[key] : json();
		return value.is_number() ? floatOf(value) : fallback;
	}
	/// @brief A text the edit gives.
	/// @param key Its key.
	/// @return It, or "".
	[[nodiscard]] std::string text(const char* key) const {
		return edit.contains(key) && edit[key].is_string() ? edit[key].get<std::string>() : std::string();
	}
	/// @brief A flag the edit gives.
	/// @param key Its key.
	/// @return It (false when it gives none).
	[[nodiscard]] bool flag(const char* key) const {
		return edit.contains(key) && edit[key].is_boolean() && edit[key].get<bool>();
	}
};

/// @brief An id no object of a list (nor a group's child) has yet: the prefix and the first free number.
/// @param objects The objects.
/// @param prefix The prefix.
/// @return It.
[[nodiscard]] std::string freeId(const std::vector<SlideObject>& objects, const std::string& prefix) {
	std::set<std::string> taken;
	for(const SlideObject& object : objects) {
		taken.insert(object.id);
		for(const SlideObject& child : object.children) taken.insert(child.id);
	}
	std::size_t n = 1;
	while(taken.contains(prefix + std::to_string(n))) ++n;
	return prefix + std::to_string(n);
}

/// @brief An id no slide of the deck has yet.
/// @param deck The deck.
/// @return It.
[[nodiscard]] std::string freeSlideId(const Deck& deck) {
	std::size_t n = deck.slides.size() + 1;
	while(std::ranges::contains(deck.slides, "s" + std::to_string(n), &Slide::id)) ++n;
	return "s" + std::to_string(n);
}

/// @brief Checks the deck has a slide for an edit of the current one: a deck a document sends may have none.
/// @param editing The edit.
/// @throws std::runtime_error If it has none.
void needSlide(const Editing& editing) {
	if(editing.deck.slides.empty()) throw std::runtime_error("Sinopia: the deck has no slides");
}

/// @brief Adds an object of a kind, in the middle of the slide unless the edit places it.
/// @param editing The edit.
void addObject(Editing& editing) {
	const std::string kind = editing.text("kind").empty() ? "rect" : editing.text("kind");
	const auto [width, height] = slideSize(editing.deck);
	SlideObject object;
	object.kind = kind;
	object.id = freeId(editing.objects(), kind);
	object.w = editing.number("w", kind == "text" ? 320.0f : 160.0f);
	object.h = editing.number("h", kind == "text" ? 80.0f : kind == "line" ? 1.0f : 120.0f);
	object.x = editing.number("x", (width - object.w) / 2.0f);
	object.y = editing.number("y", (height - object.h) / 2.0f);
	object.image = editing.text("image");
	object.text = editing.text("text");
	if(kind == "text" && object.text.empty()) object.text = "Text";
	if(kind != "text" && kind != "image") object.fill = kind == "line" ? "" : "@accent";
	if(kind == "line" || kind == "path") {
		object.stroke = "@accent";
		object.strokeWidth = 3.0f;
	}
	if(kind == "path") object.d = editing.text("d").empty() ? "M0 80C30 0 70 0 100 80" : editing.text("d");
	editing.selection = {object.id};
	editing.objects().push_back(std::move(object));
}

/// @brief Copies the selected objects, a little down and to the right, and selects the copies.
/// @param editing The edit.
void duplicateObjects(Editing& editing) {
	std::vector<SlideObject> copies;
	editing.selection.clear();
	for(const SlideObject& object : editing.objects()) {
		if(!std::ranges::contains(editing.ids, object.id)) continue;
		SlideObject copy = object;
		copy.x += 20.0f;
		copy.y += 20.0f;
		copy.locked = false;
		std::vector<SlideObject> taken = editing.objects();
		taken.insert(taken.end(), copies.begin(), copies.end());
		copy.id = freeId(taken, object.kind);
		for(SlideObject& child : copy.children) child.id = copy.id + "-" + child.id;
		editing.selection.push_back(copy.id);
		copies.push_back(std::move(copy));
	}
	std::ranges::move(copies, std::back_inserter(editing.objects()));
}

/// @brief Applies a canvas's onTransform: each item's place, size and rotation.
/// @param editing The edit.
void transformObjects(Editing& editing) {
	const json& items = editing.edit.contains("items") ? editing.edit["items"] : json::array();
	if(!items.is_array()) throw std::runtime_error("Sinopia: transform needs items");
	for(const json& item : items) {
		if(!item.is_object()) continue;
		const auto found = std::ranges::find(editing.objects(), item.value("id", std::string()), &SlideObject::id);
		if(found == editing.objects().end() || found->locked) continue;
		found->x = floatAt(item, "x", found->x);
		found->y = floatAt(item, "y", found->y);
		found->w = std::max(1.0f, floatAt(item, "width", found->w));
		found->h = std::max(1.0f, floatAt(item, "height", found->h));
		found->rotate = floatAt(item, "rotate", found->rotate);
	}
}

/// @brief The keys `set` may change on an object (not its id, kind or children).
constexpr std::array<std::string_view, 25> objectKeys{
    "x",        "y",         "w",     "h",      "rotate", "fill",    "gradient", "stroke", "strokeWidth",
    "radius",   "shadow",    "sides", "inner",  "d",      "view",    "image",    "text",   "runs",
    "textSize", "textColor", "bold",  "italic", "align",  "autofit", "build"};

/// @brief Sets keys of the selected objects, read again with the deck so every value is checked.
/// @param editing The edit.
void setObjects(Editing& editing) {
	const json& props = editing.edit.contains("props") ? editing.edit["props"] : json::object();
	if(!props.is_object()) throw std::runtime_error("Sinopia: set needs props");
	json deck = writeDeck(editing.deck);
	for(json& object : deck["slides"][editing.slide]["objects"]) {
		if(!std::ranges::contains(editing.ids, object.value("id", std::string()))) continue;
		for(const auto& [key, value] : props.items())
			if(std::ranges::contains(objectKeys, std::string_view(key))) object[key] = value.is_null() ? json() : value;
	}
	editing.deck = readDeck(deck);
}

/// @brief Adds a slide after the current one, from a layout, and shows it.
/// @param editing The edit.
void addSlide(Editing& editing) {
	Slide slide = slideFromLayout(editing.deck, editing.text("layout"), freeSlideId(editing.deck));
	if(!editing.deck.slides.empty()) slide.section = editing.deck.slides[editing.slide].section;
	const std::size_t at = editing.deck.slides.empty() ? 0 : editing.slide + 1;
	editing.deck.slides.insert(editing.deck.slides.begin() + static_cast<std::ptrdiff_t>(at), std::move(slide));
	editing.slide = at;
}

/// @brief Moves a slide from one place to another (both clamped to the deck).
/// @param editing The edit.
void moveSlide(Editing& editing) {
	needSlide(editing);
	std::vector<Slide>& slides = editing.deck.slides;
	const float last = static_cast<float>(slides.size() - 1);
	const auto place = [&](const char* key, std::size_t fallback) { // clamped before it is a whole number
		return static_cast<std::size_t>(
		    std::min(std::max(0.0f, editing.number(key, static_cast<float>(fallback))), last));
	};
	const std::size_t from = place("from", editing.slide);
	const std::size_t to = place("to", from);
	Slide moving = std::move(slides[from]);
	slides.erase(slides.begin() + static_cast<std::ptrdiff_t>(from));
	slides.insert(slides.begin() + static_cast<std::ptrdiff_t>(to), std::move(moving));
	editing.slide = to;
}

/// @brief Sets keys of the slide, read again with the deck.
/// @param editing The edit.
void setSlide(Editing& editing) {
	constexpr std::array<std::string_view, 6> keys{"notes", "background", "transition", "section", "skip", "layout"};
	const json& props = editing.edit.contains("props") ? editing.edit["props"] : json::object();
	json deck = writeDeck(editing.deck);
	for(const auto& [key, value] : props.items())
		if(std::ranges::contains(keys, std::string_view(key))) deck["slides"][editing.slide][key] = value;
	editing.deck = readDeck(deck);
}

/// @brief Sets keys of the deck: its title, size, page and theme.
/// @param editing The edit.
void setDeck(Editing& editing) {
	constexpr std::array<std::string_view, 4> keys{"title", "size", "page", "theme"};
	const json& props = editing.edit.contains("props") ? editing.edit["props"] : json::object();
	json deck = writeDeck(editing.deck);
	for(const auto& [key, value] : props.items())
		if(std::ranges::contains(keys, std::string_view(key))) deck[key] = value;
	editing.deck = readDeck(deck);
}

/// @brief Starts a section at the slide: it and the slides after it in its old section move to the new one.
/// @param editing The edit.
void addSection(Editing& editing) {
	std::size_t n = editing.deck.sections.size() + 1;
	while(std::ranges::contains(editing.deck.sections, "section" + std::to_string(n), &Section::id)) ++n;
	const Section section{"section" + std::to_string(n),
	                      editing.text("title").empty() ? "Section " + std::to_string(n) : editing.text("title")};
	const std::string old = editing.deck.slides.at(editing.slide).section;
	for(std::size_t i = editing.slide; i < editing.deck.slides.size() && editing.deck.slides[i].section == old; ++i)
		editing.deck.slides[i].section = section.id;
	editing.deck.sections.push_back(section);
}

/// @brief The edits on the selected objects, by op.
/// @return They.
[[nodiscard]] const std::vector<std::pair<std::string_view, std::function<void(Editing&)>>>& objectEdits() {
	static const std::vector<std::pair<std::string_view, std::function<void(Editing&)>>> edits{
	    {"align",
		 [](Editing& e) { alignObjects(e.objects(), e.ids, e.text("edge"), e.flag("toSlide"), slideSize(e.deck)); }},
	    {"distribute",
		 [](Editing& e) { distributeObjects(e.objects(), e.ids, !e.edit.contains("across") || e.flag("across")); }},
	    {"reorder", [](Editing& e) { reorderObjects(e.objects(), e.ids, e.text("how")); }},
	    {"nudge", [](Editing& e) { nudgeObjects(e.objects(), e.ids, e.number("x", 0.0f), e.number("y", 0.0f)); }},
	    {"group",
		 [](Editing& e) {
		     const std::string id = freeId(e.objects(), "group");
		     if(groupObjects(e.objects(), e.ids, id)) e.selection = {id};
	     }},
	    {"ungroup", [](Editing& e) { ungroupObjects(e.objects(), e.ids); }},
	    {"add", addObject},
	    {"delete",
		 [](Editing& e) {
		     std::erase_if(e.objects(),
			               [&e](const SlideObject& o) { return !o.locked && std::ranges::contains(e.ids, o.id); });
		     e.selection.clear();
	     }},
	    {"duplicate", duplicateObjects},
	    {"transform", transformObjects},
	    {"set", setObjects},
	    {"lock",
		 [](Editing& e) {
		     for(SlideObject& object : e.objects())
			     if(std::ranges::contains(e.ids, object.id)) object.locked = e.flag("locked");
	     }},
	};
	return edits;
}

/// @brief The edits on the slides and the deck, by op.
/// @return They.
[[nodiscard]] const std::vector<std::pair<std::string_view, std::function<void(Editing&)>>>& slideEdits() {
	static const std::vector<std::pair<std::string_view, std::function<void(Editing&)>>> edits{
	    {"addSlide", addSlide},
	    {"deleteSlide",
		 [](Editing& e) {
		     if(e.deck.slides.size() <= 1) return; // a deck keeps one slide
		     e.deck.slides.erase(e.deck.slides.begin() + static_cast<std::ptrdiff_t>(e.slide));
		     e.slide = std::min(e.slide, e.deck.slides.size() - 1);
	     }},
	    {"duplicateSlide",
		 [](Editing& e) {
		     needSlide(e);
		     Slide copy = e.deck.slides[e.slide];
		     copy.id = freeSlideId(e.deck);
		     e.deck.slides.insert(e.deck.slides.begin() + static_cast<std::ptrdiff_t>(e.slide) + 1, std::move(copy));
		     ++e.slide;
	     }},
	    {"moveSlide", moveSlide},
	    {"setSlide", setSlide},
	    {"addSection", addSection},
	    {"setDeck", setDeck},
	};
	return edits;
}

} // namespace

json editDeck(const json& edit, const DeckLimits& limits) {
	if(!edit.is_object()) throw std::runtime_error("Sinopia: an edit is an object");
	Editing editing{edit, readDeck(edit.contains("deck") ? edit["deck"] : json::object(), limits), 0, {}, {}};
	const json& slide = edit.contains("slide") ? edit["slide"] : json(0);
	const std::size_t count = editing.deck.slides.size();
	editing.slide = slide.is_number_unsigned() || slide.is_number_integer()
	                    ? std::min(static_cast<std::size_t>(std::max<std::int64_t>(0, slide.get<std::int64_t>())),
	                               count == 0 ? 0 : count - 1)
	                    : 0;
	if(edit.contains("ids") && edit["ids"].is_array())
		for(const json& id : edit["ids"])
			if(id.is_string()) editing.ids.push_back(id.get<std::string>());
	const std::string op = editing.text("op");
	const auto run = [&](const auto& edits) {
		const auto found =
		    std::ranges::find(edits, std::string_view(op), &std::ranges::range_value_t<decltype(edits)>::first);
		if(found == edits.end()) return false;
		found->second(editing);
		return true;
	};
	const bool onSlides = run(slideEdits());
	if(!onSlides && count == 0) throw std::runtime_error("Sinopia: the deck has no slides");
	if(!onSlides) editing.selection = editing.ids; // kept unless the edit chooses another
	if(!onSlides && !run(objectEdits())) throw std::runtime_error("Sinopia: no edit " + op);
	const Deck result = readDeck(writeDeck(editing.deck), limits); // every limit, and the text's fit
	return {{"deck", writeDeck(result)}, {"slide", editing.slide}, {"selection", editing.selection}};
}

} // namespace sinopia
