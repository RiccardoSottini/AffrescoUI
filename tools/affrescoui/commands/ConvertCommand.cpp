/// @file
/// @brief `affrescoui convert`: a file written in the other notation (modules/notation): JSON as AffrescoUI files are
/// written (`fmt`), Lisp canonically. Comments in a Lisp file are dropped; `note` keys keep their text.

#include "Cli.hpp"
#include "JsonText.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/notation/Notation.hpp>

#include <array>
#include <iostream>

namespace affresco::cli {

Exit convertFile(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 2> valued = {"--to", "-o"};
	const Arguments arguments(args, valued, {});
	const std::filesystem::path file = arguments.file();
	const notation::Notation from = notation::notationOf(file);
	const std::optional<std::string> named = arguments.value("--to");
	const std::optional<notation::Notation> to =
	    named ? notation::notationNamed(*named)
		      : (from == notation::Notation::Json ? notation::Notation::Lisp : notation::Notation::Json);
	if(!to) throw UsageError("--to: expected json or lisp");

	json value;
	try {
		value = notation::read(readFile(file), from);
	} catch(const json::parse_error& error) {
		throw FileProblem(file.string() + ": invalid JSON: " + error.what());
	} catch(const DocumentError& error) {
		throw FileProblem(file.string() + ": " + error.what());
	}
	const std::string text =
	    *to == notation::Notation::Json ? printText(parseText(value.dump())) : notation::write(value, *to);
	if(const std::optional<std::string> out = arguments.value("-o")) writeFile(*out, text);
	else std::cout << text;
	return Exit::Success;
}

} // namespace affresco::cli
