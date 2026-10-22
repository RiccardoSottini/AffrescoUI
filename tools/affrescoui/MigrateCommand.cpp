/// @file
/// @brief `affrescoui migrate`: rewrites version 1 files as version 2 (SCREEN_FORMAT.md, "Migrating from version 1"),
/// changing only the keys and values the migration changes, so that ids, key order and the layout stay as written.

#include "Cli.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/format/Migrate.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace affresco::cli {

namespace {

/// The flags migrate takes.
constexpr std::array<std::string_view, 2> migrateFlags = {"--check", "--write"};

/// @brief Where a value is in the text, and, for an object member, its key and the member before it.
struct Span {
	std::size_t keyStart = std::string::npos; ///< A member's '"' that opens its key; npos for the root or an item.
	std::size_t keyEnd = 0;                   ///< Just after its key's closing '"'.
	std::size_t start = 0;                    ///< Its value's first character.
	std::size_t end = 0;                      ///< Just after its value.
	std::size_t previousEnd = std::string::npos; ///< Just after the value of the member before it; npos if it's first.
};

/// @brief Finds where every value of a JSON text is, by its JSON Pointer.
class SpanReader {
public:
	/// @brief Reads a text the library has already parsed, so it is JSON.
	/// @param text The text.
	explicit SpanReader(std::string_view text) : text_(text) {
		skip();
		value("", Span{});
	}

	/// @brief The spans found.
	/// @return Them, by pointer.
	[[nodiscard]] const std::unordered_map<std::string, Span>& spans() const noexcept { return spans_; }

private:
	/// @brief Moves past spaces and line breaks.
	void skip() {
		while(at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_]))) ++at_;
	}

	/// @brief Moves past a string, its '"' first.
	void string() {
		for(++at_; at_ < text_.size() && text_[at_] != '"'; ++at_)
			if(text_[at_] == '\\') ++at_;
		++at_;
	}

	/// @brief Reads a value and records its span.
	/// @param pointer Its pointer.
	/// @param span Its key, if it is a member.
	void value(const std::string& pointer, Span span) {
		span.start = at_;
		const char first = at_ < text_.size() ? text_[at_] : '\0';
		if(first == '{' || first == '[') {
			++at_;
			skip();
			std::size_t previousEnd = std::string::npos;
			for(std::size_t n = 0; at_ < text_.size() && text_[at_] != '}' && text_[at_] != ']'; ++n) {
				Span member;
				std::string at = pointer + "/";
				if(first == '{') {
					member.keyStart = at_;
					string();
					member.keyEnd = at_;
					for(const char c : parseUntrusted(text_.substr(member.keyStart, member.keyEnd - member.keyStart))
					                       .get<std::string>())
						at += c == '~' ? std::string("~0") : c == '/' ? std::string("~1") : std::string(1, c);
					skip();
					++at_; // ':'
					skip();
				} else {
					at += std::to_string(n);
				}
				member.previousEnd = previousEnd;
				value(at, member);
				previousEnd = spans_[at].end;
				skip();
				if(at_ < text_.size() && text_[at_] == ',') ++at_;
				skip();
			}
			++at_;
		} else if(first == '"') {
			string();
		} else {
			while(at_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[at_])) && text_[at_] != ',' &&
			      text_[at_] != '}' && text_[at_] != ']')
				++at_;
		}
		span.end = at_;
		spans_[pointer] = span;
	}

	std::string_view text_;                       ///< The text.
	std::size_t at_ = 0;                          ///< Where reading is.
	std::unordered_map<std::string, Span> spans_; ///< What was found.
};

/// @brief One change to the text: the characters from `start` to `end` become `text`.
struct Edit {
	std::size_t start; ///< The first character replaced.
	std::size_t end;   ///< Just after the last.
	std::string text;  ///< What takes their place.
};

/// @brief Writes a migration's rewrites into the text it was read from.
/// @param text The text.
/// @param rewrites What the migration changed, with pointers into the document as the text has it.
/// @return The new text.
/// @throws std::runtime_error If a rewrite's place isn't in the text.
std::string spliced(const std::string& text, const std::vector<format::Rewrite>& rewrites) {
	const SpanReader reader(text);
	std::vector<Edit> edits;
	for(const format::Rewrite& rewrite : rewrites) {
		const auto found = reader.spans().find(rewrite.pointer);
		if(found == reader.spans().end()) throw std::runtime_error("nothing at " + rewrite.pointer);
		const Span& span = found->second;
		switch(rewrite.kind) {
		case format::Rewrite::Kind::Rename: edits.push_back({span.keyStart, span.keyEnd, json(rewrite.key).dump()}); break;
		case format::Rewrite::Kind::Replace: edits.push_back({span.start, span.end, rewrite.value.dump()}); break;
		case format::Rewrite::Kind::Remove: {
			// With the comma after it and the space up to the next member, or else with the comma before it
			std::size_t next = span.end;
			while(next < text.size() && std::isspace(static_cast<unsigned char>(text[next]))) ++next;
			if(next < text.size() && text[next] == ',') {
				for(++next; next < text.size() && std::isspace(static_cast<unsigned char>(text[next]));) ++next;
				edits.push_back({span.keyStart, next, ""});
			} else {
				edits.push_back({span.previousEnd == std::string::npos ? span.keyStart : span.previousEnd, span.end, ""});
			}
			break;
		}
		}
	}
	std::ranges::sort(edits, std::greater<>{}, &Edit::start); // from the end, so the offsets before stay right
	std::string result = text;
	std::size_t limit = std::string::npos;
	for(const Edit& edit : edits) {
		if(edit.end > limit) throw std::runtime_error("two rewrites overlap");
		result.replace(edit.start, edit.end - edit.start, edit.text);
		limit = edit.start;
	}
	return result;
}

/// @brief What migrating one file came to.
enum class Outcome {
	Current,  ///< Nothing to change: version 2, or not an AffrescoUI document.
	Migrated, ///< Changed (or, without --write, would be).
	Left      ///< Version 1 but left as it is: signed, or not JSON.
};

/// @brief Migrates one file.
/// @param file The file.
/// @param check Only say whether it would change.
/// @param write Write the new text.
/// @return What it came to.
Outcome migrateFile(const std::filesystem::path& file, bool check, bool write) {
	const std::string text = readFile(file);
	json document;
	try {
		document = parseUntrusted(text);
	} catch(const json::exception&) {
		return Outcome::Current; // not JSON: nothing this command reads
	}
	json migrated = document;
	const std::vector<format::Rewrite> rewrites = format::migrate(migrated);
	if(rewrites.empty()) return Outcome::Current;
	if(document.contains("signature")) {
		std::cout << file.string() << ": signed, left as version 1 (readers migrate it after checking the signature; "
		                              "sign it again after migrating)\n";
		return Outcome::Left;
	}

	const std::string after = spliced(text, rewrites);
	if(parseUntrusted(after) != migrated) throw std::runtime_error(file.string() + ": can't be rewritten in place");
	if(check) {
		std::cout << file.string() << ": version 1, " << rewrites.size() << " rewrites (run migrate --write)\n";
		return Outcome::Migrated;
	}
	for(const format::Rewrite& rewrite : rewrites) std::cout << file.string() << ": " << rewrite.message << '\n';
	if(write) writeFile(file, after);
	return Outcome::Migrated;
}

} // namespace

// Each rewrite is spliced into the text where the value it changes is: the rest of the file stays as written
Exit migrateFiles(std::span<const std::string_view> args) {
	const Arguments arguments(args, {}, migrateFlags);
	if(arguments.positional().empty()) throw UsageError("expected one or more files");
	const bool check = arguments.flag("--check"), write = arguments.flag("--write");
	if(check && write) throw UsageError("--check and --write go separately");

	std::size_t migrated = 0;
	for(const std::string& file : arguments.positional())
		if(migrateFile(file, check, write) == Outcome::Migrated) ++migrated;
	if(!check && !write && migrated > 0)
		std::cout << migrated << " files to migrate: nothing written (run migrate --write)\n";
	return check && migrated > 0 ? Exit::Problems : Exit::Success;
}

} // namespace affresco::cli
