/// @file
/// @brief The notations (Notation.hpp): which one a file is written in, and reading and writing in either.

#include "affresco/notation/Notation.hpp"

#include "Lisp.hpp"

#include <algorithm>
#include <cctype>

namespace affresco::notation {

NotationError::NotationError(const std::string& file, std::size_t line, std::size_t column, const std::string& message)
    : DocumentError("AffrescoUI screen: " + (file.empty() ? "" : file + ": ") + "line " + std::to_string(line) +
	                ", column " + std::to_string(column) + ": " + message),
      line_(line), column_(column), message_(message) {}

Notation notationOf(const std::filesystem::path& file) {
	std::string extension = file.extension().string();
	std::ranges::transform(extension, extension.begin(),
	                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension == ".lisp" ? Notation::Lisp : Notation::Json;
}

std::optional<Notation> notationNamed(std::string_view name) noexcept {
	if(name == "json") return Notation::Json;
	if(name == "lisp") return Notation::Lisp;
	return std::nullopt;
}

std::string_view nameOf(Notation notation) noexcept { return notation == Notation::Lisp ? "lisp" : "json"; }

json read(std::string_view text, Notation notation) {
	return notation == Notation::Lisp ? detail::readLisp(text) : parseUntrusted(text);
}

json readDocument(std::string_view text, const std::filesystem::path& file) {
	const Notation notation = notationOf(file);
	if(notation == Notation::Json) return parseUntrusted(text);
	try {
		return read(text, notation);
	} catch(const NotationError& error) { // the same problem, with the file it is in
		throw NotationError(file.string(), error.line(), error.column(), error.message());
	}
}

std::string jsonText(std::string_view text, const std::filesystem::path& file) {
	if(notationOf(file) == Notation::Json) return std::string(text);
	return readDocument(text, file).dump();
}

std::string write(const json& value, Notation notation) {
	if(nestingDepth(value) > maxNesting)
		throw DocumentError("AffrescoUI screen: a value nested more than " + std::to_string(maxNesting) +
		                    " levels deep can't be written");
	return notation == Notation::Lisp ? detail::writeLisp(value) : value.dump();
}

} // namespace affresco::notation
