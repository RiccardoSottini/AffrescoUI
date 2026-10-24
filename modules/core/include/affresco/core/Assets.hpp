#pragma once

/// @file
/// @brief Assets: files a revision brings into a document (a picture, a sound, a text), kept by version control with
/// the history, and attachments: files the person gave the app, waiting for a revision to name them
/// (VERSION_CONTROL.md, "Assets"). Their names, types and bounds, and the checks every module shares.

#include "affresco/core/Json.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace affresco {

/// The largest asset, in bytes (32 MiB): a song, a large picture.
inline constexpr std::size_t maxAssetBytes = std::size_t{32} << 20;
/// The most bytes of new assets one revision brings (64 MiB).
inline constexpr std::size_t maxRevisionAssetBytes = std::size_t{64} << 20;
/// The most assets a document has at once.
inline constexpr std::size_t maxAssets = 1000;
/// The most attachments waiting for a revision at once; the oldest goes first to make room.
inline constexpr std::size_t maxAttachments = 32;
/// The most bytes of attachments waiting at once (128 MiB); the oldest go first to make room.
inline constexpr std::size_t maxAttachmentBytes = std::size_t{128} << 20;
/// The longest asset name.
inline constexpr std::size_t maxAssetName = 100;
/// What a document writes before an asset's name where it reads a file: `"file": "asset:moon-sea.svg"`.
inline constexpr std::string_view assetScheme = "asset:";

/// @brief One asset of a document: what its "assets" section says of it.
struct AssetInfo {
	std::string name;   ///< e.g. "beat.mp3": `[a-z0-9][a-z0-9._-]{0,99}`, with the extension of its type.
	std::string type;   ///< Its media type, one of assetTypes().
	std::size_t size{}; ///< Its bytes.
	std::string sha256; ///< The SHA-256 of its bytes (64 hex digits): where the history keeps them.
};

/// @brief A file the person attached (an `attach` step, a drop, Screen::attach), waiting for a revision to name it
/// (`{"name": "beat.mp3", "attachment": "<id>"}`). Agents see this, never the bytes.
struct AttachmentInfo {
	std::string id;                ///< "att-<n>-<first 12 hex digits of its SHA-256>".
	std::string name;              ///< The file's own name, made a valid asset name ("Beat.MP3" gives "beat.mp3").
	std::string type;              ///< Its media type, one of assetTypes().
	std::size_t size{};            ///< Its bytes.
	json details = json::object(); ///< `duration` (seconds, audio), `width` and `height` (pictures), when known.
};

/// A sound described as text, which the audio module renders (audio/Sound.hpp): what an agent writes for a sound, as
/// it writes an SVG for a picture. Its names end with ".sound.json".
inline constexpr std::string_view soundType = "application/vnd.affrescoui.sound+json";

/// @brief What a revision may bring, and the extensions each type's names end with.
struct AssetType {
	std::string_view type;                      ///< The media type.
	std::array<std::string_view, 2> extensions; ///< Its extensions, without the dot ("" for none).
};

/// @brief The types an asset may have, with their extensions.
/// @return image/png, image/jpeg, image/svg+xml, audio/wav, audio/mpeg, audio/flac, application/json, text/plain.
[[nodiscard]] inline const std::array<AssetType, 9>& assetTypes() {
	static constexpr std::array<AssetType, 9> types = {{{"image/png", {"png", ""}},
	                                                    {"image/jpeg", {"jpg", "jpeg"}},
	                                                    {"image/svg+xml", {"svg", ""}},
	                                                    {"audio/wav", {"wav", ""}},
	                                                    {"audio/mpeg", {"mp3", ""}},
	                                                    {"audio/flac", {"flac", ""}},
	                                                    {"application/json", {"json", ""}},
	                                                    {"text/plain", {"txt", "md"}},
	                                                    {soundType, {"sound.json", ""}}}};
	return types;
}

/// @brief An asset's extension: what follows its last dot.
/// @param name The name.
/// @return The extension, or "" without one.
[[nodiscard]] inline std::string_view assetExtension(std::string_view name) {
	const std::size_t dot = name.rfind('.');
	return dot == std::string_view::npos ? std::string_view() : name.substr(dot + 1);
}

/// @brief The type a name's extension says (any case: a file the person attached may be "Song.MP3").
/// @param name The name.
/// @return The type, or "" if its extension is none of assetTypes().
[[nodiscard]] inline std::string_view assetTypeOf(std::string_view name) {
	std::string lower(name);
	for(char& c : lower)
		if(c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	if(lower.ends_with(".sound.json")) return soundType;
	const std::string extension(assetExtension(lower));
	if(extension.empty()) return {};
	for(const AssetType& type : assetTypes())
		for(const std::string_view known : type.extensions)
			if(!known.empty() && known == extension) return type.type;
	return {};
}

/// @brief Checks an asset's name: `[a-z0-9][a-z0-9._-]{0,99}`, so it is never a path, a hidden file or "..".
/// @param name The name.
/// @return What is wrong with it, or std::nullopt.
[[nodiscard]] inline std::optional<std::string> assetNameProblem(std::string_view name) {
	const auto plain = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
	if(name.empty() || name.size() > maxAssetName || !plain(name.front()))
		return "an asset's name is 1 to 100 lowercase letters, digits, '.', '_' or '-', starting with a letter or a "
		       "digit, e.g. \"moon-sea.svg\"";
	for(const char c : name)
		if(!plain(c) && c != '.' && c != '_' && c != '-')
			return "an asset's name has only lowercase letters, digits, '.', '_' and '-' (no folders), e.g. "
			       "\"moon-sea.svg\"";
	return std::nullopt;
}

/// @brief Checks an asset's type: one of assetTypes(), which its name's extension must say.
/// @param name The name (checked by assetNameProblem()).
/// @param type The media type.
/// @return What is wrong, or std::nullopt.
[[nodiscard]] inline std::optional<std::string> assetTypeProblem(std::string_view name, std::string_view type) {
	for(const AssetType& known : assetTypes()) {
		if(known.type != type) continue;
		if(assetTypeOf(name) == type) return std::nullopt;
		return "'" + std::string(name) + "' must end with ." + std::string(known.extensions[0]) + " for " +
		       std::string(type);
	}
	return "'" + std::string(type.substr(0, 60)) +
	       "' isn't a type an asset may have (image/png, image/jpeg, image/svg+xml, audio/wav, audio/mpeg, "
	       "audio/flac, application/json, text/plain, application/vnd.affrescoui.sound+json)";
}

/// @brief Checks that bytes are what their type says, by their first bytes (magic numbers); text must be UTF-8 without
/// NUL bytes. Decoders check the rest (DocumentRules::checkAsset).
/// @param type The media type (one of assetTypes()).
/// @param bytes The bytes.
/// @return What is wrong, or std::nullopt.
[[nodiscard]] inline std::optional<std::string> assetBytesProblem(std::string_view type, std::string_view bytes) {
	const auto starts = [&](std::string_view magic, std::size_t at = 0) {
		return bytes.size() >= at + magic.size() && bytes.substr(at, magic.size()) == magic;
	};
	const auto text = [&] {
		for(std::size_t i = 0; i < bytes.size(); ++i) {
			const auto c = static_cast<unsigned char>(bytes[i]);
			if(c == 0) return false;
			if(c < 0x80) continue;
			const std::size_t more = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 9;
			if(more == 9 || c == 0xC0 || c == 0xC1 || c > 0xF4 || i + more >= bytes.size()) return false;
			for(std::size_t k = 1; k <= more; ++k)
				if((static_cast<unsigned char>(bytes[i + k]) & 0xC0) != 0x80) return false;
			i += more;
		}
		return true;
	};
	bool fits = false;
	if(type == "image/png") fits = starts("\x89PNG\r\n\x1a\n");
	else if(type == "image/jpeg") fits = starts("\xff\xd8\xff");
	else if(type == "audio/wav") fits = starts("RIFF") && starts("WAVE", 8);
	else if(type == "audio/flac") fits = starts("fLaC");
	else if(type == "audio/mpeg")
		fits = starts("ID3") || (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
		                         (static_cast<unsigned char>(bytes[1]) & 0xE0) == 0xE0);
	else if(type == "image/svg+xml" || type == "application/json" || type == "text/plain" || type == soundType)
		fits = text();
	if(fits) return std::nullopt;
	return "its bytes aren't " + std::string(type) +
	       (type.starts_with("image/") || type.starts_with("audio/") ? " (they don't start as such a file does)"
	                                                                 : " (expected UTF-8 text)");
}

/// @brief Reads an asset reference: "asset:<name>".
/// @param value Any value.
/// @return The name, or std::nullopt if it isn't a reference.
[[nodiscard]] inline std::optional<std::string> assetReference(const json& value) {
	if(!value.is_string()) return std::nullopt;
	const auto& text = value.get_ref<const std::string&>();
	if(!text.starts_with(assetScheme)) return std::nullopt;
	return text.substr(assetScheme.size());
}

/// @brief Decodes base64 (RFC 4648, with or without padding; no line breaks).
/// @param text The text.
/// @param limit The most bytes it may give: longer text is refused before it is decoded.
/// @return The bytes, or std::nullopt if it isn't base64 or would give more than @p limit bytes.
[[nodiscard]] inline std::optional<std::string> decodeBase64(std::string_view text, std::size_t limit) {
	while(!text.empty() && text.back() == '=') text.remove_suffix(1);
	if(text.size() % 4 == 1 || text.size() / 4 * 3 > limit + 2) return std::nullopt;
	const auto value = [](char c) -> int {
		if(c >= 'A' && c <= 'Z') return c - 'A';
		if(c >= 'a' && c <= 'z') return c - 'a' + 26;
		if(c >= '0' && c <= '9') return c - '0' + 52;
		if(c == '+' || c == '-') return 62;
		if(c == '/' || c == '_') return 63;
		return -1;
	};
	std::string bytes;
	bytes.reserve(text.size() / 4 * 3 + 2);
	std::uint32_t bits = 0;
	int count = 0;
	for(const char c : text) {
		const int v = value(c);
		if(v < 0) return std::nullopt;
		bits = (bits << 6) | static_cast<std::uint32_t>(v);
		if(++count == 4) {
			bytes.push_back(static_cast<char>((bits >> 16) & 0xFF));
			bytes.push_back(static_cast<char>((bits >> 8) & 0xFF));
			bytes.push_back(static_cast<char>(bits & 0xFF));
			bits = 0;
			count = 0;
		}
	}
	if(count == 2) bytes.push_back(static_cast<char>((bits >> 4) & 0xFF));
	if(count == 3) {
		bytes.push_back(static_cast<char>((bits >> 10) & 0xFF));
		bytes.push_back(static_cast<char>((bits >> 2) & 0xFF));
	}
	if(bytes.size() > limit) return std::nullopt;
	return bytes;
}

/// @brief Converts an asset to its live-protocol JSON.
/// @param asset The asset.
/// @return `{"name", "type", "size", "sha256"}`.
inline json toJson(const AssetInfo& asset) {
	return {{"name", asset.name}, {"type", asset.type}, {"size", asset.size}, {"sha256", asset.sha256}};
}

/// @brief Converts an attachment to its live-protocol JSON: what agents see of it.
/// @param attachment The attachment.
/// @return `{"id", "name", "type", "size"}` and its details (`duration`, `width`, `height`).
inline json toJson(const AttachmentInfo& attachment) {
	json result = attachment.details.is_object() ? attachment.details : json::object();
	result["id"] = attachment.id;
	result["name"] = attachment.name;
	result["type"] = attachment.type;
	result["size"] = attachment.size;
	return result;
}

} // namespace affresco
