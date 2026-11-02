/// @file
/// @brief The Lisp notation's writer (Lisp.hpp): a JSON tree as the canonical text that reads back as the same tree.
///
/// The layout depends only on the value, so writing is canonical: a list, an object or a node goes on one line when it
/// fits in 120 columns, else one member per line; a node's children always go one per line. Over several lines, the
/// members of a list or an object align one column past its opening bracket, as Lisp editors indent them, and a node's
/// properties and children are two spaces in. Whether a value fits is found by writing it on one line and stopping as
/// soon as the line is too long, so each value is tried in at most a line's worth of work, however large it is.
///
/// Comments, when a text has them (format()), are written where they were; a container holding some goes over several
/// lines, and its closing bracket on a line of its own after a comment.

#include "Lisp.hpp"

#include "affresco/notation/Words.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace affresco::notation::detail {

namespace {

/// Longest line the writer makes where the values allow it, in bytes.
constexpr std::size_t lineWidth = 120;

/// @brief Tells whether an object is written as a node: it has a `type`, a `use` or `children`, or is in a list of
/// children.
/// @param value The object.
/// @param child Whether it is in a list of children.
/// @return true if it is.
bool isNode(const json& value, bool child) {
	return child || value.contains("type") || value.contains("use") || value.contains("children");
}

/// @brief The comments of a member, if it has any.
/// @param tree The comments of its container, or nullptr.
/// @param index The member's index.
/// @return Them, or nullptr.
const MemberComments* commentsOf(const CommentTree* tree, std::size_t index) {
	if(!tree) return nullptr;
	const auto found = tree->members.find(index);
	return found == tree->members.end() ? nullptr : &found->second;
}

/// @brief The comments inside a member, if it has any.
/// @param comments The member's comments, or nullptr.
/// @return Them, or nullptr.
const CommentTree* innerOf(const MemberComments* comments) { return comments ? comments->inner.get() : nullptr; }

/// @brief Tells whether a type is written as a node's head: a plain word that isn't `node` or one the language runs.
/// @param type The `type` member.
/// @return true if it is.
bool isHeadType(const json& type) {
	if(!type.is_string()) return false;
	const std::string& text = type.get_ref<const std::string&>();
	return isPlainWord(text) && text != untypedHead && !isLanguageWord(text);
}

/// @brief Writes a value in the Lisp notation.
class Writer {
public:
	/// @brief Writes a value.
	/// @param value The value.
	/// @param comments Its comments, or nullptr.
	/// @return Its text, with a line break at the end.
	std::string document(const json& value, const DocumentComments* comments) {
		if(comments)
			for(const std::string& comment : comments->root.leading) {
				out_ += comment;
				out_ += '\n';
			}
		write(value, comments ? comments->root.inner.get() : nullptr, false);
		if(comments) {
			endMember(&comments->root);
			for(const std::string& comment : comments->after) {
				out_ += '\n';
				out_ += comment;
			}
		}
		out_ += '\n';
		return std::move(out_);
	}

private:
	/// @brief Writes a value: on one line if it fits, else over several.
	/// @param value The value.
	/// @param tree The comments inside it, or nullptr.
	/// @param child Whether it is in a list of children.
	void write(const json& value, const CommentTree* tree, bool child) {
		if(tree) { // comments end lines: never on one line
			members(value, tree, child);
			return;
		}
		if(trying_ || !value.is_structured() || value.empty()) {
			members(value, nullptr, child);
			return;
		}
		const std::size_t start = beginTrying();
		members(value, nullptr, child);
		trying_ = false;
		if(!tooLong_) return;
		tooLong_ = false;
		out_.resize(start);
		members(value, nullptr, child);
	}

	/// @brief Starts writing on one line, to see if it fits.
	/// @return Where the text stood.
	std::size_t beginTrying() {
		const std::size_t start = out_.size();
		const std::size_t used = start - (out_.rfind('\n') == std::string::npos ? 0 : out_.rfind('\n') + 1);
		trying_ = true;
		limit_ = start + (lineWidth > used ? lineWidth - used : 0);
		return start;
	}

	/// @brief The column the text has reached, in characters.
	/// @return It, from 0.
	[[nodiscard]] std::size_t column() const {
		std::size_t count = 0;
		for(std::size_t at = out_.size(); at > 0 && out_[at - 1] != '\n'; --at)
			if((static_cast<unsigned char>(out_[at - 1]) & 0xC0U) != 0x80U) ++count; // a UTF-8 continuation byte isn't one
		return count;
	}

	/// @brief Writes a value's members in the layout chosen (on one line while trying_).
	/// @param value The value.
	/// @param tree The comments inside it, or nullptr.
	/// @param child Whether it is in a list of children.
	void members(const json& value, const CommentTree* tree, bool child) {
		if(value.is_object() && isNode(value, child)) node(value, tree);
		else if(value.is_object()) object(value, tree);
		else if(value.is_array()) list(value, tree, child);
		else scalar(value);
	}

	/// @brief Starts a member: its comments, then on a line of its own or after the opening bracket.
	/// @param comments Its comments, or nullptr.
	/// @param indent The column of its line, if it starts one.
	/// @param first Whether it is the first member, which stays on the opening bracket's line.
	/// @param opening What stands between that bracket and a first member.
	void beginMember(const MemberComments* comments, std::size_t indent, bool first, std::string_view opening = {}) {
		const bool commented = comments && !comments->leading.empty();
		if(commented) {
			for(const std::string& comment : comments->leading) {
				breakLine(indent);
				out_ += comment;
			}
			breakLine(indent);
		} else if(first) {
			append(opening);
		} else {
			separate(indent);
		}
	}

	/// @brief Ends a member: the comment at the end of its line.
	/// @param comments Its comments, or nullptr.
	void endMember(const MemberComments* comments) {
		if(!comments || comments->trailing.empty()) return;
		out_ += ' ';
		out_ += comments->trailing;
		endsInComment_ = true;
	}

	/// @brief Writes a container's closing bracket, after the comments at its end.
	/// @param closer The bracket.
	/// @param tree The container's comments, or nullptr.
	/// @param indent The column of its members.
	void close(std::string_view closer, const CommentTree* tree, std::size_t indent) {
		if(tree)
			for(const std::string& comment : tree->closing) {
				breakLine(indent);
				out_ += comment;
				endsInComment_ = true;
			}
		if(endsInComment_) breakLine(indent);
		append(closer);
	}

	/// @brief Writes a list: `[a b c]`.
	/// @param value The array.
	/// @param tree The comments inside it, or nullptr.
	/// @param child Whether its items are a list of children.
	void list(const json& value, const CommentTree* tree, bool child) {
		const std::size_t indent = column() + 1;
		append("[");
		std::size_t index = 0;
		for(const json& item : value) {
			if(tooLong_) return;
			const MemberComments* comments = commentsOf(tree, index);
			beginMember(comments, indent, index == 0);
			write(item, innerOf(comments), child);
			endMember(comments);
			++index;
		}
		close("]", tree, indent);
	}

	/// @brief Writes an object: `{:key value ...}`.
	/// @param value The object.
	/// @param tree The comments inside it, or nullptr.
	void object(const json& value, const CommentTree* tree) {
		const std::size_t indent = column() + 1;
		append("{");
		std::size_t index = 0;
		for(const auto& [key, member] : value.items()) {
			if(tooLong_) return;
			const MemberComments* comments = commentsOf(tree, index);
			beginMember(comments, indent, index == 0);
			if(isPlainWord(key)) append(":");
			name(key);
			append(" ");
			write(member, innerOf(comments), false);
			endMember(comments);
			++index;
		}
		close("}", tree, indent);
	}

	/// @brief Writes a node: `(type#id :key value ... child ...)`. The head is the `type` when it is the first key and
	/// a word isHeadType() takes, else `node`; `#id` follows it when the `id` is the key after that and a plain word;
	/// every other key keeps its place; the `children`, when they are the last key and not empty, follow one per line.
	/// @param value The object.
	/// @param tree The comments inside it, or nullptr.
	void node(const json& value, const CommentTree* tree) {
		if(value.empty()) {
			append("(");
			append(untypedHead);
			close(")", tree, column() + 1);
			return;
		}
		const auto first = value.begin();
		const auto last = std::prev(value.end());
		const bool typed = first.key() == "type" && isHeadType(*first);
		const bool hasChildren = last.key() == "children" && last->is_array() && !last->empty();
		if(trying_ && hasChildren) {
			tooLong_ = true; // children always go one per line
			return;
		}
		const std::size_t indent = column() + 2;
		append("(");
		append(typed ? first->get_ref<const std::string&>() : std::string(untypedHead));
		auto from = typed ? std::next(first) : first;
		std::size_t index = typed ? 1 : 0;
		const auto properties = hasChildren ? last : value.end();
		const bool hoisted = from != properties && from.key() == "id" && isHeadId(*from, commentsOf(tree, index));
		if(hoisted) {
			append("#");
			append(from->get_ref<const std::string&>());
		}
		bool headComment = typed && endHead(commentsOf(tree, 0)); // after the id: a comment ends the line
		if(hoisted) {
			headComment = endHead(commentsOf(tree, index)) || headComment;
			++from;
			++index;
		}
		nodeProperties(from, properties, index, tree, indent, headComment);
		if(hasChildren) {
			const CommentTree* children = innerOf(commentsOf(tree, static_cast<std::size_t>(std::distance(first, last))));
			std::size_t child = 0;
			for(const json& item : *last) {
				if(tooLong_) return;
				const MemberComments* comments = commentsOf(children, child);
				beginMember(comments, indent, false);
				write(item, innerOf(comments), true);
				endMember(comments);
				++child;
			}
		}
		close(")", tree, indent);
	}

	/// @brief Tells whether an `id` is written `#id` after the head: a plain word with no comment before it.
	/// @param id The member.
	/// @param comments Its comments, or nullptr.
	/// @return true if it is.
	[[nodiscard]] static bool isHeadId(const json& id, const MemberComments* comments) {
		return id.is_string() && isPlainWord(id.get_ref<const std::string&>()) &&
		       (!comments || (comments->leading.empty() && !comments->inner));
	}

	/// @brief Writes the comment that ends the head's line, if any.
	/// @param comments The comments of the head's type or id, or nullptr.
	/// @return true if there is one.
	bool endHead(const MemberComments* comments) {
		endMember(comments);
		return comments && !comments->trailing.empty();
	}

	/// @brief Writes a node's properties after its head: on the head's line if they fit, else the first one there and
	/// each other on a line of its own. With a comment among them or on the head's line, all go over lines.
	/// @param from The first property.
	/// @param to Past the last one.
	/// @param index The first property's index in the node.
	/// @param tree The comments inside the node, or nullptr.
	/// @param indent The column of the lines they start.
	/// @param headComment Whether a comment ends the head's line.
	void nodeProperties(const json::const_iterator& from, const json::const_iterator& to, std::size_t index,
	                    const CommentTree* tree, std::size_t indent, bool headComment) {
		if(from == to) return;
		bool commented = headComment;
		std::size_t at = index;
		for(auto item = from; item != to; ++item) commented = commented || commentsOf(tree, at++) != nullptr;
		if(!commented) {
			const bool outer = !trying_;
			const std::size_t start = outer ? beginTrying() : 0;
			for(auto item = from; item != to && !tooLong_; ++item) {
				append(" ");
				nodeProperty(item.key(), item.value(), nullptr);
			}
			if(!outer) return;
			trying_ = false;
			if(!tooLong_) return;
			tooLong_ = false;
			out_.resize(start);
		}
		at = index;
		for(auto item = from; item != to; ++item, ++at) {
			const MemberComments* comments = commentsOf(tree, at);
			beginMember(comments, indent, item == from && !headComment, " ");
			nodeProperty(item.key(), item.value(), innerOf(comments));
			endMember(comments);
		}
	}

	/// @brief Writes one of a node's properties, `:key value`.
	/// @param key The key.
	/// @param member Its value.
	/// @param tree The comments inside it, or nullptr.
	void nodeProperty(const std::string& key, const json& member, const CommentTree* tree) {
		append(":");
		name(key);
		append(" ");
		write(member, tree, key == "children" && member.is_array());
	}

	/// @brief Writes a value that holds no others: a string, a number, true, false or nil. A number that isn't finite
	/// (a value computed at run time, never one read) is nil, as JSON writes it null: dump()'s "null" would read back
	/// as the string "null".
	/// @param value The value.
	void scalar(const json& value) {
		if(value.is_string()) stringValue(value.get_ref<const std::string&>());
		else if(value.is_null() || (value.is_number_float() && !std::isfinite(value.get<double>()))) append("nil");
		else append(value.dump());
	}

	/// @brief Writes a string value: bare if it is a plain word that doesn't start with a capital, which display text
	/// does, so that text keeps its quotes when it gains a space; else in quotes with JSON's escapes.
	/// @param text The string.
	void stringValue(const std::string& text) {
		if(trying_ && out_.size() + text.size() > limit_) tooLong_ = true; // quotes would only make it longer
		else if(isPlainWord(text) && !std::isupper(static_cast<unsigned char>(text.front()))) append(text);
		else append(json(text).dump());
	}

	/// @brief Writes a name, a key: bare if it is a plain word, else in quotes with JSON's escapes.
	/// @param text The name.
	void name(const std::string& text) {
		if(trying_ && out_.size() + text.size() > limit_) tooLong_ = true; // quotes would only make it longer
		else if(isPlainWord(text)) append(text);
		else append(json(text).dump());
	}

	/// @brief Starts the next member of a list, an object or a node: a space on one line, a line of its own else.
	/// @param indent The member's column.
	void separate(std::size_t indent) {
		if(trying_) append(" ");
		else breakLine(indent);
	}

	/// @brief Starts a line.
	/// @param indent Its column.
	void breakLine(std::size_t indent) {
		out_ += '\n';
		out_.append(indent, ' ');
		endsInComment_ = false;
	}

	/// @brief Adds text, and notes when a value tried on one line no longer fits.
	/// @param text The text.
	void append(std::string_view text) {
		if(tooLong_) return;
		out_ += text;
		if(trying_ && out_.size() > limit_) tooLong_ = true;
	}

	std::string out_;            ///< The text so far.
	bool trying_ = false;        ///< Writing on one line, to see if it fits.
	bool tooLong_ = false;       ///< What is being tried on one line went past limit_.
	bool endsInComment_ = false; ///< The text ends in a comment, so what follows starts a line.
	std::size_t limit_ = 0;      ///< While trying_: the length of out_ the line may reach.
};

} // namespace

std::string writeLisp(const json& value, const DocumentComments* comments) { return Writer().document(value, comments); }

} // namespace affresco::notation::detail
