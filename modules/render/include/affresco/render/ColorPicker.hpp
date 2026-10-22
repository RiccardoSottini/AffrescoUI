#pragma once

/// @file
/// @brief ColorPicker: a colour chosen on a saturation-value square with a hue strip (or a hue wheel with a value
/// strip), an alpha strip, typed fields (hex, RGB, HSL, HSV, OKLCH), swatches and the recent colours.

#include "affresco/render/ColorSpaces.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

namespace detail {
class BitmapSlot;
} // namespace detail

/// @brief A colour picker. Everything works from the keyboard: Tab moves between its parts (then out of it), the
/// arrows move within a part (Shift for larger steps), typing edits a field (Enter or Tab applies it, Escape drops
/// it), Enter applies a swatch. Pressing on the preview and releasing over any panel of the window picks that
/// panel's colour (an eyedropper within the window only).
class ColorPicker : public Panel {
public:
	/// @brief How the hue and saturation are chosen.
	enum class Mode {
		Square, ///< A saturation-value square and a hue strip.
		Wheel,  ///< A hue-saturation wheel and a value strip.
	};

	/// @brief The parts Tab moves between, in order.
	enum class Part : std::uint8_t {
		Plane,    ///< The square or the wheel.
		Strip,    ///< The hue strip (the value strip for a wheel).
		Alpha,    ///< The alpha strip.
		Hex,      ///< The hex field.
		Rgb,      ///< The RGB field (0..255 each).
		Hsl,      ///< The HSL field (degrees, percent, percent).
		Hsv,      ///< The HSV field (degrees, percent, percent).
		Oklch,    ///< The OKLCH field (lightness 0..1, chroma, degrees).
		Swatches, ///< The swatches and the recent colours.
	};

	static constexpr std::size_t maxSwatches = 32; ///< The most swatches setSwatches() keeps.
	static constexpr std::size_t maxRecent = 8;    ///< The most recent colours kept.
	static constexpr std::size_t fieldCount = 5;   ///< The number of typed fields.

	/// @brief An opaque white picker, square mode, with alpha and fields, a transparent background.
	ColorPicker();
	/// @brief Frees the cached bitmaps.
	~ColorPicker() override;

	/// @brief Sets the colour, without firing onValueChanged. The hue is kept when the colour has none (a grey).
	/// @param value The colour.
	void setValue(Color value);
	/// @brief The colour.
	[[nodiscard]] Color value() const noexcept;

	/// @brief Sets how the hue and saturation are chosen.
	/// @param mode The mode.
	void setMode(Mode mode);
	/// @brief The mode.
	[[nodiscard]] Mode mode() const noexcept { return mode_; }
	/// @brief Shows or hides the alpha strip (hidden, the colour stays opaque).
	/// @param shown Whether it shows.
	void setShowsAlpha(bool shown);
	/// @brief Shows or hides the typed fields.
	/// @param shown Whether they show.
	void setShowsFields(bool shown);

	/// @brief Sets the swatches offered under the fields.
	/// @param swatches The colours; only the first maxSwatches are kept.
	void setSwatches(std::span<const Color> swatches);
	/// @brief The swatches.
	[[nodiscard]] std::span<const Color> swatches() const noexcept { return swatches_; }
	/// @brief The colours chosen last, newest first (at most maxRecent).
	[[nodiscard]] std::span<const Color> recentColors() const noexcept { return recent_; }
	/// @brief Adds the current colour to the recent ones (the picker does this when a drag ends or a field applies).
	void rememberValue();

	/// @brief Sets the font of the fields.
	/// @param font The font; nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Sets the colours of the text and of the active part's outline.
	/// @param text The fields' text.
	/// @param accent The active part's outline and the caret.
	void setColors(Color text, Color accent);

	/// @brief The part the keyboard acts on.
	[[nodiscard]] Part activePart() const noexcept { return active_; }
	/// @brief A field's text: what is being typed in the active field, else the colour in that field's form.
	/// @param field Hex, Rgb, Hsl, Hsv or Oklch.
	/// @return The text (ASCII).
	[[nodiscard]] std::string fieldText(Part field) const;
	/// @brief Applies a text to a field as if typed, firing onValueChanged if the colour changed.
	/// @param field Hex, Rgb, Hsl, Hsv or Oklch.
	/// @param text The text, e.g. "#ff8000", "255, 128, 0", "30, 100%, 50%", "0.7 0.15 30".
	/// @return false if the text isn't a colour in that form (nothing changes).
	bool applyFieldText(Part field, std::string_view text);

	Signal<Color> onValueChanged; ///< The user changed the colour (not setValue()).

protected:
	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	/// @brief Where each part is, in window units, for the current frame.
	struct Layout {
		Rect plane;                            ///< The square, or the wheel's bounding square.
		Rect strip;                            ///< The hue (or value) strip.
		Rect alpha;                            ///< The alpha strip; empty when hidden.
		Rect preview;                          ///< The current colour, also the eyedropper's handle.
		std::array<Rect, fieldCount> fields{}; ///< The fields; empty when hidden.
		Rect swatches;                         ///< The swatch area; empty when there are none.
		std::size_t perRow = 1;                ///< Swatches per row.
	};

	/// @brief Computes where the parts are within frame().
	[[nodiscard]] Layout layout() const noexcept;
	/// @brief The font the fields use, or nullptr.
	[[nodiscard]] const Font* font() const noexcept;
	/// @brief The colour from the HSV state and the alpha.
	[[nodiscard]] color::Rgb rgb() const noexcept;
	/// @brief The number of swatches and recent colours shown.
	[[nodiscard]] std::size_t swatchCount() const noexcept { return swatches_.size() + recent_.size(); }
	/// @brief A swatch or recent colour by its position in the swatch area.
	/// @param index Swatches first, then the recent colours.
	[[nodiscard]] Color swatchAt(std::size_t index) const noexcept;
	/// @brief The swatch area's cell for a swatch.
	/// @param area The swatch area.
	/// @param index The swatch's position.
	[[nodiscard]] Rect swatchRect(const Layout& area, std::size_t index) const noexcept;
	/// @brief Whether a part is shown, so Tab stops there.
	/// @param part The part.
	[[nodiscard]] bool hasPart(Part part) const noexcept;
	/// @brief Moves the keyboard to the next or previous shown part.
	/// @param forward Towards the end.
	/// @return false when there is no such part (the focus should leave).
	bool stepPart(bool forward);
	/// @brief Changes the HSV state and alpha, refreshing the field texts and firing onValueChanged.
	/// @param hsv The new HSV.
	/// @param alpha The new alpha.
	void change(color::Hsv hsv, double alpha);
	/// @brief Sets the colour from sRGB, keeping the hue of greys, and fires onValueChanged.
	/// @param srgb The colour.
	/// @param alpha Its opacity.
	void changeRgb(color::Rgb srgb, double alpha);
	/// @brief Rebuilds the field texts from the colour.
	void refreshTexts();
	/// @brief Sets the colour from a pointer position in a part.
	/// @param part The part pressed.
	/// @param position The pointer, in window units.
	void pick(Part part, Vec2 position);
	/// @brief Handles the arrow keys for the active part.
	/// @param event The key.
	/// @return Whether it was handled.
	bool arrowKey(const KeyEvent& event);
	/// @brief Handles a key while a field is active.
	/// @param event The key.
	/// @return Whether it was handled.
	bool fieldKey(const KeyEvent& event);
	/// @brief Applies the text being typed in the active field, if any.
	void commitField();
	/// @brief Picks the colour of the panel under a point (the eyedropper).
	/// @param position In window units.
	void pickFromWindow(Vec2 position);
	/// @brief Draws the plane (square or wheel) and its marker.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintPlane(Painter& painter, const Layout& area) const;
	/// @brief Draws the strips, the preview and their markers.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintStrips(Painter& painter, const Layout& area) const;
	/// @brief Draws the fields.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintFields(Painter& painter, const Layout& area) const;
	/// @brief Draws the swatches and the recent colours.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintSwatches(Painter& painter, const Layout& area) const;

	color::Hsv hsv_{0.0, 0.0, 1.0};                   ///< The colour, as hue, saturation and value.
	double alpha_ = 1.0;                              ///< The opacity.
	Mode mode_ = Mode::Square;                        ///< See setMode().
	bool showsAlpha_ = true;                          ///< See setShowsAlpha().
	bool showsFields_ = true;                         ///< See setShowsFields().
	std::vector<Color> swatches_;                     ///< See setSwatches().
	std::vector<Color> recent_;                       ///< See recentColors().
	std::shared_ptr<const Font> font_;                ///< See setFont().
	Color text_ = colors::black;                      ///< See setColors().
	Color accent_ = colors::accent;                   ///< See setColors().
	Part active_ = Part::Plane;                       ///< See activePart().
	std::array<std::u32string, fieldCount> texts_;    ///< Each field's text for the colour.
	std::u32string editing_;                          ///< What is typed in the active field.
	bool typing_ = false;                             ///< Whether editing_ holds typed text.
	std::size_t swatch_ = 0;                          ///< The swatch the keyboard is on.
	Part dragPart_ = Part::Plane;                     ///< The part a drag started on.
	bool dragging_ = false;                           ///< The button is held on a part.
	bool eyedropping_ = false;                        ///< The button was pressed on the preview.
	std::unique_ptr<detail::BitmapSlot> planeBitmap_; ///< The square or the wheel, cached.
	std::unique_ptr<detail::BitmapSlot> stripBitmap_; ///< The hue or value strip, cached.
	std::unique_ptr<detail::BitmapSlot> alphaBitmap_; ///< The alpha strip, cached.
};

} // namespace affresco
