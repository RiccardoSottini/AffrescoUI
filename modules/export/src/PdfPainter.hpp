#pragma once

/// @file
/// @brief detail::PdfPainter: a Painter whose calls become a PDF page's drawing operators.

#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"

#include "PdfResources.hpp"

#include <optional>
#include <string>

namespace affresco::detail {

/// @brief Draws into a page's content stream, in window units (the page's base matrix maps them onto the page).
///
/// A panel's clip is set lazily: before its first drawing that isn't a shadow, so shadows are clipped by the parent's
/// area only, as on screen.
class PdfPainter final : public Painter {
public:
	/// @brief A painter over a page's content.
	/// @param content The content stream so far.
	/// @param resources Where fonts, images and transparency states go.
	/// @param limit The largest content stream, in bytes.
	PdfPainter(std::string& content, PdfResources& resources, std::size_t limit);

	/// @brief Clips what is drawn next (but shadows) to a rectangle, inside the current graphics state.
	/// @param rect The panel's bounds.
	void clipTo(const Rect& rect) noexcept { pendingClip_ = rect; }
	/// @brief Sets the pending clip, if any, now.
	void flushClip();
	/// @brief Throws if the content grew past its limit.
	/// @throws std::runtime_error Past it.
	void checkLimit() const;

	/// @brief A filled rectangle.
	void fillRect(const Rect& rect, Color color) override;
	/// @brief Text, as glyphs of the embedded fonts (colour glyphs as images).
	void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) override;
	/// @brief An image, stretched to the rectangle.
	void drawImage(const Bitmap& bitmap, const Rect& rect) override;
	/// @brief A filled rounded rectangle.
	void fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) override;
	/// @brief A rounded outline, inside the rectangle: the outer shape less the inner one.
	void strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) override;
	/// @brief A shadow: layers of the shape, from smaller to larger, whose alphas add up to its colour.
	void drawShadow(const Rect& rect, const CornerRadii& radii, const Shadow& shadow) override;
	/// @brief A mesh, as the outline where its coverage is one half.
	void fillMesh(std::span<const MeshVertex> quads, Vec2 offset, Color color) override;

private:
	/// @brief Fills a path in a colour: q, the alpha's state, the colour, the path, the fill, Q.
	/// @param path The path.
	/// @param color The colour.
	/// @param op The fill operator ("f", "f*").
	void fill(const std::string& path, Color color, std::string_view op);
	/// @brief Sets a colour for filling (and stroking) and its alpha's state.
	/// @param color The colour.
	void setColor(Color color);
	/// @brief Draws the glyphs of shaped text that come from outline faces.
	/// @param font The font.
	/// @param shaped Its shaped text.
	/// @param origin Where it starts.
	void drawOutlineGlyphs(const Font& font, const Font::ShapedText& shaped, Vec2 origin);
	/// @brief Draws the glyphs of shaped text that come from colour faces, as images cut from the atlas.
	/// @param font The font.
	/// @param shaped Its shaped text.
	/// @param origin Where it starts.
	void drawColorGlyphs(const Font& font, const Font::ShapedText& shaped, Vec2 origin);
	/// @brief Draws a named image in a rectangle.
	/// @param name Its resource name.
	/// @param rect Where it goes.
	void placeImage(const std::string& name, const Rect& rect);

	std::string& content_;            ///< The content stream.
	PdfResources& resources_;         ///< Fonts, images and states.
	std::size_t limit_;               ///< The largest content stream.
	std::optional<Rect> pendingClip_; ///< A clip not set yet.
};

} // namespace affresco::detail
