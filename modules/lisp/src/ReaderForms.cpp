/// @file
/// @brief The reader's forms after a '#' (Reader.hpp): a function literal `#(...)`, a set literal `#{...}` and a
/// reader conditional `#?(...)`. The lexer gives each its own token, and the bracket after it is read here.

#include "Reader.hpp"

namespace affresco::lisp {

Value Reader::hashForm(const Token& token, int depth) {
	switch(token.kind) {
	case TokenKind::Lambda:
		return lambda(token, depth);
	case TokenKind::SetOf:
		return setOf(token, depth);
	default:
		return conditional(token, depth);
	}
}

Value Reader::lambda(const Token& token, int /*depth*/) {
	lexer_.fail(token.offset, "a function literal #(...) isn't read yet");
}

Value Reader::setOf(const Token& token, int /*depth*/) {
	lexer_.fail(token.offset, "a set literal #{...} isn't in the language: write a vector");
}

Value Reader::conditional(const Token& token, int /*depth*/) {
	lexer_.fail(token.offset, "reader conditionals #?(...) aren't read yet");
}

} // namespace affresco::lisp
