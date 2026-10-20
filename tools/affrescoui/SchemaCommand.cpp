/// @file
/// @brief `affrescoui schema`: the format's JSON Schema and SCREEN_FORMAT.md's tables, generated from the registries.

#include "Cli.hpp"

#include <affresco/affresco.hpp>

#include <array>
#include <iostream>

namespace affresco::cli {

namespace {

/// The options schema takes that have a value.
constexpr std::array<std::string_view, 3> schemaOptions = {"-o", "--doc", "--types"};

/// The flags schema takes.
constexpr std::array<std::string_view, 1> schemaFlags = {"--check"};

/// The smallest document there is: the registries are the screen's, whatever it shows.
constexpr std::string_view emptyScreen = R"({"format": "affrescoui-screen", "version": 1, "root": {}})";

/// @brief Writes a generated file, or with --check says whether it is out of date.
/// @param file The file.
/// @param text What it should hold.
/// @param check Only say whether it would change.
/// @return Success, or Problems if --check finds it out of date.
Exit update(const std::filesystem::path& file, const std::string& text, bool check) {
	if(check) {
		if(readFile(file) == text) return Exit::Success;
		std::cout << file.string() << ": out of date (run affrescoui schema)\n";
		return Exit::Problems;
	}
	writeFile(file, text);
	return Exit::Success;
}

/// @brief A screen with the types asked for.
/// @param types "builtin" (the default), or "examples" for the example apps' types too.
/// @return The screen.
/// @throws UsageError If the types are unknown, or the tool was built without the examples.
Screen screenWith(std::string_view types) {
	Screen screen = Screen::fromString(emptyScreen);
	if(types == "examples") {
#ifndef AFFRESCOUI_CLI_EXAMPLE_TYPES
		throw UsageError("this affrescoui was built without the examples");
#endif
		registerApplicationTypes(screen);
	} else if(types != "builtin") {
		throw UsageError("--types takes builtin or examples");
	}
	return screen;
}

/// @brief Generates SCREEN_FORMAT.md's tables into it, or with --check says whether they are out of date.
/// @param screen The screen whose registries say what the tables hold.
/// @param doc The document.
/// @param check Only say whether it would change.
/// @return Success, or Problems if --check finds it out of date.
/// @throws FileProblem If its markers are wrong.
Exit updateTables(const Screen& screen, const std::filesystem::path& doc, bool check) {
	std::string tables;
	try {
		tables = screen.referenceTables(readFile(doc));
	} catch(const std::invalid_argument& error) {
		throw FileProblem(doc.string() + ": " + error.what());
	}
	return update(doc, tables, check);
}

} // namespace

Exit generateSchema(std::span<const std::string_view> args) {
	const Arguments arguments(args, schemaOptions, schemaFlags);
	if(!arguments.positional().empty()) throw UsageError("schema takes no file; -o names where to write it");
	const bool check = arguments.flag("--check");
	const auto file = arguments.value("-o"), doc = arguments.value("--doc");
	if(check && !file && !doc) throw UsageError("--check needs -o or --doc");
	const Screen screen = screenWith(arguments.value("--types").value_or("builtin"));
	const std::string schema = screen.schema().dump(1, '\t') + '\n';
	Exit result = Exit::Success;
	if(file) result = update(*file, schema, check);
	else if(!doc) std::cout << schema;
	if(doc && updateTables(screen, *doc, check) != Exit::Success) result = Exit::Problems;
	return result;
}

} // namespace affresco::cli
