/// @file
/// @brief What the workspace draws of a deck: theme colours resolved, masters and layouts under the slides.

#include "DeckView.hpp"

#include <algorithm>
#include <cmath>

namespace sinopia {

using affresco::json;

namespace {

/// @brief The text a theme's "text" token gives, or a dark grey.
/// @param deck The deck.
/// @return The colour.
[[nodiscard]] std::string textColor(const Deck& deck) { return resolveDeckColor(deck, "@text", "#1f2937"); }

/// @brief Resolves an object's colours and adds what the screen draws it with.
/// @param deck The deck.
/// @param object The object.
/// @return It as the workspace draws it.
[[nodiscard]] json renderObject(const Deck& deck, const SlideObject& object) {
	json value = writeObject(object);
	value["fill"] = resolveDeckColor(deck, object.fill, "transparent");
	value["stroke"] = resolveDeckColor(deck, object.stroke, "transparent");
	if(object.stroke.empty()) value["strokeWidth"] = 0.0f;
	const std::string text = resolveDeckColor(deck, object.textColor, textColor(deck));
	value["textColor"] = text;
	if(object.gradient)
		value["gradient"] = {{"from", resolveDeckColor(deck, object.gradient->from, "transparent")},
		                     {"to", resolveDeckColor(deck, object.gradient->to, "transparent")},
		                     {"angle", object.gradient->angle}};
	if(object.shadow) value["shadow"]["color"] = resolveDeckColor(deck, object.shadow->color, "#00000040");
	const auto size = [&object](float wanted) { return std::round(wanted * object.fit * 2.0f) / 2.0f; };
	json runs = json::array();
	if(object.runs.empty() && !object.text.empty())
		runs.push_back({{"text", object.text},
		                {"bold", object.bold},
		                {"italic", object.italic},
		                {"size", size(object.textSize)},
		                {"color", text}});
	for(const TextRun& run : object.runs)
		runs.push_back({{"text", run.text},
		                {"bold", run.bold || object.bold},
		                {"italic", run.italic || object.italic},
		                {"size", size(run.size > 0.0f ? run.size : object.textSize)},
		                {"color", resolveDeckColor(deck, run.color, text)}});
	value["runs"] = std::move(runs);
	value["fontSize"] = size(object.textSize);
	value["viewBox"] = "0 0 " + json(object.view.first).dump() + " " + json(object.view.second).dump();
	json children = json::array();
	for(const SlideObject& child : object.children) children.push_back(renderObject(deck, child));
	value["children"] = std::move(children);
	return value;
}

/// @brief The text of an object and its group's children, lines joined.
/// @param object The object.
/// @return It.
[[nodiscard]] std::string plainText(const SlideObject& object) {
	std::string text = object.text;
	for(const TextRun& run : object.runs) text += run.text;
	for(const SlideObject& child : object.children) {
		const std::string inner = plainText(child);
		if(!inner.empty()) text += (text.empty() ? "" : "\n") + inner;
	}
	return text;
}

/// @brief A slide's title and body for the outline: its title placeholder (else its first text), and the rest.
/// @param slide The slide.
/// @return The title and the body.
[[nodiscard]] std::pair<std::string, std::string> outline(const Slide& slide) {
	const auto titled = std::ranges::find(slide.objects, std::string("title"), &SlideObject::placeholder);
	const auto first =
	    titled != slide.objects.end()
	        ? titled
	        : std::ranges::find_if(slide.objects, [](const SlideObject& o) { return !plainText(o).empty(); });
	std::string body;
	for(auto object = slide.objects.begin(); object != slide.objects.end(); ++object) {
		const std::string text = object == first ? std::string() : plainText(*object);
		if(!text.empty()) body += (body.empty() ? "" : "\n") + text;
	}
	return {first != slide.objects.end() ? plainText(*first) : std::string(), body};
}

/// @brief A slide's layout, if it has one, and that layout's master.
/// @param deck The deck.
/// @param slide The slide.
/// @return The layout and the master, either null.
[[nodiscard]] std::pair<const Master*, const Master*> layoutOf(const Deck& deck, const Slide& slide) {
	const auto layout = std::ranges::find(deck.layouts, slide.layout, &Master::id);
	if(layout == deck.layouts.end()) return {nullptr, deck.masters.empty() ? nullptr : &deck.masters.front()};
	const auto master = std::ranges::find(deck.masters, layout->master, &Master::id);
	return {&*layout, master == deck.masters.end() ? nullptr : &*master};
}

/// @brief What shows under a slide: its master's objects, then its layout's that aren't placeholders, locked.
/// @param deck The deck.
/// @param slide The slide.
/// @return Them, with ids of their own ("u1-…").
[[nodiscard]] json underObjects(const Deck& deck, const Slide& slide) {
	const auto [layout, master] = layoutOf(deck, slide);
	json under = json::array();
	for(const Master* source : {master, layout}) {
		if(!source) continue;
		for(const SlideObject& object : source->objects) {
			if(!object.placeholder.empty()) continue;
			json value = renderObject(deck, object);
			value["id"] = "u" + std::to_string(under.size() + 1) + "-" + object.id;
			value["locked"] = true;
			under.push_back(std::move(value));
		}
	}
	return under;
}

/// @brief A slide's background: its own, its layout's, its master's, the theme's "background", or white.
/// @param deck The deck.
/// @param slide The slide.
/// @return The colour.
[[nodiscard]] std::string background(const Deck& deck, const Slide& slide) {
	const auto [layout, master] = layoutOf(deck, slide);
	std::string chosen = slide.background;
	if(chosen.empty() && layout) chosen = layout->background;
	if(chosen.empty() && master) chosen = master->background;
	return resolveDeckColor(deck, chosen.empty() ? "@background" : chosen, "#ffffff");
}

/// @brief The last build step of a slide: the largest order among its objects that build.
/// @param slide The slide.
/// @return It (0: nothing builds).
[[nodiscard]] int lastBuild(const Slide& slide) {
	int last = 0;
	for(const SlideObject& object : slide.objects)
		if(object.build.kind != "none") last = std::max(last, object.build.order);
	return last;
}

} // namespace

std::string resolveDeckColor(const Deck& deck, const std::string& color, const std::string& fallback) {
	if(color.empty()) return fallback;
	if(color.front() != '@') return color;
	const auto token = std::ranges::find(deck.theme, std::string_view(color).substr(1),
	                                     [](const auto& entry) { return std::string_view(entry.first); });
	if(token == deck.theme.end() || token->second.empty() || token->second.front() == '@') return fallback;
	return token->second;
}

json renderDeck(const Deck& deck) {
	const auto [width, height] = slideSize(deck);
	json slides = json::array();
	for(std::size_t i = 0; i < deck.slides.size(); ++i) {
		const Slide& slide = deck.slides[i];
		json objects = json::array();
		for(const SlideObject& object : slide.objects) objects.push_back(renderObject(deck, object));
		const auto [title, body] = outline(slide);
		slides.push_back({{"id", slide.id},
		                  {"index", i},
		                  {"skip", slide.skip},
		                  {"section", slide.section},
		                  {"notes", slide.notes},
		                  {"transition", {{"kind", slide.transition.kind}, {"ms", slide.transition.ms}}},
		                  {"background", background(deck, slide)},
		                  {"title", title},
		                  {"body", body},
		                  {"builds", lastBuild(slide)},
		                  {"under", underObjects(deck, slide)},
		                  {"objects", std::move(objects)}});
	}
	return {{"width", width},
	        {"height", height},
	        {"page", {deck.page.first, deck.page.second}},
	        {"title", deck.title},
	        {"slides", std::move(slides)}};
}

} // namespace sinopia
