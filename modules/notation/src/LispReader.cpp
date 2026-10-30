/// @file
/// @brief The Lisp notation's reader (Lisp.hpp): text into the JSON tree it spells, nothing evaluated.
///
/// One pass over the text, a token at a time. The reader descends one call per container, and refuses a container
/// before it would nest deeper than maxNesting, so the stack it uses is bounded whatever the text.

#include "Lisp.hpp"

#include "Words.hpp"

#include "affresco/notation/Notation.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affresco::notation::detail {

namespace {

/// @brief What a token is.
enum class Kind {
	OpenNode,  ///< `(`
	CloseNode, ///< `)`
	OpenList,  ///< `[`
	CloseList, ///< `]`
	OpenMap,   ///< `{`
	CloseMap,  ///< `}`
	String,    ///< `"text"`, with JSON's escapes
	Key,       ///< `:name` or `:"name"`
	Id,        ///< `#name` or `#"name"`
	Word,      ///< anything else between delimiters: a bare word, a number, true, false or nil
	End        ///< the end of the text
};

/// @brief A token: what it is, its text, and where it starts.
struct Token {
	Kind kind = Kind::End;  ///< What it is.
	std::string_view text;  ///< Its text, as written (with its ':' or '#').
	std::size_t offset = 0; ///< Where it starts in the text, in bytes.
};

/// @brief Tells whether a character ends a bare word: a space, a bracket, a quote or a comment.
/// @param c The character.
/// @return true if it does.
constexpr bool isDelimiter(char c) noexcept { return std::string_view(" \t\r\n()[]{}\";").contains(c); }

/// @brief Moves past digits.
/// @param text The text.
/// @param at Where they may start.
/// @return Past the last of them (@p at if there are none).
std::size_t pastDigits(std::string_view text, std::size_t at) noexcept {
	while(at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
	return at;
}

/// @brief Moves past an optional part of a number: one of its marks, an optional sign, then digits (".5", "e+10").
/// @param text The text.
/// @param at Where the part may start.
/// @param marks The characters that start it: "." or "eE".
/// @param sign Whether a sign may follow the mark.
/// @return Past the part, @p at if there is none, or std::string_view::npos if its mark has no digits after it.
std::size_t pastPart(std::string_view text, std::size_t at, std::string_view marks, bool sign) noexcept {
	if(at >= text.size() || !marks.contains(text[at])) return at;
	++at;
	if(sign && at < text.size() && (text[at] == '+' || text[at] == '-')) ++at;
	const std::size_t end = pastDigits(text, at);
	return end == at ? std::string_view::npos : end;
}

/// @brief Tells whether a text is a JSON number: `-?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?`.
/// @param text The text.
/// @return true if it is.
bool isJsonNumber(std::string_view text) noexcept {
	const std::size_t start = text.starts_with('-') ? 1 : 0;
	const std::size_t integer = pastDigits(text, start);
	if(integer == start || (text[start] == '0' && integer > start + 1)) return false; // no digit, or a leading zero
	const std::size_t fraction = pastPart(text, integer, ".", false);
	return fraction != std::string_view::npos && pastPart(text, fraction, "eE", true) == text.size();
}

/// @brief An object being read: its members in the order written, a key at most once.
///
/// An ordered_json object finds a key by searching its members one by one; past a few, the keys seen are also kept in
/// a hash set, so reading an object of many keys takes time in proportion to its size (as parseUntrusted() does).
class ObjectBuilder {
public:
	/// @brief Adds a member at the end.
	/// @param key Its key.
	/// @param value Its value.
	/// @return false if the object has that key already (nothing is added).
	bool add(std::string key, json value) {
		auto& members = static_cast<Members&>(object_.get_ref<json::object_t&>());
		if(seen_.empty() && members.size() >= searchedMembers)
			for(const auto& [name, unused] : members) seen_.insert(name);
		const bool known = seen_.empty() ? std::ranges::find(members, key, &Members::value_type::first) != members.end()
		                                 : !seen_.insert(key).second;
		if(known) return false;
		members.emplace_back(std::move(key), std::move(value)); // the vector's own: no search
		return true;
	}
	/// @brief The object read. @return It (moved out).
	json take() noexcept { return std::move(object_); }

private:
	using Members = std::vector<std::pair<const std::string, json>>; ///< What an ordered_json object stores.
	/// Keys found by a search one by one; past this many, through the set.
	static constexpr std::size_t searchedMembers = 16;

	json object_ = json::object();         ///< The object.
	std::unordered_set<std::string> seen_; ///< Its keys, once it has more than searchedMembers.
};

/// @brief Reads one value from a text in the Lisp notation.
class Reader {
public:
	/// @brief Reads @p text.
	/// @param text The text; it must outlive the reader.
	explicit Reader(std::string_view text) noexcept : text_(text) {}

	/// @brief Reads the text's one value.
	/// @return It.
	/// @throws NotationError If the text isn't one value.
	json document() {
		if(text_.size() > maxLispBytes)
			fail(0, "the text is longer than " + std::to_string(maxLispBytes >> 20) + " MiB");
		if(text_.starts_with("\xEF\xBB\xBF")) at_ = 3; // a byte order mark, as some editors write
		json result = value(next(), 0);
		if(const Token after = next(); after.kind != Kind::End)
			fail(after.offset, "expected the end of the text after the value, found " + shown(after));
		return result;
	}

private:
	/// @brief Reads the next token, past spaces and comments.
	/// @return It.
	/// @throws NotationError If a string or a quoted name never ends.
	Token next() {
		skipSpace();
		if(at_ >= text_.size()) return {Kind::End, {}, at_};
		const std::size_t start = at_;
		const char c = text_[at_];
		static constexpr std::string_view brackets = "()[]{}";
		if(const std::size_t bracket = brackets.find(c); bracket != std::string_view::npos) {
			++at_;
			return {static_cast<Kind>(bracket), text_.substr(start, 1), start};
		}
		if(c == '"') return {Kind::String, quoted(start), start};
		if(c == ':' || c == '#') {
			++at_;
			if(at_ < text_.size() && text_[at_] == '"') quoted(at_);
			else wordAt(at_);
			return {c == ':' ? Kind::Key : Kind::Id, text_.substr(start, at_ - start), start};
		}
		wordAt(start);
		return {Kind::Word, text_.substr(start, at_ - start), start};
	}

	/// @brief Moves past spaces, line breaks and comments (`;` to the end of the line).
	void skipSpace() noexcept {
		while(at_ < text_.size()) {
			const char c = text_[at_];
			if(c == ';') {
				const std::size_t end = text_.find('\n', at_);
				at_ = end == std::string_view::npos ? text_.size() : end;
			} else if(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
				++at_;
			} else {
				return;
			}
		}
	}

	/// @brief Moves past a string in quotes, its escapes included.
	/// @param start Where its opening quote is.
	/// @return Its text, quotes included.
	/// @throws NotationError If it never ends.
	std::string_view quoted(std::size_t start) {
		std::size_t end = start + 1;
		while(end < text_.size() && text_[end] != '"') end += text_[end] == '\\' ? 2 : 1;
		if(end >= text_.size()) fail(start, "a string that never ends: expected '\"'");
		at_ = end + 1;
		if(at_ < text_.size() && !isDelimiter(text_[at_]))
			fail(at_, "expected a space or a bracket after the string's closing quote");
		return text_.substr(start, at_ - start);
	}

	/// @brief Moves past a bare word: everything up to a delimiter.
	/// @param start Where it starts.
	void wordAt(std::size_t start) noexcept {
		at_ = start;
		while(at_ < text_.size() && !isDelimiter(text_[at_])) ++at_;
	}

	/// @brief Reads a value that starts with a token.
	/// @param first The token.
	/// @param depth How many objects and lists hold the value.
	/// @return The value.
	/// @throws NotationError If it isn't a value.
	json value(const Token& first, int depth) {
		switch(first.kind) {
		case Kind::OpenNode:
			return node(first, depth);
		case Kind::OpenList:
			return list(first, depth);
		case Kind::OpenMap:
			return map(first, depth);
		case Kind::String:
			return unquote(first.text, first.offset);
		case Kind::Word:
			return word(first);
		case Kind::Key:
			fail(first.offset, "a key (" + shown(first) + ") outside an object or a node");
		case Kind::Id:
			fail(first.offset, "an id (" + shown(first) + ") outside a node");
		case Kind::End:
			fail(first.offset, "expected a value, found the end of the text");
		default:
			fail(first.offset, "expected a value, found " + shown(first));
		}
	}

	/// @brief Refuses a container that would nest deeper than maxNesting.
	/// @param open Its opening token.
	/// @param levels How many levels it would reach.
	/// @throws NotationError If it would.
	void shallowEnough(const Token& open, int levels) const {
		if(levels > maxNesting) fail(open.offset, "nested more than " + std::to_string(maxNesting) + " levels deep");
	}

	/// @brief Reads a node, `(type #id :key value ... child ...)`, after its '('.
	/// @param open The '('.
	/// @param depth How many objects and lists hold it.
	/// @return The object: `type` first (unless the head is `node`), then the ids and keys in the order written, then
	/// `children` if it has any.
	/// @throws NotationError If it isn't a node.
	json node(const Token& open, int depth) {
		shallowEnough(open, depth + 1);
		const Token head = next();
		if(head.kind != Kind::Word || !isPlainWord(head.text))
			fail(head.offset, "expected the node's type, or node, after '(', found " + shown(head));
		ObjectBuilder object;
		if(head.text != untypedHead) object.add("type", std::string(head.text));
		json children = json::array();
		for(Token item = next(); item.kind != Kind::CloseNode; item = next()) {
			if(item.kind == Kind::End) fail(item.offset, "expected ')' to close the node opened at " + placeOf(open));
			if(item.kind == Kind::Key || item.kind == Kind::Id) {
				if(!children.empty())
					fail(item.offset, shown(item) + " after the node's children: properties come first");
				const bool id = item.kind == Kind::Id;
				std::string key = id ? "id" : name(item); // the key's own problem first, then its value's
				json member = id ? json(name(item)) : value(next(), depth + 1);
				add(object, item, std::move(member), std::move(key));
			} else {
				shallowEnough(item, depth + 2);
				children.push_back(value(item, depth + 2));
			}
		}
		if(!children.empty() && !object.add("children", std::move(children)))
			fail(open.offset, "a node with :children and children after its properties");
		return object.take();
	}

	/// @brief Reads a list, `[value ...]`, after its '['.
	/// @param open The '['.
	/// @param depth How many objects and lists hold it.
	/// @return The array.
	/// @throws NotationError If it isn't a list.
	json list(const Token& open, int depth) {
		shallowEnough(open, depth + 1);
		json items = json::array();
		for(Token item = next(); item.kind != Kind::CloseList; item = next()) {
			if(item.kind == Kind::End) fail(item.offset, "expected ']' to close the list opened at " + placeOf(open));
			items.push_back(value(item, depth + 1));
		}
		return items;
	}

	/// @brief Reads an object, `{:key value ...}`, after its '{'.
	/// @param open The '{'.
	/// @param depth How many objects and lists hold it.
	/// @return The object, its keys in the order written.
	/// @throws NotationError If it isn't an object.
	json map(const Token& open, int depth) {
		shallowEnough(open, depth + 1);
		ObjectBuilder object;
		for(Token item = next(); item.kind != Kind::CloseMap; item = next()) {
			if(item.kind == Kind::End) fail(item.offset, "expected '}' to close the object opened at " + placeOf(open));
			if(item.kind != Kind::Key) fail(item.offset, "expected a key (:name) or '}', found " + shown(item));
			std::string key = name(item); // the key's own problem first, then its value's
			json member = value(next(), depth + 1);
			add(object, item, std::move(member), std::move(key));
		}
		return object.take();
	}

	/// @brief Adds a member to an object being read.
	/// @param object The object.
	/// @param item The member's key or id token, for the error.
	/// @param member Its value.
	/// @param key Its key.
	/// @throws NotationError If the object has that key already.
	void add(ObjectBuilder& object, const Token& item, json member, std::string key) const {
		const std::string written = excerpt(key); // a key can be as long as the text: the error shows its start
		if(!object.add(std::move(key), std::move(member)))
			fail(item.offset, "the key \"" + written + "\" twice: a key has one value");
	}

	/// @brief The name a key or an id token gives.
	/// @param token The token: `:name`, `:"name"`, `#name` or `#"name"`.
	/// @return The name.
	/// @throws NotationError If the name is neither a plain word nor a string in quotes.
	[[nodiscard]] std::string name(const Token& token) const {
		const std::string_view written = token.text.substr(1);
		if(written.starts_with('"')) return unquote(written, token.offset + 1);
		if(!isPlainWord(written))
			fail(token.offset, "expected a word or a string in quotes after '" + std::string(token.text.substr(0, 1)) +
			                       "', found " + shown(token));
		return std::string(written);
	}

	/// @brief Reads a string in quotes, with JSON's escapes.
	/// @param written Its text, quotes included.
	/// @param offset Where it starts, for the error.
	/// @return The string.
	/// @throws NotationError If it isn't a JSON string (an unknown escape, a control character, bytes that aren't
	/// UTF-8).
	[[nodiscard]] std::string unquote(std::string_view written, std::size_t offset) const {
		try {
			return json::parse(written).get<std::string>();
		} catch(const json::exception&) {
			fail(offset, "a string in quotes that JSON can't read: an unknown escape, a control character or bytes "
			             "that aren't UTF-8");
		}
	}

	/// @brief Reads a bare word: true, false, nil, a number or a string.
	/// @param token The word.
	/// @return Its value.
	/// @throws NotationError If it is none of these.
	[[nodiscard]] json word(const Token& token) const {
		const std::string_view text = token.text;
		if(text == "true" || text == "false") return text == "true";
		if(text == "nil") return nullptr;
		if(isPlainWord(text)) return std::string(text);
		if(!isJsonNumber(text))
			fail(token.offset, "unexpected " + shown(token) +
			                       ": not a word, a number, true, false or nil (a string "
			                       "with other characters goes in quotes)");
		if(text.size() > maxNumberLength)
			fail(token.offset, "a number longer than " + std::to_string(maxNumberLength) + " characters");
		try {
			return parseUntrusted(text);
		} catch(const DocumentError&) { // a number no double can hold, such as 1e400
			fail(token.offset, "a number too large: " + shown(token));
		}
	}

	/// @brief The start of a text an error quotes: its first 40 bytes, cut before a character rather than inside one,
	/// and "..." if there is more.
	/// @param text The text.
	/// @return Its start.
	[[nodiscard]] static std::string excerpt(std::string_view text) {
		constexpr std::size_t most = 40;
		if(text.size() <= most) return std::string(text);
		std::size_t cut = most;
		while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) --cut; // a UTF-8 continuation byte
		return std::string(text.substr(0, cut)) + "...";
	}

	/// @brief A token as an error shows it: its excerpt(), in quotes.
	/// @param token The token.
	/// @return The text.
	[[nodiscard]] static std::string shown(const Token& token) {
		return token.kind == Kind::End ? "the end of the text" : "'" + excerpt(token.text) + "'";
	}

	/// @brief The line and column of a place in the text.
	/// @param offset The place, in bytes.
	/// @return Its line and its column (in characters), from 1.
	[[nodiscard]] std::pair<std::size_t, std::size_t> lineAndColumn(std::size_t offset) const {
		offset = std::min(offset, text_.size());
		const std::string_view before = text_.substr(0, offset);
		const std::size_t line = static_cast<std::size_t>(std::ranges::count(before, '\n')) + 1;
		const std::size_t start = before.rfind('\n') == std::string_view::npos ? 0 : before.rfind('\n') + 1;
		std::size_t column = 1;
		for(const char c : before.substr(start))
			if((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++column; // a UTF-8 continuation byte isn't one
		return {line, column};
	}

	/// @brief A token's place, as an error names it.
	/// @param token The token.
	/// @return "line 3, column 8".
	[[nodiscard]] std::string placeOf(const Token& token) const {
		const auto [line, column] = lineAndColumn(token.offset);
		return "line " + std::to_string(line) + ", column " + std::to_string(column);
	}

	/// @brief Reports a problem at a place in the text.
	/// @param offset The place, in bytes.
	/// @param message What is wrong.
	/// @throws NotationError Always.
	[[noreturn]] void fail(std::size_t offset, const std::string& message) const {
		const auto [line, column] = lineAndColumn(offset);
		throw NotationError("", line, column, message);
	}

	std::string_view text_; ///< See the constructor.
	std::size_t at_ = 0;    ///< Where the next token is looked for.
};

} // namespace

json readLisp(std::string_view text) { return Reader(text).document(); }

} // namespace affresco::notation::detail
