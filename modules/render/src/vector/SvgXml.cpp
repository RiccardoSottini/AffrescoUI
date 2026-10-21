/// @file
/// @brief vector::XmlReader: tags and attributes, with the five predefined entities replaced.

#include "SvgXml.hpp"

#include <array>
#include <cctype>
#include <stdexcept>

namespace affresco::vector {

namespace {

constexpr std::size_t attributeLimit = 256; ///< The most attributes a tag has.

/// @brief Replaces the predefined entities (&amp; &lt; &gt; &quot; &apos;); others are kept as they are.
/// @param text The text.
/// @return It, replaced.
std::string unescape(std::string_view text) {
	static constexpr std::array<std::pair<std::string_view, char>, 5> entities{
	    {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}}};
	std::string result;
	result.reserve(text.size());
	for(std::size_t at = 0; at < text.size();) {
		bool replaced = false;
		for(const auto& [entity, character] : entities)
			if(text.substr(at, entity.size()) == entity) {
				result += character;
				at += entity.size();
				replaced = true;
				break;
			}
		if(!replaced) result += text[at++];
	}
	return result;
}

/// @brief Tells whether a character may be in a name.
[[nodiscard]] bool isNameCharacter(char character) noexcept {
	return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' || character == '_' ||
	       character == ':' || character == '.';
}

} // namespace

std::optional<XmlTag> XmlReader::next() {
	while(true) {
		const std::size_t open = text_.find('<', at_);
		if(open == std::string_view::npos) return std::nullopt;
		at_ = open;
		if(skip("<!--", "-->") || skip("<![CDATA[", "]]>") || skip("<?", "?>") || skip("<!", ">")) continue;
		return tag();
	}
}

bool XmlReader::skip(std::string_view start, std::string_view end) {
	if(text_.substr(at_, start.size()) != start) return false;
	const std::size_t close = text_.find(end, at_ + start.size());
	if(close == std::string_view::npos) fail("an unclosed " + std::string(start));
	at_ = close + end.size();
	return true;
}

XmlTag XmlReader::tag() {
	XmlTag result;
	++at_; // the '<'
	if(at_ < text_.size() && text_[at_] == '/') {
		result.closing = true;
		++at_;
	}
	result.name = name();
	if(result.name.empty()) fail("a tag without a name");
	while(true) {
		skipSpace();
		if(at_ >= text_.size()) fail("an unclosed tag");
		if(text_[at_] == '>') {
			++at_;
			return result;
		}
		if(text_.substr(at_, 2) == "/>") {
			at_ += 2;
			result.selfClosing = true;
			return result;
		}
		if(result.closing) fail("a closing tag with attributes");
		attribute(result);
	}
}

void XmlReader::attribute(XmlTag& tag) {
	const std::string_view key = name();
	if(key.empty()) fail("a broken attribute");
	skipSpace();
	if(at_ >= text_.size() || text_[at_] != '=') fail("an attribute without a value");
	++at_;
	skipSpace();
	const char quote = at_ < text_.size() ? text_[at_] : '\0';
	if(quote != '"' && quote != '\'') fail("an attribute value without quotes");
	const std::size_t close = text_.find(quote, at_ + 1);
	if(close == std::string_view::npos) fail("an unclosed attribute value");
	if(tag.attributes.size() >= attributeLimit) fail("too many attributes");
	tag.attributes.emplace_back(key, unescape(text_.substr(at_ + 1, close - at_ - 1)));
	at_ = close + 1;
}

std::string_view XmlReader::name() {
	const std::size_t start = at_;
	while(at_ < text_.size() && isNameCharacter(text_[at_])) ++at_;
	return text_.substr(start, at_ - start);
}

void XmlReader::skipSpace() noexcept {
	while(at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_])) != 0) ++at_;
}

void XmlReader::fail(std::string_view problem) const {
	throw std::runtime_error("AffrescoUI: broken SVG (" + std::string(problem) + " at character " +
	                         std::to_string(at_) + ")");
}

} // namespace affresco::vector
