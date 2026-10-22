/// @file
/// @brief The "carousel" node type: a Carousel whose children are pages shown one at a time, its arrows, dots and
/// colours, and the page shown as its value.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Carousel.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Sets a carousel from its node: loop, arrows, dots and colours, the defaults for those it doesn't give.
/// @param panel The panel, a Carousel.
/// @param values Its keys.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applyCarousel(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* carousel = dynamic_cast<Carousel*>(&panel);
	if(!carousel) return;
	const auto flag = [&](const char* key, bool fallback) {
		return values.contains(key) ? boolean(values[key], path + "." + key) : fallback;
	};
	carousel->setLoop(flag("loop", false));
	carousel->setArrows(flag("arrowButtons", true));
	carousel->setDots(flag("dots", true));
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	carousel->setColors(colorOr("arrowColor", colors::muted), colorOr("dotColor", colors::idle),
	                    colorOr("activeColor", colors::accent));
}

/// @brief Shows a carousel's page: its bound value, else "index", else the first.
/// Its last text is not used: a carousel shows a page.
/// @param panel The panel, a Carousel.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showPage(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
	auto* carousel = dynamic_cast<Carousel*>(&panel);
	if(!carousel) return;
	const json& given = bound ? *bound : properties.value("index", json(0));
	const double index = given.is_number() ? given.get<double>() : 0.0;
	carousel->setIndex(std::isfinite(index) && index > 0.0 ? static_cast<std::size_t>(std::min(index, 1.0e6)) : 0);
}

/// @brief Reports a page the user turned to.
/// @param panel The panel, a Carousel.
/// @param changed Gets its index.
/// @return The connection.
Connection listenToCarousel(Panel& panel, std::function<void(const json&)> changed) {
	auto* carousel = dynamic_cast<Carousel*>(&panel);
	if(!carousel) return 0;
	return carousel->onIndexChanged.connect([changed = std::move(changed)](std::size_t index) { changed(index); });
}

/// @brief Disconnects listenToCarousel().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToCarousel(Panel& panel, Connection connection) {
	if(auto* carousel = dynamic_cast<Carousel*>(&panel)) carousel->onIndexChanged.disconnect(connection);
}

} // namespace

void registerCarouselType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath}, {"index", ValueKind::Number},
	                   {"loop", ValueKind::Boolean},   {"arrowButtons", ValueKind::Boolean},
	                   {"dots", ValueKind::Boolean},   {"arrowColor", ValueKind::Color},
	                   {"dotColor", ValueKind::Color}, {"activeColor", ValueKind::Color}};
	set->apply = applyCarousel;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const Carousel*>(&panel) != nullptr; };
	set->value = {"index", showPage, listenToCarousel, stopListeningToCarousel};
	registry.add({"carousel", "Pages shown one at a time, with arrows, dots and keys to turn them",
	              [] { return std::make_unique<Carousel>(); }, set, false});
}

} // namespace affresco::format
