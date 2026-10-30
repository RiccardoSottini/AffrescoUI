/// @file
/// @brief The Lisp notation's writer (Lisp.hpp): a JSON tree as the canonical text that reads back as the same tree.
///
/// The layout depends only on the value, so writing is canonical: a list, an object or a node goes on one line when it
/// fits in 120 columns, else one member per line; a node's children always go one per line. Whether a value fits is
/// found by writing it on one line and stopping as soon as the line is too long, so each value is tried in at most a
/// line's worth of work, however large it is.

#include "Lisp.hpp"

#include "Words.hpp"

#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace affresco::notation::detail {

namespace {

/// Longest line the writer makes where the values allow it, in bytes.
constexpr std::size_t lineWidth = 120;

/// @brief Tells whether an object is written as a node: it has an `id` that is a string.
/// @param value The object.
/// @return true if it is.
bool isNode(const json& value) {
	const auto id = value.find("id");
	return id != value.end() && id->is_string();
}

/// @brief Writes a value in the Lisp notation.
class Writer {
public:
	/// @brief Writes a value.
	/// @param value The value.
	/// @return Its text, with a line break at the end.
	std::string text(const json& value) {
		write(value, 0);
		out_ += '\n';
		return std::move(out_);
	}

private:
	/// @brief Writes a value: on one line if it fits, else over several.
	/// @param value The value.
	/// @param level The indentation of the line it starts on, in steps of two spaces.
	void write(const json& value, std::size_t level) {
		if(trying_ || !value.is_structured() || value.empty()) {
			members(value, level);
			return;
		}
		const std::size_t start = out_.size();
		const std::size_t used = start - (out_.rfind('\n') == std::string::npos ? 0 : out_.rfind('\n') + 1);
		trying_ = true;
		limit_ = start + (lineWidth > used ? lineWidth - used : 0);
		members(value, level);
		trying_ = false;
		if(!tooLong_) return;
		tooLong_ = false;
		out_.resize(start);
		members(value, level);
	}

	/// @brief Writes a value's members in the layout chosen (on one line while trying_).
	/// @param value The value.
	/// @param level The indentation of the line it starts on.
	void members(const json& value, std::size_t level) {
		if(value.is_object() && isNode(value)) node(value, level);
		else if(value.is_object()) object(value, level);
		else if(value.is_array()) list(value, level);
		else scalar(value);
	}

	/// @brief Writes a list: `[a b c]`.
	/// @param value The array.
	/// @param level The indentation of the line it starts on.
	void list(const json& value, std::size_t level) {
		append("[");
		bool first = true;
		for(const json& item : value) {
			if(tooLong_) return;
			if(!first) separate(level + 1);
			first = false;
			write(item, level + 1);
		}
		append("]");
	}

	/// @brief Writes an object: `{:key value ...}`.
	/// @param value The object.
	/// @param level The indentation of the line it starts on.
	void object(const json& value, std::size_t level) {
		append("{");
		bool first = true;
		for(const auto& [key, member] : value.items()) {
			if(tooLong_) return;
			if(!first) separate(level + 1);
			first = false;
			property(key, member, level + 1);
		}
		append("}");
	}

	/// @brief Writes a node: `(type #id :key value ... child ...)`. The head is the `type` when it is the first key
	/// and a plain word other than `node`, else `node`; every other key keeps its place, `id` written `#id`; the
	/// `children`, when they are the last key and not empty, follow one per line.
	/// @param value The object.
	/// @param level The indentation of the line it starts on.
	void node(const json& value, std::size_t level) {
		const auto first = value.begin();
		const bool typed = first.key() == "type" && first->is_string() &&
		                   isPlainWord(first->get_ref<const std::string&>()) &&
		                   first->get_ref<const std::string&>() != untypedHead;
		const auto last = std::prev(value.end());
		const bool hasChildren = last.key() == "children" && last->is_array() && !last->empty();
		if(trying_ && hasChildren) {
			tooLong_ = true; // children always go one per line
			return;
		}
		append("(");
		append(typed ? first->get_ref<const std::string&>() : std::string(untypedHead));
		const auto properties = hasChildren ? last : value.end();
		nodeProperties(typed ? std::next(first) : first, properties, level);
		if(hasChildren)
			for(const json& child : *last) {
				if(tooLong_) return;
				separate(level + 1);
				write(child, level + 1);
			}
		append(")");
	}

	/// @brief Writes a node's properties after its head: on the head's line if they fit, else the first one there and
	/// each other on a line of its own.
	/// @param from The first property.
	/// @param to Past the last one.
	/// @param level The indentation of the node's line.
	void nodeProperties(const json::const_iterator& from, const json::const_iterator& to, std::size_t level) {
		if(from == to) return;
		const bool outer = !trying_;
		const std::size_t start = out_.size();
		if(outer) {
			const std::size_t used = start - (out_.rfind('\n') == std::string::npos ? 0 : out_.rfind('\n') + 1);
			trying_ = true;
			limit_ = start + (lineWidth > used ? lineWidth - used : 0);
		}
		for(auto at = from; at != to && !tooLong_; ++at) {
			append(" ");
			nodeProperty(at.key(), at.value(), level);
		}
		if(!outer) return;
		trying_ = false;
		if(!tooLong_) return;
		tooLong_ = false;
		out_.resize(start);
		for(auto at = from; at != to; ++at) {
			if(at == from) append(" ");
			else separate(level + 1);
			nodeProperty(at.key(), at.value(), level);
		}
	}

	/// @brief Writes one of a node's properties: its id as `#id`, any other key as `:key value`.
	/// @param key The key.
	/// @param member Its value.
	/// @param level The indentation of the node's line.
	void nodeProperty(const std::string& key, const json& member, std::size_t level) {
		if(key == "id" && member.is_string()) {
			append("#");
			atom(member.get_ref<const std::string&>());
		} else {
			property(key, member, level + 1);
		}
	}

	/// @brief Writes `:key value`.
	/// @param key The key.
	/// @param member Its value.
	/// @param level The indentation of the line it is on.
	void property(const std::string& key, const json& member, std::size_t level) {
		append(":");
		atom(key);
		append(" ");
		write(member, level);
	}

	/// @brief Writes a value that holds no others: a string, a number, true, false or nil.
	/// @param value The value.
	void scalar(const json& value) {
		if(value.is_string()) atom(value.get_ref<const std::string&>());
		else if(value.is_null()) append("nil");
		else append(value.dump());
	}

	/// @brief Writes a string: bare if it is a plain word, else in quotes with JSON's escapes.
	/// @param text The string.
	void atom(const std::string& text) {
		if(trying_ && out_.size() + text.size() > limit_) tooLong_ = true; // quotes would only make it longer
		else if(isPlainWord(text)) append(text);
		else append(json(text).dump());
	}

	/// @brief Starts the next member of a list, an object or a node: a space on one line, a line of its own else.
	/// @param level The member's indentation.
	void separate(std::size_t level) {
		if(trying_) {
			append(" ");
			return;
		}
		out_ += '\n';
		out_.append(level * 2, ' ');
	}

	/// @brief Adds text, and notes when a value tried on one line no longer fits.
	/// @param text The text.
	void append(std::string_view text) {
		if(tooLong_) return;
		out_ += text;
		if(trying_ && out_.size() > limit_) tooLong_ = true;
	}

	std::string out_;       ///< The text so far.
	bool trying_ = false;   ///< Writing on one line, to see if it fits.
	bool tooLong_ = false;  ///< What is being tried on one line went past limit_.
	std::size_t limit_ = 0; ///< While trying_: the length of out_ the line may reach.
};

} // namespace

std::string writeLisp(const json& value) { return Writer().text(value); }

} // namespace affresco::notation::detail
