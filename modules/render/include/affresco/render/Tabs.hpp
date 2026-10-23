#pragma once

/// @file
/// @brief Tabs: a strip of tabs that scrolls when they don't fit, reordered by dragging, with close buttons and
/// markers.

#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco {

class Font;

/// @brief One tab of a Tabs strip.
struct Tab {
	std::string id;       ///< What names it in signals; unique within the strip.
	std::string label;    ///< Its text, UTF-8.
	bool marker = false;  ///< Shows a dot (e.g. unsaved changes) where the close button goes, until hovered.
	bool closable = true; ///< Has a close button.

	/// @brief Equal when every field is.
	bool operator==(const Tab&) const = default;
};

/// @brief A tab dragged to another place in its strip.
struct TabMove {
	std::string id;       ///< The tab.
	std::size_t from = 0; ///< Where it was.
	std::size_t to = 0;   ///< Where it goes, counted in the strip without it.
};

/// @brief A row of tabs, one of them active. The tabs are given (setTabs()); the strip reports what the user does
/// (onActivated, onCloseRequested, onMoved) and the owner changes the tabs, so the strip always shows its owner's
/// state.
///
/// When the tabs are wider than the strip, the wheel scrolls it and the active tab is kept in view. A tab has a close
/// button when it is active or hovered; otherwise its marker, if any, shows there. Dragging a tab along the strip
/// moves it. With the keyboard: Left and Right activate the tab before or after, Home and End the first or the last,
/// Delete closes the active one.
class Tabs : public Panel {
public:
	/// @brief An empty strip with the default look.
	Tabs();

	/// @brief Sets the tabs.
	/// @param tabs In order; ids should be unique.
	void setTabs(std::vector<Tab> tabs);
	/// @brief The tabs.
	[[nodiscard]] const std::vector<Tab>& tabs() const noexcept { return tabs_; }
	/// @brief Sets the active tab, without firing onActivated.
	/// @param id Its id; one that isn't a tab's leaves none active.
	void setActive(std::string id);
	/// @brief The active tab's id.
	[[nodiscard]] const std::string& active() const noexcept { return active_; }

	/// @brief Sets the font of the labels.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Sets the colours.
	/// @param text The labels.
	/// @param activeText The active tab's label.
	/// @param activeBackground The active tab's background.
	/// @param accent The line under the active tab, the markers and the drop indicator.
	void setColors(Color text, Color activeText, Color activeBackground, Color accent);
	/// @brief Sets the markers' colour.
	/// @param color The colour, or std::nullopt for the accent's.
	void setMarkerColor(std::optional<Color> color);
	/// @brief Sets whether a marked tab keeps its marker while active, showing its close button only while hovered.
	/// @param keep Whether it does; by default the active tab shows its close button.
	void setKeepMarker(bool keep);

	/// @brief Where a tab dropped at a point would go.
	/// @param position In window units.
	/// @return An index from 0 to the number of tabs.
	[[nodiscard]] std::size_t insertionAt(Vec2 position) const noexcept;
	/// @brief The tab under a point.
	/// @param position In window units.
	/// @return Its index, or the number of tabs for none.
	[[nodiscard]] std::size_t tabAt(Vec2 position) const noexcept;
	/// @brief Shows where something dragged over the strip would be dropped.
	/// @param index An index from insertionAt(), or std::nullopt to hide it.
	void showDropIndicator(std::optional<std::size_t> index);
	/// @brief How far the strip is scrolled, in pixels.
	[[nodiscard]] float scrollPosition() const noexcept { return scroll_; }

	Signal<const std::string&> onActivated;      ///< The user picked a tab (its id).
	Signal<const std::string&> onCloseRequested; ///< The user clicked a tab's close button or pressed Delete.
	Signal<const TabMove&> onMoved;              ///< The user dragged a tab to another place.

protected:
	void arrange() override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void keyEvent(const KeyEvent& event) override;
	void hoverEvent(bool hovered) override;
	bool scrollContent(Vec2 delta) override;
	void paint(Painter& painter) const override;

private:
	/// @brief Where a tab sits in the strip, unscrolled.
	struct Place {
		std::u32string text; ///< Its label, decoded.
		float left = 0.0f;   ///< From the strip's left edge.
		float width = 0.0f;  ///< Its width.
	};

	/// @brief The font the labels use: setFont()'s, or the window's default.
	[[nodiscard]] const Font* font() const noexcept;
	/// @brief Whether a point is on a tab's close button.
	/// @param index The tab.
	/// @param position In window units.
	[[nodiscard]] bool onCloseButton(std::size_t index, Vec2 position) const noexcept;
	/// @brief Activates a tab and fires onActivated if it wasn't active.
	/// @param index The tab.
	void activate(std::size_t index);
	/// @brief The index of the active tab.
	/// @return Its index, or the number of tabs for none.
	[[nodiscard]] std::size_t activeIndex() const noexcept;
	/// @brief Scrolls so a tab is fully inside the strip.
	/// @param index The tab.
	void keepInView(std::size_t index);
	/// @brief Draws a tab.
	/// @param painter Where.
	/// @param font The font.
	/// @param index Which.
	void paintTab(Painter& painter, const Font& font, std::size_t index) const;

	std::vector<Tab> tabs_;                    ///< See tabs().
	std::vector<Place> places_;                ///< Where each tab sits, from the last layout pass.
	std::string active_;                       ///< See active().
	std::shared_ptr<const Font> font_;         ///< See setFont().
	Color text_ = colors::muted;               ///< See setColors().
	Color activeText_ = colors::black;         ///< See setColors().
	Color activeBackground_ = colors::white;   ///< See setColors().
	Color accent_ = colors::accent;            ///< See setColors().
	std::optional<Color> markerColor_;         ///< See setMarkerColor().
	bool keepMarker_ = false;                  ///< See setKeepMarker().
	float scroll_ = 0.0f;                      ///< See scrollPosition().
	float contentWidth_ = 0.0f;                ///< All the tabs' width.
	std::size_t hovered_ = 0;                  ///< The tab under the pointer, or tabs_.size().
	std::size_t pressed_ = 0;                  ///< The tab the left button went down on, or tabs_.size().
	float pressX_ = 0.0f;                      ///< Where it went down.
	bool reordering_ = false;                  ///< The pressed tab is being dragged along the strip.
	std::optional<std::size_t> dropIndicator_; ///< See showDropIndicator().
	bool keepActiveInView_ = true;             ///< The next layout pass scrolls the active tab into view.
};

} // namespace affresco
