/// @file
/// @brief detail::Inspector: the overlay, its trace and the statistics it shows.

#include "pictures/Inspector.hpp"

#include "affresco/render/Label.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>
#include <utility>

namespace affresco::detail {

namespace {

constexpr float infoWidth = 360.0f;    ///< The widest the information gets, in window units.
constexpr float margin = 8.0f;         ///< Between the information and the window's edge.
constexpr double refreshEvery = 500.0; ///< Milliseconds between two readings of the statistics.
constexpr std::size_t shownTrace = 6;  ///< The trace lines the overlay shows.

/// @brief A number with a fixed number of decimals.
/// @param value The number.
/// @return It, with two decimals.
std::string decimals(double value) {
	std::array<char, 32> text{};
	std::snprintf(text.data(), text.size(), "%.2f", value);
	return text.data();
}

/// @brief A rectangle as the overlay shows it.
/// @param rect In window units.
/// @return "x, y, width × height".
std::string bounds(const Rect& rect) {
	const auto whole = [](float value) { return std::to_string(static_cast<long>(value)); };
	return whole(rect.left) + ", " + whole(rect.top) + ", " + whole(rect.right - rect.left) + " x " +
	       whole(rect.bottom - rect.top);
}

/// @brief The name of a key the trace may show: keys that type nothing. A typed character is never named, so a
/// password typed while inspecting stays out of the trace.
/// @param key The key.
/// @return Its name, or "a key".
std::string keyName(Key key) {
	using Named = std::pair<Key, std::string_view>;
	static constexpr std::array<Named, 13> names{{{Key::Escape, "Escape"},
	                                              {Key::Enter, "Enter"},
	                                              {Key::Tab, "Tab"},
	                                              {Key::Backspace, "Backspace"},
	                                              {Key::Delete, "Delete"},
	                                              {Key::Right, "Right"},
	                                              {Key::Left, "Left"},
	                                              {Key::Down, "Down"},
	                                              {Key::Up, "Up"},
	                                              {Key::PageUp, "PageUp"},
	                                              {Key::PageDown, "PageDown"},
	                                              {Key::Home, "Home"},
	                                              {Key::End, "End"}}};
	const auto* const found = std::ranges::find(names, key, &Named::first);
	return std::string(found == names.end() ? "a key" : found->second);
}

/// @brief Joins lines.
/// @param lines The lines.
/// @return Them, one per line.
std::string joined(const std::vector<std::string>& lines) {
	std::string text;
	for(const std::string& line : lines) {
		if(!text.empty()) text += '\n';
		text += line;
	}
	return text;
}

} // namespace

Inspector::~Inspector() {
	if(on()) stop();
}

std::optional<std::string> Inspector::setOn(bool on) {
	if(on == this->on()) return std::nullopt;
	if(on) {
		Window* window = runtime_.window();
		if(!window) return "AffrescoUI screen: the inspector needs a window";
		start(*window);
		return std::nullopt;
	}
	stop();
	return std::nullopt;
}

void Inspector::stop() {
	static_cast<void>(window_->onMouseMove.disconnect(windowSlots_[0]));
	static_cast<void>(window_->onMouseButton.disconnect(windowSlots_[1]));
	static_cast<void>(window_->onScroll.disconnect(windowSlots_[2]));
	static_cast<void>(window_->onKey.disconnect(windowSlots_[3]));
	static_cast<void>(runtime_.onSend.disconnect(runtimeSlots_[0]));
	static_cast<void>(runtime_.onError.disconnect(runtimeSlots_[1]));
	static_cast<void>(versions_.onCommit.disconnect(versionSlots_[0]));
	static_cast<void>(versions_.onDecision.disconnect(versionSlots_[1]));
	window_->cancelTimer(timer_);
	window_->setInspection(nullptr); // frees the highlight and the information
	window_ = nullptr;
	highlight_ = nullptr;
	info_ = nullptr;
	windowSlots_.clear();
	runtimeSlots_.clear();
	versionSlots_.clear();
}

void Inspector::start(Window& window) {
	auto layer = std::make_unique<Panel>();
	layer->setColor(colors::transparent);
	highlight_ = &layer->add<Panel>();
	highlight_->setColor(Color{0.23f, 0.51f, 0.96f, 0.15f});
	highlight_->setBorder(2.0f, Color::hex(0x3b82f6));
	info_ = &layer->add<Label>();
	info_->setColor(Color{0.07f, 0.09f, 0.13f, 0.92f});
	info_->setTextColor(Color::hex(0xf1f5f9));
	info_->setPadding(8.0f);
	info_->setWrap(true);
	info_->setFitToText(true, infoWidth);
	window_ = &window;
	window.setInspection(std::move(layer));
	connect();
	scheduleRefresh();
	refresh();
}

void Inspector::connect() {
	windowSlots_.push_back(window_->onMouseMove.connect([this](const MouseMoveEvent&) { refresh(); }));
	windowSlots_.push_back(window_->onMouseButton.connect([this](const MouseButtonEvent& event) {
		if(event.action == Action::Press) record("click " + nodeOf(window_->panelAt(event.position)));
	}));
	windowSlots_.push_back(window_->onScroll.connect(
	    [this](const ScrollEvent& event) { record("scroll " + nodeOf(window_->panelAt(event.position))); }));
	windowSlots_.push_back(window_->onKey.connect([this](const KeyEvent& event) {
		if(event.action == Action::Press) record("key " + keyName(event.key));
	}));
	runtimeSlots_.push_back(runtime_.onSend.connect(
	    [this](const std::string& channel, const json&) { record("send to " + channel); })); // never the message
	runtimeSlots_.push_back(runtime_.onError.connect([this](const std::string&) { record("error in a handler"); }));
	versionSlots_.push_back(versions_.onCommit.connect([this](const CommitInfo& commit) {
		record("commit #" + std::to_string(commit.number) + " by " + commit.author);
	}));
	versionSlots_.push_back(versions_.onDecision.connect([this](const Decision& decision) {
		record("proposal " + std::to_string(decision.proposal) + (decision.error ? " rejected" : " merged"));
	}));
}

void Inspector::record(std::string line) {
	trace_.push_back(std::move(line));
	if(trace_.size() > traceLength) trace_.pop_front(); // shown at the next refresh
}

void Inspector::scheduleRefresh() {
	timer_ = window_->setTimer(refreshEvery, [this] {
		refresh();
		scheduleRefresh();
	});
}

void Inspector::setVisible(bool visible) const {
	if(Panel* layer = window_ ? window_->inspection() : nullptr) layer->setVisible(visible);
}

void Inspector::refresh() {
	if(!on()) return;
	const Panel* panel = window_->panelAt(window_->pointer());
	place(panel, joined(describe(panel)));
}

std::vector<std::string> Inspector::describe(const Panel* panel) {
	std::vector<std::string> lines;
	const json about = panel ? runtime_.inspect(*panel) : json();
	if(about.is_object()) {
		lines.push_back(about["node"].get<std::string>() + "  (" + about["type"].get<std::string>() + ")");
		lines.push_back("bounds " + bounds(panel->frame()));
		if(about.contains("bind")) lines.push_back("bind " + about["bind"].dump());
		for(const auto& [key, written] : about["bindings"].items()) lines.push_back(key + " = " + written.dump());
		if(!about["handlers"].empty()) lines.push_back("handlers " + about["handlers"].dump());
		lines.push_back(blameLine(about["node"].get<std::string>()));
	} else {
		lines.emplace_back(panel ? "a panel no node built" : "nothing under the pointer");
	}
	for(std::string& line : statisticsLines()) lines.push_back(std::move(line));
	lines.emplace_back("events:");
	const std::size_t first = trace_.size() > shownTrace ? trace_.size() - shownTrace : 0;
	for(std::size_t index = first; index < trace_.size(); ++index) lines.push_back("  " + trace_[index]);
	return lines;
}

std::string Inspector::blameLine(const std::string& node) const {
	try {
		const CommitInfo commit = versions_.blame(node);
		return "changed in #" + std::to_string(commit.number) + " by " + commit.author + ": " + commit.message;
	} catch(const std::exception&) {
		return "changed in: no history says";
	}
}

std::vector<std::string> Inspector::statisticsLines() const {
	if(!showStatistics_) return {"statistics left out"};
	const FrameStatistics& frame = window_->frameStatistics();
	const format::UpdateStatistics& update = runtime_.lastUpdate();
	return {"frame " + decimals(frame.frameMilliseconds) + " ms, " + std::to_string(frame.drawCalls) + " draw calls",
	        "update " + decimals(update.milliseconds) + " ms, " + std::to_string(update.panelsApplied) +
	            " panels rebuilt, " + std::to_string(update.panelsCreated) + " created"};
}

std::string Inspector::nodeOf(const Panel* panel) {
	const json about = panel ? runtime_.inspect(*panel) : json();
	return about.is_object() ? about["node"].get<std::string>() : "nothing";
}

void Inspector::place(const Panel* panel, const std::string& text) {
	highlight_->setVisible(panel != nullptr);
	if(panel) {
		const Rect& frame = panel->frame();
		highlight_->setPosition(px(frame.left), px(frame.top));
		highlight_->setSize(px(frame.right - frame.left), px(frame.bottom - frame.top));
	}
	info_->setText(text);
	// On the half of the window the pointer isn't on, so it never covers what it describes
	const bool left = window_->pointer().x > static_cast<float>(window_->width()) / 2.0f;
	const float x = left ? margin : std::max(margin, static_cast<float>(window_->width()) - infoWidth - margin);
	info_->setPosition(px(x), px(margin));
}

} // namespace affresco::detail
