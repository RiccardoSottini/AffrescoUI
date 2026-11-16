/// @file
/// @brief format::Verify: the check that a partial update gives what a full one gives.

#include "Verify.hpp"
#include "Engine.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Label.hpp"

#include <cstdlib>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// @brief One property of one panel, as written down.
struct Property {
	std::string name;  ///< The property.
	std::string value; ///< Its value, as text.
};

/// @brief A panel as written down.
struct Written {
	const Panel* panel = nullptr;     ///< The panel.
	std::string place;                ///< Its node's place in the document.
	std::vector<Property> properties; ///< What it shows and holds.
};

/// @brief Writes a value down, whatever bytes its strings hold (the state may hold bytes that aren't UTF-8).
/// @param value The value.
/// @return It as text.
std::string text(const json& value) { return value.dump(-1, ' ', false, json::error_handler_t::replace); }

/// @brief Writes a colour down.
/// @param color The colour.
/// @return It as text.
std::string text(const Color& color) { return std::format("{},{},{},{}", color.r, color.g, color.b, color.a); }

/// @brief Writes a panel tree down, depth first: each record's place, type, key and values; its shown, disabled and
/// focus flags; each panel's visibility and colour; a label's text and text colour; the children in order.
/// @param panel The subtree's root.
/// @param reconciler The records.
/// @param place The place of the nearest panel above with a record, for one without.
/// @param out Gets the panels.
void write(const Panel& panel, Reconciler& reconciler, const std::string& place, std::vector<Written>& out) {
	Written written{&panel, place, {}};
	std::vector<Property>& properties = written.properties;
	if(const Record* record = reconciler.recordOf(&panel)) {
		written.place = record->path;
		properties.push_back({"type", record->type});
		properties.push_back({"key", text(record->key)});
		for(std::size_t i = 0; i < record->values.size(); ++i)
			properties.push_back({"values[" + std::to_string(i) + "]", text(record->values[i])});
		properties.push_back({"shown", record->shown ? "true" : "false"});
		properties.push_back({"disabled", record->disabled ? "true" : "false"});
		properties.push_back({"focused", record->focused ? "true" : "false"});
	}
	properties.push_back({"visible", panel.isVisible() ? "true" : "false"});
	properties.push_back({"color", text(panel.color())});
	if(const auto* label = dynamic_cast<const Label*>(&panel)) {
		properties.push_back({"text", label->text()});
		properties.push_back({"textColor", text(label->textColor())});
	}
	std::string children;
	for(const auto& child : panel.children()) children += std::format("{} ", static_cast<const void*>(child.get()));
	properties.push_back({"children", std::move(children)});
	const std::string at = written.place;
	out.push_back(std::move(written));
	for(const auto& child : panel.children()) write(*child, reconciler, at, out);
}

/// @brief The test-only id whose reads are dropped (testing::dropReadsOf()).
/// @return It.
std::string& dropped() noexcept {
	static std::string id;
	return id;
}

} // namespace

Verify::Verify(Engine& engine) noexcept : engine_(engine) {
	const char* const variable = std::getenv("AFFRESCOUI_VERIFY_UPDATES"); // NOLINT(concurrency-mt-unsafe): once
	on_ = variable && std::string_view(variable) == "1";
}

void Verify::check(Panel& root, const std::function<void()>& fullPass) {
	// The update wrote the state (a control's value written back as its values were applied): the next update brings
	// the panels in line, and is checked in turn
	if(engine_.document.stateChanged()) return;
	Reconciler& reconciler = engine_.reconciler;
	std::vector<Written> partial;
	write(root, reconciler, "root", partial);

	const UpdateStatistics statistics = reconciler.statistics();
	const bool suspended = std::exchange(engine_.handlersSuspended, true);
	std::string failed;
	checking_ = true;
	firstCreated_.clear();
	try {
		fullPass();
	} catch(const DocumentError& error) {
		failed = error.what();
	}
	checking_ = false;
	engine_.handlersSuspended = suspended;
	reconciler.counters() = statistics;

	std::vector<Written> full;
	write(root, reconciler, "root", full);

	const auto differ = [this](const std::string& place, std::string_view property, std::string_view before,
	                           std::string_view after) {
		log(LogLevel::Error, std::format("update check: {}.{}: partial {}, full {}", place, property, before, after));
		engine_.report("update check: " + place + "." + std::string(property) +
		               ": a partial update and a full update differ");
	};
	if(!failed.empty()) return differ("root", "build", "built", failed);
	if(!firstCreated_.empty()) return differ(firstCreated_, "panel", "kept", "created");
	for(std::size_t i = 0; i < partial.size() && i < full.size(); ++i) {
		const Written& a = partial[i];
		const Written& b = full[i];
		if(a.panel != b.panel) return differ(a.place, "children", "kept", "another panel");
		for(std::size_t k = 0; k < a.properties.size() || k < b.properties.size(); ++k) {
			const Property* x = k < a.properties.size() ? &a.properties[k] : nullptr;
			const Property* y = k < b.properties.size() ? &b.properties[k] : nullptr;
			if(x && y && x->name == y->name && x->value == y->value) continue;
			return differ(a.place, x ? x->name : y->name, x ? x->value : "none", y ? y->value : "none");
		}
	}
	if(partial.size() != full.size())
		differ("root", "children", std::to_string(partial.size()), std::to_string(full.size()));
}

namespace testing {

void dropReadsOf(std::string id) { dropped() = std::move(id); }

const std::string& droppedReads() noexcept { return dropped(); }

} // namespace testing

} // namespace affresco::format
