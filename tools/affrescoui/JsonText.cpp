/// @file
/// @brief Reading and writing JSON as written (JsonText.hpp).

#include "JsonText.hpp"

#include <affresco/core/Json.hpp>

#include <stdexcept>

namespace affresco::cli {

namespace {

/// The widest a line gets before a value is written on several lines, in columns.
constexpr std::size_t lineWidth = 120;

/// The columns a tab counts as, as in the C++ code (.clang-format).
constexpr std::size_t tabWidth = 4;

/// @brief Reads JSON text, keeping how it was written.
class Reader {
public:
	/// @brief A reader of some text.
	/// @param text The text; must outlive the reader.
	explicit Reader(std::string_view text) : text_(text) {}

	/// @brief Reads the one value the text holds.
	/// @return The value.
	/// @throws std::runtime_error If the text isn't one JSON value.
	TextValue document() {
		skipSpace();
		TextValue value = read(0);
		skipSpace();
		if(at_ < text_.size()) fail("expected the end of the file");
		return value;
	}

private:
	/// @brief Stops reading with an error that says where.
	/// @param problem What is wrong.
	[[noreturn]] void fail(const std::string& problem) const {
		std::size_t line = 1, column = 1;
		for(std::size_t i = 0; i < at_ && i < text_.size(); ++i) {
			const bool lineBreak = text_[i] == '\n';
			column = lineBreak ? 1 : column + 1;
			if(lineBreak) ++line;
		}
		throw std::runtime_error("line " + std::to_string(line) + ", column " + std::to_string(column) + ": " +
		                         problem);
	}

	/// @brief Skips spaces, tabs and line breaks.
	/// @return How many line breaks it skipped.
	int skipSpace() {
		int lines = 0;
		while(at_ < text_.size() &&
		      (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\n' || text_[at_] == '\r')) {
			lines += text_[at_] == '\n' ? 1 : 0;
			++at_;
		}
		return lines;
	}

	/// @brief A scalar.
	/// @param text Its text as written.
	/// @return The value.
	static TextValue scalar(std::string text) {
		TextValue value;
		value.text = std::move(text);
		return value;
	}

	/// @brief The character being read, or '\0' at the end.
	/// @return It.
	[[nodiscard]] char peek() const { return at_ < text_.size() ? text_[at_] : '\0'; }

	/// @brief Reads a value.
	/// @param depth How many objects and arrays it is inside.
	/// @return The value.
	TextValue read(int depth) {
		if(depth > maxNesting) fail("nested more than " + std::to_string(maxNesting) + " levels deep");
		const char c = peek();
		if(c == '{' || c == '[') return container(depth);
		if(c == '"') return scalar(string());
		const std::size_t start = at_;
		// A number, true, false or null: the JSON library already checked which
		while(at_ < text_.size() && std::string_view("+-.0123456789eEtruefalsn").contains(text_[at_])) ++at_;
		if(at_ == start) fail(c == '\0' ? "unexpected end of the file" : std::string("unexpected '") + c + "'");
		return scalar(std::string(text_.substr(start, at_ - start)));
	}

	/// @brief Reads a string literal.
	/// @return Its text, quotes and escapes as written.
	std::string string() {
		const std::size_t start = at_++;
		while(at_ < text_.size() && text_[at_] != '"') {
			if(text_[at_] == '\\') ++at_;
			if(text_[at_] == '\n') fail("a line break inside a string");
			++at_;
		}
		if(at_ >= text_.size()) fail("a string without its closing '\"'");
		++at_;
		return std::string(text_.substr(start, at_ - start));
	}

	/// @brief Reads an object or an array.
	/// @param depth How many objects and arrays it is inside.
	/// @return The value.
	TextValue container(int depth) {
		const bool object = peek() == '{';
		const char close = object ? '}' : ']';
		TextValue value{object ? TextValue::Kind::Object : TextValue::Kind::Array, {}, {}, false, at_, {}};
		const std::size_t inside = ++at_;
		value.expanded = skipSpace() > 0 && object;
		value.lead = text_.substr(inside, at_ - inside);
		if(peek() == close) {
			++at_;
			return value;
		}
		for(bool first = true;; first = false) {
			const int lines = first ? 0 : skipSpace();
			TextValue::Member member{{}, {}, lines > 1};
			if(object) {
				if(peek() != '"') fail("expected a key in quotes");
				member.key = string();
				skipSpace();
				if(peek() != ':') fail("expected ':'");
				++at_;
				skipSpace();
			}
			member.value = read(depth + 1);
			value.members.push_back(std::move(member));
			skipSpace();
			if(peek() == close) break;
			if(peek() != ',') fail(std::string("expected ',' or '") + close + "'");
			++at_;
		}
		++at_;
		return value;
	}

	std::string_view text_; ///< What is read.
	std::size_t at_ = 0;    ///< Where.
};

/// @brief Writes values, deciding for each object and array whether it fits on one line.
class Printer {
public:
	/// @brief Writes a value that starts a line, after any text already on it.
	/// @param value The value.
	/// @param indent How many tabs its lines start with.
	/// @param column The column it starts at.
	/// @param after What follows it on its last line (a ',' or nothing), so that it fits too.
	void write(const TextValue& value, std::size_t indent, std::size_t column, std::size_t after) {
		if(const std::string flat = inlineText(value); value.kind == TextValue::Kind::Scalar || value.members.empty() ||
		                                               (!flat.empty() && column + flat.size() + after <= lineWidth)) {
			out_ += flat;
			return;
		}
		writeExpanded(value, indent);
	}

	/// @brief The text written.
	/// @return It.
	[[nodiscard]] std::string& text() { return out_; }

private:
	/// @brief Writes an object or an array over several lines, one member per line.
	/// @param value The value.
	/// @param indent How many tabs its first line starts with.
	void writeExpanded(const TextValue& value, std::size_t indent) {
		const bool object = value.kind == TextValue::Kind::Object;
		out_ += object ? "{\n" : "[\n";
		for(std::size_t i = 0; i < value.members.size(); ++i) {
			const TextValue::Member& member = value.members[i];
			if(member.blankBefore && i > 0) out_ += '\n';
			out_ += std::string(indent + 1, '\t');
			std::size_t at = (indent + 1) * tabWidth;
			if(object) {
				out_ += member.key + ": ";
				at += member.key.size() + 2;
			}
			const bool last = i + 1 == value.members.size();
			write(member.value, indent + 1, at, last ? 0 : 1);
			out_ += last ? "\n" : ",\n";
		}
		out_ += std::string(indent, '\t') + (object ? "}" : "]");
	}

	/// @brief A value on one line: `{ "a": 1, "b": [2, 3] }`.
	/// @param value The value.
	/// @return The text, or "" if it can't be on one line (it holds an expanded object or an empty line).
	static std::string inlineText(const TextValue& value) {
		if(value.kind == TextValue::Kind::Scalar) return value.text;
		if(value.members.empty()) return value.kind == TextValue::Kind::Object ? "{}" : "[]";
		if(value.expanded) return "";
		const bool object = value.kind == TextValue::Kind::Object;
		std::string text = object ? "{ " : "[";
		for(std::size_t i = 0; i < value.members.size(); ++i) {
			const TextValue::Member& member = value.members[i];
			if(member.blankBefore && i > 0) return "";
			const std::string item = inlineText(member.value);
			if(item.empty()) return "";
			text += (i ? ", " : "") + (object ? member.key + ": " : std::string()) + item;
		}
		return text + (object ? " }" : "]");
	}

	std::string out_; ///< The text so far.
};

/// @brief Reads a key as written.
/// @param key The key, quotes included.
/// @return Its text.
std::string unquoted(const std::string& key) { return json::parse(key).get<std::string>(); }

} // namespace

TextValue parseText(std::string_view text) {
	// The JSON library says first whether it is JSON at all, with its own limits (nesting): the reader then only has
	// to keep how it is written
	try {
		static_cast<void>(parseUntrusted(text));
	} catch(const json::exception& error) {
		throw std::runtime_error(error.what());
	} catch(const DocumentError& error) {
		throw std::runtime_error(error.what());
	}
	return Reader(text).document();
}

std::string printText(const TextValue& value) {
	Printer printer;
	printer.write(value, 0, 0, 0);
	return std::move(printer.text()) + "\n";
}

TextValue* memberOf(TextValue& object, std::string_view key) {
	if(object.kind != TextValue::Kind::Object) return nullptr;
	for(TextValue::Member& member : object.members)
		if(unquoted(member.key) == key) return &member.value;
	return nullptr;
}

TextValue* valueAt(TextValue& root, std::string_view pointer) {
	std::vector<std::string> keys; // the pointer's keys, unescaped by the JSON library
	for(json::json_pointer path{std::string(pointer)}; !path.empty(); path.pop_back())
		keys.insert(keys.begin(), path.back());

	TextValue* value = &root;
	for(const std::string& key : keys) {
		if(value->kind == TextValue::Kind::Object) {
			value = memberOf(*value, key);
		} else if(value->kind == TextValue::Kind::Array && !key.empty() &&
		          key.find_first_not_of("0123456789") == std::string::npos && key.size() < 10) {
			const std::size_t index = std::stoul(key);
			value = index < value->members.size() ? &value->members[index].value : nullptr;
		} else {
			value = nullptr;
		}
		if(!value) return nullptr;
	}
	return value;
}

} // namespace affresco::cli
