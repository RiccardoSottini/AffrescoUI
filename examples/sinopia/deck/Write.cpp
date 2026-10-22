/// @file
/// @brief Sinopia's deck model, written back: a deck and its objects as the JSON readDeck reads.

#include "Deck.hpp"

#include <utility>

namespace sinopia {

using affresco::json;

namespace {

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

} // namespace sinopia
