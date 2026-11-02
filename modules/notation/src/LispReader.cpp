/// @file
/// @brief The Lisp notation's reader (Lisp.hpp): text into the JSON tree it spells, nothing evaluated.
///
/// One pass over the text, a token at a time. The reader descends one call per container, and refuses a container
/// before it would nest deeper than maxNesting, so the stack it uses is bounded whatever the text.

#include "Lisp.hpp"

#include "affresco/notation/Lexer.hpp"
#include "affresco/notation/Notation.hpp"
#include "affresco/notation/Words.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affresco::notation::detail {

namespace {

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
	/// @brief How many members it has so far. @return The count.
	[[nodiscard]] std::size_t size() const noexcept { return object_.size(); }
	/// @brief The object read. @return It (moved out).
	json take() noexcept { return std::move(object_); }

private:
	using Members = std::vector<std::pair<const std::string, json>>; ///< What an ordered_json object stores.
	/// Keys found by a search one by one; past this many, through the set.
	static constexpr std::size_t searchedMembers = 16;

	json object_ = json::object();         ///< The object.
	std::unordered_set<std::string> seen_; ///< Its keys, once it has more than searchedMembers.
};

/// A member's place among the comments: which container's, and its index there.
struct MemberPlace {
	CommentTree* tree = nullptr; ///< The container; nullptr if there is no such member.
	std::size_t index = 0;       ///< Its index.
};

/// @brief Reads one value from a text in the Lisp notation.
class Reader {
public:
	/// @brief Reads @p text.
	/// @param text The text; it must outlive the reader.
	/// @param comments Where to keep its comments, or nullptr to skip them.
	explicit Reader(std::string_view text, DocumentComments* comments = nullptr) : lexer_(text), comments_(comments) {
		if(comments_) lexer_.keepComments();
	}

	/// @brief Reads the text's one value.
	/// @return It.
	/// @throws NotationError If the text isn't one value.
	json document() {
		const Token first = lexer_.next();
		if(comments_)
			for(const SourceComment& comment : lexer_.takeComments()) comments_->root.leading.emplace_back(comment.text);
		json result = value(first, 0, comments_ ? &comments_->root.inner : nullptr);
		if(const Token after = lexer_.next(); after.kind != TokenKind::End)
			lexer_.fail(after.offset, "expected the end of the text after the value, found " + Lexer::shown(after));
		if(comments_)
			for(const SourceComment& comment : lexer_.takeComments()) {
				if(comment.sameLine) comments_->root.trailing = comment.text;
				else comments_->after.emplace_back(comment.text);
			}
		return result;
	}

private:
	using Inner = std::unique_ptr<CommentTree>; ///< The comments inside a value, if it has any.

	/// @brief Gives the comments read so far to their places: one that ends the previous member's line is that
	/// member's, the rest stand before the next member, or at the end of the container.
	/// @param previous The member before them, if any.
	/// @param next The member after them; without a tree, they are at the end of @p closing.
	/// @param closing The container whose end they are at, when there is no member after them.
	void attach(MemberPlace previous, MemberPlace next, CommentTree* closing = nullptr) {
		if(!comments_) return;
		for(const SourceComment& comment : lexer_.takeComments()) {
			if(comment.sameLine && previous.tree) previous.tree->members[previous.index].trailing = comment.text;
			else if(next.tree) next.tree->members[next.index].leading.emplace_back(comment.text);
			else if(closing) closing->closing.emplace_back(comment.text);
		}
	}

	/// @brief Keeps a container's comments for its parent, if it has any.
	/// @param tree The container's comments.
	/// @param inner Where its parent keeps them; may be nullptr.
	static void keep(CommentTree&& tree, Inner* inner) {
		if(inner && !tree.empty()) *inner = std::make_unique<CommentTree>(std::move(tree));
	}

	/// @brief Reads a value that starts with a token.
	/// @param first The token.
	/// @param depth How many objects and lists hold the value.
	/// @param inner Where the comments inside it go; may be nullptr.
	/// @return The value.
	/// @throws NotationError If it isn't a value.
	json value(const Token& first, int depth, Inner* inner) {
		switch(first.kind) {
		case TokenKind::OpenNode:
			return node(first, depth, inner);
		case TokenKind::OpenList:
			return list(first, depth, inner);
		case TokenKind::OpenMap:
			return map(first, depth, inner);
		case TokenKind::String:
			return lexer_.unquote(first.text, first.offset);
		case TokenKind::Word:
			return word(first);
		case TokenKind::Key:
			lexer_.fail(first.offset, "a key (" + Lexer::shown(first) + ") outside an object or a node");
		case TokenKind::Id:
			lexer_.fail(first.offset, "an id (" + Lexer::shown(first) + ") outside a node");
		case TokenKind::End:
			lexer_.fail(first.offset, "expected a value, found the end of the text");
		default:
			lexer_.fail(first.offset, "expected a value, found " + Lexer::shown(first));
		}
	}

	/// @brief Refuses a container that would nest deeper than maxNesting.
	/// @param open Its opening token.
	/// @param levels How many levels it would reach.
	/// @throws NotationError If it would.
	void shallowEnough(const Token& open, int levels) const {
		if(levels > maxNesting)
			lexer_.fail(open.offset, "nested more than " + std::to_string(maxNesting) + " levels deep");
	}

	/// @brief Reads a node's head, `type`, `node`, `type#id` or `node#id`, after its '('.
	/// @param object The node, which gets its type and its id.
	void head(ObjectBuilder& object) {
		const Token token = lexer_.next();
		const std::size_t hash = token.kind == TokenKind::Word ? token.text.find('#') : std::string_view::npos;
		const std::string_view type = token.text.substr(0, hash);
		const bool hasId = hash != std::string_view::npos;
		if(token.kind != TokenKind::Word || !isPlainWord(type) || (hasId && !isPlainWord(token.text.substr(hash + 1))))
			lexer_.fail(token.offset, "expected the node's type, or node, after '(', found " + Lexer::shown(token));
		if(type != untypedHead) object.add("type", std::string(type));
		if(hasId) object.add("id", std::string(token.text.substr(hash + 1)));
	}

	/// @brief Reads a node, `(type#id :key value ... child ...)`, after its '('.
	/// @param open The '('.
	/// @param depth How many objects and lists hold it.
	/// @param inner Where the comments inside it go; may be nullptr.
	/// @return The object: `type` first (unless the head is `node`), then the ids and keys in the order written, then
	/// `children` if it has any.
	/// @throws NotationError If it isn't a node.
	json node(const Token& open, int depth, Inner* inner) {
		shallowEnough(open, depth + 1);
		ObjectBuilder object;
		head(object);
		CommentTree tree;
		CommentTree childrenTree;
		json children = json::array();
		// The member before the comments read last
		const auto previous = [&]() -> MemberPlace {
			if(!children.empty()) return {&childrenTree, children.size() - 1};
			return object.size() > 0 ? MemberPlace{&tree, object.size() - 1} : MemberPlace{};
		};
		for(Token item = lexer_.next(); item.kind != TokenKind::CloseNode; item = lexer_.next()) {
			if(item.kind == TokenKind::End)
				lexer_.fail(item.offset, "expected ')' to close the node opened at " + lexer_.placeOf(open.offset));
			if(item.kind == TokenKind::Key || item.kind == TokenKind::Id) {
				if(!children.empty())
					lexer_.fail(item.offset, Lexer::shown(item) + " after the node's children: properties come first");
				attach(previous(), {&tree, object.size()});
				property(object, tree, item, depth);
			} else {
				attach(previous(), {&childrenTree, children.size()});
				shallowEnough(item, depth + 2);
				Inner child;
				children.push_back(value(item, depth + 2, comments_ ? &child : nullptr));
				if(child) childrenTree.members[children.size() - 1].inner = std::move(child);
			}
		}
		attach(previous(), {}, &tree);
		if(!childrenTree.empty())
			tree.members[object.size()].inner = std::make_unique<CommentTree>(std::move(childrenTree));
		if(!children.empty() && !object.add("children", std::move(children)))
			lexer_.fail(open.offset, "a node with :children and children after its properties");
		keep(std::move(tree), inner);
		return object.take();
	}

	/// @brief Reads one of a node's properties: `#id`, or `:key value`.
	/// @param object The node.
	/// @param tree The comments of its properties.
	/// @param item The id or the key.
	/// @param depth How many objects and lists hold the node.
	void property(ObjectBuilder& object, CommentTree& tree, const Token& item, int depth) {
		const bool id = item.kind == TokenKind::Id;
		const std::size_t index = object.size();
		std::string key = id ? "id" : lexer_.name(item); // the key's own problem first, then its value's
		Inner inner;
		json member = id ? json(lexer_.name(item)) : value(lexer_.next(), depth + 1, comments_ ? &inner : nullptr);
		if(inner) tree.members[index].inner = std::move(inner);
		add(object, item, std::move(member), std::move(key));
	}

	/// @brief Reads a list, `[value ...]`, after its '['.
	/// @param open The '['.
	/// @param depth How many objects and lists hold it.
	/// @param inner Where the comments inside it go; may be nullptr.
	/// @return The array.
	/// @throws NotationError If it isn't a list.
	json list(const Token& open, int depth, Inner* inner) {
		shallowEnough(open, depth + 1);
		json items = json::array();
		CommentTree tree;
		const auto previous = [&]() -> MemberPlace {
			return items.empty() ? MemberPlace{} : MemberPlace{&tree, items.size() - 1};
		};
		for(Token item = lexer_.next(); item.kind != TokenKind::CloseList; item = lexer_.next()) {
			if(item.kind == TokenKind::End)
				lexer_.fail(item.offset, "expected ']' to close the list opened at " + lexer_.placeOf(open.offset));
			attach(previous(), {&tree, items.size()});
			Inner member;
			items.push_back(value(item, depth + 1, comments_ ? &member : nullptr));
			if(member) tree.members[items.size() - 1].inner = std::move(member);
		}
		attach(previous(), {}, &tree);
		keep(std::move(tree), inner);
		return items;
	}

	/// @brief Reads an object, `{:key value ...}` (a key can also be a string in quotes), after its '{'.
	/// @param open The '{'.
	/// @param depth How many objects and lists hold it.
	/// @param inner Where the comments inside it go; may be nullptr.
	/// @return The object, its keys in the order written.
	/// @throws NotationError If it isn't an object.
	json map(const Token& open, int depth, Inner* inner) {
		shallowEnough(open, depth + 1);
		ObjectBuilder object;
		CommentTree tree;
		const auto previous = [&]() -> MemberPlace {
			return object.size() == 0 ? MemberPlace{} : MemberPlace{&tree, object.size() - 1};
		};
		for(Token item = lexer_.next(); item.kind != TokenKind::CloseMap; item = lexer_.next()) {
			if(item.kind == TokenKind::End)
				lexer_.fail(item.offset, "expected '}' to close the object opened at " + lexer_.placeOf(open.offset));
			if(item.kind != TokenKind::Key && item.kind != TokenKind::String)
				lexer_.fail(item.offset, "expected a key (:name) or '}', found " + Lexer::shown(item));
			attach(previous(), {&tree, object.size()});
			const std::size_t index = object.size();
			// the key's own problem first, then its value's
			std::string key = item.kind == TokenKind::Key ? lexer_.name(item) : lexer_.unquote(item.text, item.offset);
			Inner member;
			json content = value(lexer_.next(), depth + 1, comments_ ? &member : nullptr);
			if(member) tree.members[index].inner = std::move(member);
			add(object, item, std::move(content), std::move(key));
		}
		attach(previous(), {}, &tree);
		keep(std::move(tree), inner);
		return object.take();
	}

	/// @brief Adds a member to an object being read.
	/// @param object The object.
	/// @param item The member's key or id token, for the error.
	/// @param member Its value.
	/// @param key Its key.
	/// @throws NotationError If the object has that key already.
	void add(ObjectBuilder& object, const Token& item, json member, std::string key) const {
		const std::string written = Lexer::excerpt(key); // a key can be as long as the text: the error shows its start
		if(!object.add(std::move(key), std::move(member)))
			lexer_.fail(item.offset, "the key \"" + written + "\" twice: a key has one value");
	}

	/// @brief Reads a bare word: true, false, nil, a number or a string.
	/// @param token The word.
	/// @return Its value.
	/// @throws NotationError If it is none of these.
	[[nodiscard]] json word(const Token& token) const {
		if(isPlainWord(token.text)) return std::string(token.text);
		if(std::optional<json> constant = lexer_.constant(token)) return *std::move(constant);
		lexer_.fail(token.offset, "unexpected " + Lexer::shown(token) +
		                              ": not a word, a number, true, false or nil (a string "
		                              "with other characters goes in quotes)");
	}

	Lexer lexer_;                ///< The tokens.
	DocumentComments* comments_; ///< See the constructor.
};

} // namespace

json readLisp(std::string_view text) { return Reader(text).document(); }

json readLisp(std::string_view text, DocumentComments& comments) { return Reader(text, &comments).document(); }

} // namespace affresco::notation::detail
