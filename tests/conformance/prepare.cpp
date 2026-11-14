/// @file
/// @brief The conformance run's token pass (prepare.hpp): tokens, an outline of the forms, the table of what is out of
/// scope, and the edits that write `(%out "<reason>")` and each assertion's line into the text.
#include "prepare.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace affresco::conformance {

namespace {

/// @brief How deep forms may nest: the language's reader stops at the same depth.
constexpr std::size_t maxDepth = 256;

/// @brief What a token is.
enum class TokenKind {
	Open,      ///< `(`, `[`, `{`, `#{`, `#(`, `#?(`, `#?@(`.
	Close,     ///< `)`, `]`, `}`.
	String,    ///< `"..."`.
	Regex,     ///< `#"..."`.
	Character, ///< `\a`, `\newline`.
	Atom,      ///< A word, a number, a keyword, `##Inf`.
	Prefix     ///< `'`, `` ` ``, `~`, `~@`, `@`, `^`, `#'`, `#_`, `#:ns`, `#tag`: applies to the form after it.
};

/// @brief A token of the text.
struct Token {
	TokenKind kind;        ///< What it is.
	std::size_t begin;     ///< Its first byte.
	std::size_t end;       ///< One past its last byte.
	std::size_t line;      ///< Its line, from 1.
	std::string_view text; ///< Its text: a view of the file's, which outlives it.
};

/// @brief A form of the outline: a token, and for an opening token or a prefix the forms it holds.
struct Node {
	std::size_t token = 0;      ///< Its first token.
	std::size_t last = 0;       ///< Its last token: the closing one for a collection.
	std::vector<Node> children; ///< What it holds: the items of a collection, the form after a prefix.
};

/// @brief Whether a byte ends a word.
/// @param c The byte.
/// @return true for white space, a comma, a bracket, a quote or a semicolon.
bool delimits(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == ',' || c == '(' || c == ')' ||
	       c == '[' || c == ']' || c == '{' || c == '}' || c == '"' || c == ';';
}

/// @brief Splits text into tokens. Strings, regular expressions and comments are read whole, so nothing inside them
/// is ever looked at again.
class Tokenizer {
public:
	/// @brief Starts on a text. @param text The text; outlives it.
	explicit Tokenizer(std::string_view text) : text_(text) {}

	/// @brief Every token. @return Them, in order.
	std::vector<Token> tokens() {
		std::vector<Token> result;
		while(skipSpace()) result.push_back(next());
		return result;
	}

private:
	/// @brief Skips white space and comments, counting lines. @return Whether a token follows.
	bool skipSpace() {
		while(at_ < text_.size()) {
			const char c = text_[at_];
			if(c == '\n') ++line_;
			if(c == ';') {
				while(at_ < text_.size() && text_[at_] != '\n') ++at_;
				continue;
			}
			if(!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == ',')) return true;
			++at_;
		}
		return false;
	}

	/// @brief The token here. @return It.
	Token next() {
		const std::size_t begin = at_;
		const std::size_t line = line_;
		const TokenKind kind = scan();
		return {kind, begin, at_, line, text_.substr(begin, at_ - begin)};
	}

	/// @brief Reads one token, moving past it. @return What it is.
	TokenKind scan() {
		const char c = text_[at_];
		if(c == '(' || c == '[' || c == '{') return ++at_, TokenKind::Open;
		if(c == ')' || c == ']' || c == '}') return ++at_, TokenKind::Close;
		if(c == '"') return quoted(), TokenKind::String;
		if(c == '\\') return character(), TokenKind::Character;
		if(c == '#') return dispatch();
		if(c == '\'' || c == '`' || c == '@' || c == '^') return ++at_, TokenKind::Prefix;
		if(c == '~') {
			at_ += at_ + 1 < text_.size() && text_[at_ + 1] == '@' ? 2 : 1;
			return TokenKind::Prefix;
		}
		word();
		return TokenKind::Atom;
	}

	/// @brief Reads what follows a `#`. @return What it is.
	TokenKind dispatch() {
		const std::string_view rest = text_.substr(at_);
		for(const std::string_view open : {"#?@(", "#?(", "#{", "#("})
			if(rest.starts_with(open)) return at_ += open.size(), TokenKind::Open;
		if(rest.starts_with("#\"")) return ++at_, quoted(), TokenKind::Regex;
		if(rest.starts_with("#'") || rest.starts_with("#_")) return at_ += 2, TokenKind::Prefix;
		if(rest.starts_with("##")) return word(), TokenKind::Atom;
		word(); // #:ns before a map, or a tag (#inst, #cpp) before a form
		return TokenKind::Prefix;
	}

	/// @brief Reads a string from its opening quote, escapes included.
	void quoted() {
		++at_;
		while(at_ < text_.size() && text_[at_] != '"') {
			if(text_[at_] == '\\') ++at_;
			if(at_ < text_.size() && text_[at_] == '\n') ++line_;
			++at_;
		}
		at_ = std::min(at_ + 1, text_.size());
	}

	/// @brief Reads a character: the backslash, one character (whole, in UTF-8), and the letters of a named one.
	void character() {
		++at_;
		if(at_ < text_.size()) ++at_;
		while(at_ < text_.size() && (static_cast<unsigned char>(text_[at_]) & 0xC0U) == 0x80U) ++at_;
		while(at_ < text_.size() && !delimits(text_[at_])) ++at_;
	}

	/// @brief Reads a word up to the next delimiter.
	void word() {
		++at_;
		while(at_ < text_.size() && !delimits(text_[at_])) ++at_;
	}

	std::string_view text_; ///< The text.
	std::size_t at_ = 0;    ///< Where it reads.
	std::size_t line_ = 1;  ///< The line there.
};

/// @brief Builds the outline of a text's forms from its tokens.
class Outliner {
public:
	/// @brief Starts on tokens. @param tokens The tokens; outlive it.
	explicit Outliner(const std::vector<Token>& tokens) : tokens_(tokens) {}

	/// @brief The top-level forms. @return Them, or std::nullopt with error() set.
	std::optional<std::vector<Node>> forms() {
		std::vector<Node> result;
		while(at_ < tokens_.size()) {
			if(tokens_[at_].kind == TokenKind::Close) return fail("a closing bracket with nothing open", tokens_[at_]);
			auto node = form(0);
			if(!node) return std::nullopt;
			result.push_back(std::move(*node));
		}
		return result;
	}

	/// @brief Why the outline failed. @return It.
	[[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
	/// @brief One form from the token here.
	/// @param depth How deep it is.
	/// @return It, or std::nullopt with error() set.
	std::optional<Node> form(std::size_t depth) {
		if(depth > maxDepth) return fail("forms nest too deep", tokens_[at_]);
		Node node{at_, at_, {}};
		const Token& token = tokens_[at_++];
		if(token.kind == TokenKind::Prefix) {
			if(at_ >= tokens_.size() || tokens_[at_].kind == TokenKind::Close) return node;
			auto child = form(depth + 1);
			if(!child) return std::nullopt;
			node.last = child->last;
			node.children.push_back(std::move(*child));
			return node;
		}
		if(token.kind != TokenKind::Open) return node;
		while(at_ < tokens_.size() && tokens_[at_].kind != TokenKind::Close) {
			auto child = form(depth + 1);
			if(!child) return std::nullopt;
			node.children.push_back(std::move(*child));
		}
		if(at_ >= tokens_.size()) return fail("a form that isn't closed", token);
		node.last = at_++;
		return node;
	}

	/// @brief Records an error. @param message What is wrong. @param token Where. @return std::nullopt.
	std::nullopt_t fail(const std::string& message, const Token& token) {
		error_ = "line " + std::to_string(token.line) + ": " + message;
		return std::nullopt;
	}

	const std::vector<Token>& tokens_; ///< The tokens.
	std::size_t at_ = 0;               ///< The token it reads.
	std::string error_;                ///< See error().
};

/// @brief The calls the language has no value for, by the word they start with (a list's first word, its namespace
/// dropped when it is `clojure.core`), and the reason an assertion holding one is out of scope. One row per word.
const std::unordered_map<std::string_view, std::string_view>& calls() {
	static const std::unordered_map<std::string_view, std::string_view> table = {
	    {"bigint", "numbers"},       // a big integer: numbers are JSON's
	    {"bigdec", "numbers"},       // a big decimal
	    {"rationalize", "numbers"},  // a ratio
	    {"char", "characters"},      // a character as its own kind: a character reads as a string here
	    {"char?", "characters"},     // whether a value is a character
	    {"var", "vars"},             // there are no vars
	    {"resolve", "vars"},         // a symbol's var
	    {"intern", "vars"},          // a var made in a namespace
	    {"atom", "mutation"},        // nothing changes a value
	    {"swap!", "mutation"},       // an atom changed
	    {"swap-vals!", "mutation"},  // an atom changed
	    {"reset!", "mutation"},      // an atom set
	    {"reset-vals!", "mutation"}, // an atom set
	    {"volatile!", "mutation"},   // a volatile
	    {"vswap!", "mutation"},      // a volatile changed
	    {"vreset!", "mutation"},     // a volatile set
	    {"transient", "mutation"},   // a collection changed in place (conj!, persistent! need one)
	    {"lazy-seq", "laziness"},    // nothing is lazy
	    {"iterate", "laziness"},     // an infinite sequence
	    {"cycle", "laziness"},       // an infinite sequence
	    {"repeatedly", "laziness"},  // an infinite, or impure, sequence
	    {"realized?", "laziness"},   // whether a lazy value was worked out
	    {"lazy-seq?", "laziness"},   // the portability namespace's: whether a value is a lazy sequence
	    {"instance?", "host"},       // a host class
	    {"new", "host"},             // a host object
	    {"memfn", "host"},           // a host method as a function
	    {"future", "host"},          // a thread
	    {"future-call", "host"},     // a thread
	    {"promise", "host"},         // a value for threads
	    {"deliver", "host"},         // a value for threads
	    {"agent", "host"},           // a thread's state
	    {"send", "host"},            // a thread's state
	    {"send-off", "host"},        // a thread's state
	    {"pmap", "host"},            // threads
	    {"pcalls", "host"},          // threads
	    {"pvalues", "host"},         // threads
	    {"locking", "host"},         // a host lock
	    {"aclone", "host"},          // a host array
	    {"aget", "host"},            // a host array
	    {"aset", "host"},            // a host array
	    {"alength", "host"},         // a host array
	    {"amap", "host"},            // a host array
	    {"areduce", "host"},         // a host array
	    {"make-array", "host"},      // a host array
	    {"into-array", "host"},      // a host array
	    {"to-array", "host"},        // a host array
	    {"to-array-2d", "host"},     // a host array
	    {"object-array", "host"},    // a host array
	    {"int-array", "host"},       // a host array
	    {"long-array", "host"},      // a host array
	    {"short-array", "host"},     // a host array
	    {"byte-array", "host"},      // a host array
	    {"char-array", "host"},      // a host array
	    {"float-array", "host"},     // a host array
	    {"double-array", "host"},    // a host array
	    {"boolean-array", "host"},   // a host array
	    {"meta", "metadata"},        // there is no metadata to read back
	    {"with-meta", "metadata"},   // metadata given
	    {"vary-meta", "metadata"},   // metadata changed
	    {"alter-meta!", "metadata"}, // metadata changed
	    {"reset-meta!", "metadata"}, // metadata set
	    {"in-ns", "namespaces"},     // one flat space of names per program
	};
	return table;
}

/// @brief Whether a word is a whole number of digits.
/// @param text The word.
/// @return true if it is one or more ASCII digits.
bool digits(std::string_view text) {
	return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// @brief Whether digits fit a 64-bit signed integer, with their sign.
/// @param text The digits.
/// @param negative Whether a minus sign was before them.
/// @return true if they do.
bool fitsInteger(std::string_view text, bool negative) {
	std::uint64_t value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if(error != std::errc() || end != text.data() + text.size()) return false;
	const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
	return value <= limit + (negative ? 1U : 0U);
}

/// @brief Whether a number, read as JSON's, has another value than Clojure gives it: a ratio that isn't whole, an
/// integer past 64 bits, a big decimal with more digits than a double keeps, a leading 0 (octal in Clojure).
/// @param word The word.
/// @return true if so.
bool numberChangesValue(std::string_view word) {
	const bool negative = word.starts_with('-');
	if(word.starts_with('-') || word.starts_with('+')) word.remove_prefix(1);
	if(word.empty() || word[0] < '0' || word[0] > '9') return false;
	if(const auto slash = word.find('/'); slash != std::string_view::npos) {
		const std::string_view top = word.substr(0, slash);
		const std::string_view bottom = word.substr(slash + 1);
		if(!digits(top) || !digits(bottom) || !fitsInteger(top, false) || !fitsInteger(bottom, false)) return true;
		std::uint64_t numerator = 0;
		std::uint64_t denominator = 0;
		std::from_chars(top.data(), top.data() + top.size(), numerator);
		std::from_chars(bottom.data(), bottom.data() + bottom.size(), denominator);
		return denominator != 0 && numerator % denominator != 0;
	}
	if(word.ends_with('N')) word.remove_suffix(1);
	else if(word.ends_with('M')) {
		word.remove_suffix(1);
		const auto significant = std::ranges::count_if(word, [](char c) { return c >= '0' && c <= '9'; });
		return significant > 15;
	}
	if(!digits(word)) return false; // a double, hexadecimal, a radix: read as the reader reads it
	if(word.size() > 1 && word[0] == '0') return true;
	return !fitsInteger(word, negative);
}

/// @brief Whether a word is a host's name: `Long/MAX_VALUE`, `js/Number`, `cpp/jank`, a method `.toString`, a
/// constructor `String.`.
/// @param word The word.
/// @param head Whether it is the first word of a list.
/// @return true if so.
bool hostName(std::string_view word, bool head) {
	if(word.empty() || word[0] == ':' || word[0] == '\\') return false;
	if(const auto slash = word.find('/'); slash != std::string_view::npos && slash > 0) {
		const std::string_view space = word.substr(0, slash);
		if(space == "js" || space == "cpp" || (space[0] >= 'A' && space[0] <= 'Z')) return true;
	}
	if(!head || word.size() < 2) return false;
	const bool number = word[1] >= '0' && word[1] <= '9';
	return (word[0] == '.' && !number) || (word.back() == '.' && word[0] >= 'A' && word[0] <= 'Z');
}

/// @brief The reason a word is out of scope wherever it is written.
/// @param word The word.
/// @return The reason, or "".
std::string_view wordReason(std::string_view word) {
	if(word.starts_with("##")) return "numbers"; // ##Inf, ##-Inf, ##NaN
	if(numberChangesValue(word)) return "numbers";
	if(word.starts_with("::")) return "namespaces";                                                        // ::k
	if(word.size() > 2 && word[0] == ':' && word.find('/') != std::string_view::npos) return "namespaces"; // :a/b
	if(word == "*ns*") return "namespaces";
	if(hostName(word, false)) return "host";
	return {};
}

/// @brief The edits the pass makes, and what it reads of the outline.
class Preparer {
public:
	/// @brief Starts on a text.
	/// @param text The text; outlives it.
	/// @param tokens Its tokens; outlive it.
	/// @param options What the language reads.
	Preparer(std::string_view text, const std::vector<Token>& tokens, const Options& options)
	    : text_(text), tokens_(tokens), options_(options) {}

	/// @brief Prepares the file from its outline. @param forms The top-level forms. @return The file.
	PreparedFile file(const std::vector<Node>& forms) {
		PreparedFile result;
		for(const Node& form : forms)
			if(headOf(form) == "ns") readNamespace(form, result);
		for(const Node& form : forms) {
			if(headOf(form) == "ns") continue;
			mark(form, false);
			TopForm top = topForm(form);
			if(top.head == "when-var-exists")
				for(std::size_t at = 2; at < form.children.size(); ++at) top.body.push_back(topForm(form.children[at]));
			result.assertions += top.assertions;
			result.forms.push_back(std::move(top));
		}
		for(const TopForm& top : result.forms) collect(top, result);
		result.unreadLine = unreadLine_;
		if(unreadLine_ != 0) result.unread = setLiteral;
		return result;
	}

private:
	/// @brief The text of a token. @param node A form. @return Its first token's text.
	std::string_view textOf(const Node& node) const { return tokens_[node.token].text; }
	/// @brief The kind of a form's first token. @param node A form. @return It.
	TokenKind kindOf(const Node& node) const { return tokens_[node.token].kind; }

	/// @brief Whether a form is a list `(...)`. @param node The form. @return true if so.
	bool isList(const Node& node) const { return kindOf(node) == TokenKind::Open && textOf(node) == "("; }

	/// @brief A list's first word, its namespace dropped, for a few namespaces that mean the same thing.
	/// @param node The form.
	/// @return The word, or "" for anything else.
	std::string_view headOf(const Node& node) const {
		if(!isList(node) || node.children.empty() || kindOf(node.children[0]) != TokenKind::Atom) return {};
		std::string_view word = textOf(node.children[0]);
		const auto slash = word.find('/');
		if(slash == std::string_view::npos || slash == 0 || slash + 1 == word.size()) return word;
		const std::string_view space = word.substr(0, slash);
		const bool known = space == "clojure.core" || space == "clojure.test" ||
		                   space == "clojure.core-test.portability" || space == testAlias_ || space == portableAlias_;
		return known ? word.substr(slash + 1) : word;
	}

	/// @brief The word after a list's first, past metadata. @param node The list. @return It, or "".
	std::string nameOf(const Node& node) const {
		for(std::size_t at = 1; at < node.children.size(); ++at) {
			const Node& child = node.children[at];
			if(kindOf(child) == TokenKind::Prefix && textOf(child) == "^") continue;
			return kindOf(child) == TokenKind::Atom ? std::string(textOf(child)) : std::string();
		}
		return {};
	}

	/// @brief The form a reader conditional gives the language: its `:affresco` or `:default` branch.
	/// @param node The conditional.
	/// @return The form, or nullptr when no branch is chosen.
	const Node* chosen(const Node& node) const {
		for(std::size_t at = 0; at + 1 < node.children.size(); at += 2) {
			const std::string_view platform = textOf(node.children[at]);
			if(platform == ":affresco" || platform == ":default") return &node.children[at + 1];
		}
		return nullptr;
	}

	/// @brief How many items a form is where it is written: 0 for a discarded one or a conditional that chooses
	/// nothing, the chosen vector's for a splicing one, else 1.
	/// @param node The form.
	/// @return The count.
	std::size_t itemsOf(const Node& node) const {
		const std::string_view text = textOf(node);
		if(kindOf(node) == TokenKind::Prefix && text == "#_") return 0;
		if(text == "#?(") return chosen(node) != nullptr ? 1 : 0;
		if(text != "#?@(") return 1;
		const Node* branch = chosen(node);
		if(branch == nullptr) return 0;
		std::size_t count = 0;
		for(const Node& child : branch->children) count += itemsOf(child);
		return count;
	}

	/// @brief The assertions written in a form, in the branches the language reads.
	/// @param node The form.
	/// @return An `is` is one, an `are` one per row.
	std::size_t assertionsIn(const Node& node) const {
		const std::string_view text = textOf(node);
		if(kindOf(node) == TokenKind::Prefix && text == "#_") return 0;
		if(text == "#?(" || text == "#?@(") {
			const Node* branch = chosen(node);
			return branch == nullptr ? 0 : assertionsIn(*branch);
		}
		const std::string_view head = headOf(node);
		if(head == "is") return 1;
		if(head == "are" && node.children.size() >= 3) {
			const std::size_t parameters = itemsOf(node.children[1]) == 0 ? 0 : [&] {
				std::size_t count = 0;
				for(const Node& child : node.children[1].children) count += itemsOf(child);
				return count;
			}();
			std::size_t items = 0;
			for(std::size_t at = 3; at < node.children.size(); ++at) items += itemsOf(node.children[at]);
			return parameters == 0 ? 0 : items / parameters;
		}
		std::size_t count = 0;
		for(const Node& child : node.children) count += assertionsIn(child);
		return count;
	}

	/// @brief Whether a form is text where a map's key or a set's item is: a keyword, a string, a character, or a word
	/// (whose value is unknown here: not marked).
	/// @param node The form.
	/// @return false for a number, nil, a boolean or a collection written out.
	bool textKey(const Node& node) const {
		const TokenKind kind = kindOf(node);
		const std::string_view text = textOf(node);
		if(kind == TokenKind::Open) return text == "(" || text == "#(" || text == "#?(" || text == "#?@(";
		if(kind != TokenKind::Atom) return true;
		if(text == "nil" || text == "true" || text == "false") return false;
		const std::string_view digitsPart = text.starts_with('-') || text.starts_with('+') ? text.substr(1) : text;
		return digitsPart.empty() || digitsPart[0] < '0' || digitsPart[0] > '9';
	}

	/// @brief The reason a collection written out is out of scope: a map or a set with a key that isn't text.
	/// @param node The collection.
	/// @return The reason, or "".
	std::string_view collectionReason(const Node& node) const {
		const std::string_view text = textOf(node);
		if(text != "{" && text != "#{") return {};
		const std::size_t step = text == "{" ? 2 : 1;
		std::size_t index = 0;
		for(const Node& child : node.children) {
			if(itemsOf(child) == 0) continue;
			if(index % step == 0 && !textKey(child)) return "keys";
			++index;
		}
		return {};
	}

	/// @brief The reason a list is out of scope, by the call it is.
	/// @param node The list.
	/// @return The reason, or "".
	std::string_view callReason(const Node& node) const {
		const std::string_view head = headOf(node);
		if(head.empty()) return {};
		if(const auto found = calls().find(head); found != calls().end()) return found->second;
		std::size_t arguments = 0;
		for(std::size_t at = 1; at < node.children.size(); ++at) arguments += itemsOf(node.children[at]);
		if((head == "range" && arguments == 0) || (head == "repeat" && arguments == 1)) return "laziness"; // infinite
		if(hostName(head, true)) return "host";
		return {};
	}

	/// @brief The reason a form is out of scope where it is written.
	/// @param node The form.
	/// @return The reason, or "".
	std::string_view reasonOf(const Node& node) const {
		const std::string_view text = textOf(node);
		switch(kindOf(node)) {
		case TokenKind::Regex:
			return "regex";
		case TokenKind::Atom:
			return wordReason(text);
		case TokenKind::Prefix:
			if(text == "@") return "mutation";                 // @x: an atom's value
			if(text == "#'") return "vars";                    // #'x
			if(text.starts_with("#:")) return "namespaces";    // #:ns{...}
			if(text == "#cpp" || text == "#js") return "host"; // jank's and ClojureScript's interop
			return {};
		case TokenKind::Open: {
			const std::string_view reason = collectionReason(node);
			return reason.empty() ? callReason(node) : reason;
		}
		default:
			return {};
		}
	}

	/// @brief Puts `(%out "reason")` in place of every form out of scope, the outermost, and writes each assertion's
	/// line into it (`(is x)` becomes `(cf--is 12 x)`).
	/// @param node The form.
	/// @param quoted Whether it is inside a quote, where the reader's own forms are still read but a call isn't run.
	void mark(const Node& node, bool quoted) {
		if(kindOf(node) == TokenKind::Prefix && textOf(node) == "#_") return;
		if(const std::string_view reason = options_.markOut ? reasonOf(node) : ""; !reason.empty()) {
			replace(node, "(%out \"" + std::string(reason) + "\")");
			return;
		}
		if(!options_.readsSets && options_.markOut && kindOf(node) == TokenKind::Open && textOf(node) == "#{") {
			replace(node, "(%out \"" + std::string(setsReason) + "\")");
			if(unreadLine_ == 0) unreadLine_ = tokens_[node.token].line;
			return;
		}
		const std::string_view head = headOf(node);
		if(!quoted && (head == "is" || head == "are")) {
			const Token& word = tokens_[node.children[0].token];
			edits_.push_back({word.begin, word.end, "cf--" + std::string(head) + " " + std::to_string(word.line)});
		}
		const bool inside = quoted || (kindOf(node) == TokenKind::Prefix && textOf(node) == "'");
		for(const Node& child : node.children) mark(child, inside);
	}

	/// @brief Puts a text in place of a form, with as many line breaks after it as the form held, so the lines after it
	/// keep their numbers.
	/// @param node The form.
	/// @param text What goes there.
	void replace(const Node& node, const std::string& text) {
		const Token& first = tokens_[node.token];
		const Token& last = tokens_[node.last];
		const auto lines = std::count(text_.begin() + static_cast<std::ptrdiff_t>(first.begin),
		                              text_.begin() + static_cast<std::ptrdiff_t>(last.end), '\n');
		edits_.push_back({first.begin, last.end, text + std::string(static_cast<std::size_t>(lines), '\n')});
	}

	/// @brief A top-level form, prepared. @param node The form. @return It.
	TopForm topForm(const Node& node) const {
		TopForm top;
		top.line = tokens_[node.token].line;
		top.text = edited(tokens_[node.token].begin, tokens_[node.last].end);
		top.head = std::string(headOf(node));
		top.name = nameOf(node);
		top.assertions = assertionsIn(node);
		return top;
	}

	/// @brief The text of a range with the edits inside it made.
	/// @param begin Its first byte.
	/// @param end One past its last.
	/// @return The text.
	std::string edited(std::size_t begin, std::size_t end) const {
		std::string result;
		std::size_t at = begin;
		for(const Edit& edit : edits_) {
			if(edit.begin < at || edit.end > end) continue;
			result.append(text_.substr(at, edit.begin - at));
			result.append(edit.text);
			at = edit.end;
		}
		result.append(text_.substr(at, end - at));
		return result;
	}

	/// @brief Reads the requirements and aliases of an `ns` form.
	/// @param node The form.
	/// @param file Where they go.
	void readNamespace(const Node& node, PreparedFile& file) {
		for(const Node& clause : node.children) {
			if(!isList(clause) || clause.children.empty() || textOf(clause.children[0]) != ":require") continue;
			for(std::size_t at = 1; at < clause.children.size(); ++at) {
				const Node* item = &clause.children[at];
				if(textOf(*item) == "#?(") item = chosen(*item);
				if(item != nullptr) readRequirement(*item, file);
			}
		}
	}

	/// @brief Reads one requirement: a library's name, or a vector of it and `:as alias`.
	/// @param node The requirement.
	/// @param file Where it goes.
	void readRequirement(const Node& node, PreparedFile& file) {
		if(kindOf(node) == TokenKind::Atom) {
			file.requirements.push_back({std::string(textOf(node)), ""});
			return;
		}
		if(textOf(node) != "[" || node.children.empty() || kindOf(node.children[0]) != TokenKind::Atom) return;
		Requirement requirement{std::string(textOf(node.children[0])), ""};
		for(std::size_t at = 1; at + 1 < node.children.size(); ++at)
			if(textOf(node.children[at]) == ":as" && kindOf(node.children[at + 1]) == TokenKind::Atom)
				requirement.alias = std::string(textOf(node.children[at + 1]));
		if(requirement.library == "clojure.test") testAlias_ = requirement.alias;
		if(requirement.library == "clojure.core-test.portability") portableAlias_ = requirement.alias;
		file.requirements.push_back(std::move(requirement));
	}

	/// @brief Collects the tests and the tested function of a top-level form and its body.
	/// @param top The form.
	/// @param file Where they go.
	static void collect(const TopForm& top, PreparedFile& file) {
		if(top.head == "deftest" && !top.name.empty()) file.tests.push_back(top.name);
		if(top.head == "when-var-exists" && file.tested.empty()) file.tested = top.name;
		for(const TopForm& inner : top.body) collect(inner, file);
	}

	/// @brief A replacement of a range of the text.
	struct Edit {
		std::size_t begin; ///< Its first byte.
		std::size_t end;   ///< One past its last.
		std::string text;  ///< What goes there.
	};

	/// @brief What a set the reader doesn't take is called.
	static constexpr std::string_view setLiteral = "a set literal #{...}";
	/// @brief The reason a set is out of scope while the reader doesn't take it: not a difference, a step not merged.
	static constexpr std::string_view setsReason = "sets (not merged yet)";

	std::string_view text_;            ///< The text.
	const std::vector<Token>& tokens_; ///< Its tokens.
	Options options_;                  ///< What the language reads.
	std::vector<Edit> edits_;          ///< The edits, in the order of the text: mark() walks it in order.
	std::size_t unreadLine_ = 0;       ///< The first line of a set put as `(%out "sets (not merged yet)")`, or 0.
	std::string testAlias_ = "t";      ///< clojure.test's alias in this file.
	std::string portableAlias_ = "p";  ///< The portability namespace's alias in this file.
};

} // namespace

const std::vector<std::string_view>& outReasons() {
	static const std::vector<std::string_view> reasons = {
	    "numbers",    "characters",           "regex", "vars", "mutation", "laziness", "host", "metadata", "keys",
	    "namespaces", "sets (not merged yet)"};
	return reasons;
}

PreparedFile prepare(std::string_view text, const Options& options) {
	const std::vector<Token> tokens = Tokenizer(text).tokens();
	Outliner outliner(tokens);
	const auto forms = outliner.forms();
	if(!forms) {
		PreparedFile failed;
		failed.error = outliner.error();
		return failed;
	}
	return Preparer(text, tokens, options).file(*forms);
}

} // namespace affresco::conformance
