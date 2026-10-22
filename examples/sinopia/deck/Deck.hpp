#pragma once

/// @file
/// @brief Sinopia's deck model: slides of shapes, text and images, with masters, layouts, sections and a theme, read
/// from and written to JSON within limits.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sinopia {

/// @brief What a deck may hold: a file or a peer's change past these is refused (SECURITY.md, "Limits").
struct DeckLimits {
	std::size_t slides = 500;               ///< The most slides.
	std::size_t objects = 1000;             ///< The most objects a slide, master or layout has, groups' included.
	std::size_t textBytes = 20000;          ///< The longest text of an object, its runs together, in bytes.
	std::size_t notesBytes = 50000;         ///< The longest speaker notes of a slide.
	std::size_t pathBytes = 65536;          ///< The longest path data of a pen path (VectorPath's own limit).
	std::size_t runs = 256;                 ///< The most runs of rich text an object has.
	std::size_t masters = 32;               ///< The most masters, and the most layouts.
	std::size_t sections = 200;             ///< The most sections.
	std::size_t tokens = 64;                ///< The most theme tokens.
	std::size_t idBytes = 64;               ///< The longest id or name.
	std::size_t imageFileBytes = 32u << 20; ///< The largest image file placed on a slide.
	int imageSide = 4096;                   ///< The widest and tallest image placed on a slide, in pixels.
	std::size_t fileBytes = 8u << 20;       ///< The largest deck file read.
	std::size_t exportPages = 500;          ///< The most pages a PDF or PNG export writes (the PDF's own limits too).
};

/// @brief A run of rich text.
struct TextRun {
	std::string text;    ///< Its text.
	bool bold = false;   ///< Bold.
	bool italic = false; ///< Italic.
	float size = 0.0f;   ///< Its size in slide units; 0: the object's.
	std::string color;   ///< Its colour; empty: the object's.

	bool operator==(const TextRun&) const = default; ///< Field by field.
};

/// @brief A linear gradient fill.
struct Gradient {
	std::string from;    ///< The colour where it starts.
	std::string to;      ///< The colour where it ends.
	float angle = 90.0f; ///< Its direction in degrees: 0 left to right, 90 top to bottom.

	bool operator==(const Gradient&) const = default; ///< Field by field.
};

/// @brief A drop shadow.
struct Shadow {
	float x = 0.0f;                  ///< Its offset to the right.
	float y = 4.0f;                  ///< Its offset down.
	float blur = 8.0f;               ///< How soft it is.
	std::string color = "#00000040"; ///< Its colour.

	bool operator==(const Shadow&) const = default; ///< Field by field.
};

/// @brief How an object comes in while presenting.
struct Build {
	std::string kind = "none"; ///< "none", "appear", "fade" or "fly".
	int order = 0;             ///< Its step: builds of order 1 come with the first click, and so on.

	bool operator==(const Build&) const = default; ///< Field by field.
};

/// @brief An object of a slide, a master or a layout.
struct SlideObject {
	std::string id;                   ///< Unique in its slide.
	std::string kind = "rect";        ///< rect, ellipse, polygon, star, line, arrow, path, image, text or group.
	float x = 0.0f;                   ///< Its left, in slide units (in its group's for a group's child).
	float y = 0.0f;                   ///< Its top.
	float w = 100.0f;                 ///< Its width.
	float h = 100.0f;                 ///< Its height.
	float rotate = 0.0f;              ///< Its rotation in degrees, about its centre.
	std::string fill;                 ///< Its fill colour; empty: none.
	std::optional<Gradient> gradient; ///< A gradient fill instead of the colour.
	std::string stroke;               ///< Its outline's colour; empty: none.
	float strokeWidth = 0.0f;         ///< Its outline's width.
	float radius = 0.0f;              ///< A rectangle's corner radius.
	std::optional<Shadow> shadow;     ///< Its shadow.
	int sides = 5;                    ///< A polygon's sides, a star's points.
	float inner = 0.5f;               ///< A star's inner radius, as a fraction of the outer.
	std::string d;                    ///< A pen path's data, in the units of `view`.
	std::pair<float, float> view{100.0f, 100.0f}; ///< The width and height of a pen path's coordinates.
	std::string image;                            ///< An image's file, relative to the deck.
	std::string text;                             ///< A text frame's (or a shape's) text, when it has no runs.
	std::vector<TextRun> runs;                    ///< Rich text, instead of `text`.
	float textSize = 24.0f;                       ///< The text's size.
	std::string textColor;                        ///< The text's colour; empty: the theme's.
	bool bold = false;                            ///< The plain text is bold.
	bool italic = false;                          ///< The plain text is italic.
	std::string align = "left";                   ///< "left", "center" or "right".
	bool autofit = true;                          ///< Whether the text shrinks to fit the frame.
	float fit = 1.0f;                             ///< Computed: the scale the text is drawn at (autofit).
	bool overflow = false;             ///< Computed: whether the text, at that scale, is taller than the frame.
	bool locked = false;               ///< Whether the canvas leaves it in place.
	Build build;                       ///< How it comes in while presenting.
	std::string placeholder;           ///< In a layout: "title", "subtitle" or "body"; on a slide, the one it fills.
	std::vector<SlideObject> children; ///< A group's objects.

	bool operator==(const SlideObject&) const = default; ///< Field by field.
};

/// @brief How a slide replaces the one before it.
struct Transition {
	std::string kind = "none"; ///< "none", "fade", "push", "wipe" or "zoom".
	int ms = 500;              ///< How long it takes.

	bool operator==(const Transition&) const = default; ///< Field by field.
};

/// @brief A slide.
struct Slide {
	std::string id;         ///< Unique in the deck.
	std::string layout;     ///< The layout it was made from (its master's background and objects show under it).
	std::string section;    ///< The section it is in; empty: none.
	bool skip = false;      ///< Whether presenting and exporting leave it out.
	std::string notes;      ///< The speaker notes.
	std::string background; ///< Its own background colour; empty: the master's.
	Transition transition;  ///< How it comes in.
	std::vector<SlideObject> objects; ///< Back to front.

	bool operator==(const Slide&) const = default; ///< Field by field.
};

/// @brief A master or a layout: a background and objects under the slides that use it.
struct Master {
	std::string id;                   ///< Unique among the masters (or the layouts).
	std::string name;                 ///< What the picker shows.
	std::string master;               ///< A layout's master.
	std::string background;           ///< The background colour; empty: the theme's.
	std::vector<SlideObject> objects; ///< Shown under the slide's (a layout's are placeholders slides copy).

	bool operator==(const Master&) const = default; ///< Field by field.
};

/// @brief A named group of slides.
struct Section {
	std::string id;    ///< Unique in the deck.
	std::string title; ///< Its title.

	bool operator==(const Section&) const = default; ///< Field by field.
};

/// @brief A deck of slides, or a document of vector pages.
struct Deck {
	std::string title = "Untitled";                         ///< Its title.
	std::string size = "16:9";                              ///< "16:9", "4:3", "a4", "letter" or "custom".
	std::pair<float, float> page{960.0f, 540.0f};           ///< Computed for all but "custom": the page in points.
	std::vector<std::pair<std::string, std::string>> theme; ///< Theme tokens, as name and value ("accent", "#7c3aed").
	std::vector<Master> masters;                            ///< Masters.
	std::vector<Master> layouts;                            ///< Layouts.
	std::vector<Section> sections;                          ///< Sections.
	std::vector<Slide> slides;                              ///< Slides, in order.

	bool operator==(const Deck&) const = default; ///< Field by field.
};

/// @brief Reads a deck, checking every value and limit: missing values take their defaults, numbers are clamped to
/// sensible ranges, and ids are made unique.
/// @param value The deck as JSON.
/// @param limits What it may hold.
/// @return The deck, with its computed values (page size, text fit) worked out.
/// @throws std::runtime_error If it isn't a deck, or is past a limit.
[[nodiscard]] Deck readDeck(const affresco::json& value, const DeckLimits& limits = {});

/// @brief Writes a deck as JSON, every value included: readDeck() of it gives the same deck.
/// @param deck The deck.
/// @return It.
[[nodiscard]] affresco::json writeDeck(const Deck& deck);

/// @brief Writes one object as JSON, every value included (as writeDeck() does).
/// @param object The object.
/// @return It.
[[nodiscard]] affresco::json writeObject(const SlideObject& object);

/// @brief The size of a deck's slides in slide units: 800 wide for slides, pages fitted into 800 by 600.
/// @param deck The deck.
/// @return The width and height.
[[nodiscard]] std::pair<float, float> slideSize(const Deck& deck) noexcept;

/// @brief Works out a text frame's autofit: the scale its text is drawn at to fit, and whether it overflows. An
/// estimate from the length of its lines (a character is about half its size wide).
/// @param object The object; its `fit` and `overflow` are set.
void fitText(SlideObject& object) noexcept;

/// @brief A new slide made from a layout: its placeholders copied, with fresh ids.
/// @param deck The deck.
/// @param layout The layout's id (the first layout if there is no such one).
/// @param id The new slide's id.
/// @return The slide.
[[nodiscard]] Slide slideFromLayout(const Deck& deck, const std::string& layout, std::string id);

/// @brief A number a document sends, as a float clamped to the floats' range: converting a double past it is
/// undefined, and a document may send one (the deck's own numbers are read and clamped by readDeck()).
/// @param number The number.
/// @return It.
/// @throws affresco::json::type_error If it isn't a number, as json::get() does.
[[nodiscard]] float floatOf(const affresco::json& number);

/// @brief A key's number, as floatOf() gives it, or a fallback without the key: json::value() for a float.
/// @param object The object.
/// @param key The key.
/// @param fallback The number without the key.
/// @return It.
/// @throws affresco::json::type_error If the key's value isn't a number.
[[nodiscard]] float floatAt(const affresco::json& object, const char* key, float fallback);

} // namespace sinopia
