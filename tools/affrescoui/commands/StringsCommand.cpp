/// @file
/// @brief `affrescoui strings`: a screen's visible text: what is still written in place, what the language files miss,
/// and moving text to one language file per language. Only what must change in a file changes: its layout stays.

#include "Cli.hpp"
#include "JsonText.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>
#include <affresco/format/Literals.hpp>
#include <affresco/notation/Notation.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <set>

namespace affresco::cli {

namespace {

namespace fs = std::filesystem;

/// The options that have no value.
constexpr std::array<std::string_view, 2> stringsFlags = {"--json", "--write"};

/// The members of "strings" that aren't languages.
constexpr std::array<std::string_view, 4> settingKeys = {"default", "locale", "required", "keep"};

/// How deep includes are followed; the library refuses deeper ones when it loads the screen.
constexpr int mostIncludeDepth = 32;

/// @brief One of the files a screen is written in.
struct OwnFile {
	fs::path path; ///< Where it is.
	json document; ///< What it holds.
};

/// @brief A screen as `strings` works on it.
struct App {
	fs::path entry;             ///< Its screen file.
	std::vector<OwnFile> files; ///< That file and what it includes from its own folder, not what it shares with others.
	json merged;                ///< The document, its includes merged.
	format::TextVocabulary vocabulary; ///< What says which values a person reads.
};

/// @brief A change to a file's text.
struct Splice {
	std::size_t at;     ///< Where it starts.
	std::size_t length; ///< What it replaces.
	std::string text;   ///< What goes there.
};

/// @brief Reads a file in its notation.
/// @param path The file.
/// @return Its document.
/// @throws FileProblem If it isn't a value in its notation.
json readDocument(const fs::path& path) {
	try {
		return notation::read(readFile(path), notation::notationOf(path));
	} catch(const json::parse_error& error) {
		throw FileProblem(path.string() + ": invalid JSON: " + error.what());
	} catch(const DocumentError& error) {
		throw FileProblem(path.string() + ": " + error.what());
	}
}

/// @brief Adds a file and what it includes from inside a folder.
/// @param path The file.
/// @param folder The folder.
/// @param files Where they go.
/// @param depth How deep it is.
void addOwnFiles(const fs::path& path, const fs::path& folder, std::vector<OwnFile>& files, int depth) {
	const fs::path normal = path.lexically_normal();
	if(depth > mostIncludeDepth || std::ranges::any_of(files, [&](const OwnFile& f) { return f.path == normal; }))
		return;
	files.push_back({normal, readDocument(normal)});
	const json named = json(files.back().document).value("include", json::array());
	for(const json& include : named.is_array() ? named : json::array({named})) {
		if(!include.is_string()) continue;
		const fs::path part = (normal.parent_path() / include.get<std::string>()).lexically_normal();
		if(!part.lexically_relative(folder).generic_string().starts_with("../"))
			addOwnFiles(part, folder, files, depth + 1);
	}
}

/// @brief Loads a screen as the host would: an app's entry with what its manifest brings (its themes and models).
/// @param entry The screen file.
/// @return The screen.
/// @throws DocumentError If it doesn't load.
Screen screenOf(const fs::path& entry) {
	std::filesystem::path manifest;
	try {
		manifest = Screen::appOf(entry);
	} catch(const DocumentError&) { // it belongs to no app
	}
	if(!manifest.empty() && fs::exists(manifest)) return Screen::fromApp(manifest, {}, Screen::History::InMemory);
	return Screen::fromString(notation::jsonText(readFile(entry), entry), entry.parent_path());
}

/// @brief Reads a screen, or the screen of an app's folder, as the library loads it.
/// @param given The file or folder.
/// @return It.
/// @throws FileProblem If it doesn't load.
App readApp(const fs::path& given) {
	App app;
	app.entry = given;
	if(fs::is_directory(given)) {
		app.entry = given / format::parseApp(parseUntrusted(readFile(given / "app.json"))).entry;
	}
	addOwnFiles(app.entry, app.entry.parent_path().lexically_normal(), app.files, 0);
	setLogSink([](LogLevel, std::string_view) {});
	try {
		Screen screen = screenOf(app.entry);
		registerApplicationTypes(screen);
		setLogSink({});
		app.merged = json::parse(screen.documentJson());
		app.vocabulary = format::textVocabulary(json::parse(screen.catalogJson()), app.merged);
	} catch(const DocumentError& error) {
		setLogSink({});
		throw FileProblem(app.entry.string() + ": " + error.what());
	}
	return app;
}

/// @brief The merged document's "strings" section.
/// @param app The screen.
/// @return It, or an empty object.
json stringsOf(const App& app) {
	const json section = app.merged.value("strings", json::object());
	return section.is_object() ? section : json::object();
}

/// @brief Whether a member of "strings" is a language.
/// @param key The member's name.
/// @param value Its value.
/// @return Whether it is.
bool isLanguage(std::string_view key, const json& value) {
	return value.is_object() && std::ranges::find(settingKeys, key) == settingKeys.end();
}

/// @brief What's written in place in each of a screen's files.
/// @param app The screen.
/// @return The text found, with the file it is in.
std::vector<std::pair<const OwnFile*, format::LiteralText>> literalsOf(const App& app) {
	const json keep = stringsOf(app).value("keep", json::array());
	std::vector<std::pair<const OwnFile*, format::LiteralText>> found;
	for(const OwnFile& file : app.files)
		for(format::LiteralText& literal : format::literalTexts(file.document, app.vocabulary, keep))
			found.emplace_back(&file, std::move(literal));
	return found;
}

/// @brief The name to show for a file.
/// @param app The screen.
/// @param file The file.
/// @return Its path from the screen's folder.
std::string shownName(const App& app, const OwnFile& file) {
	return file.path.lexically_relative(app.entry.parent_path().lexically_normal()).generic_string();
}

/// @brief Which keys a value's expressions name, through every string in it.
struct KeysRead {
	std::set<std::string, std::less<>> exact;    ///< `t('a.b')`.
	std::set<std::string, std::less<>> prefixes; ///< `t('a.' + x)`.
	bool computed = false;                       ///< `t(x)`: any key may be read.

	/// @brief Reads a value.
	/// @param value The value.
	void read(const json& value) {
		if(value.is_string()) readText(value.get_ref<const std::string&>());
		else if(value.is_structured())
			for(const json& member : value) read(member);
	}

	/// @brief Reads a text.
	/// @param text The text.
	void readText(std::string_view text) {
		for(std::size_t at = text.find("t("); at != std::string_view::npos; at = text.find("t(", at + 2)) {
			if(at > 0 && (std::isalnum(static_cast<unsigned char>(text[at - 1])) != 0 || text[at - 1] == '_' ||
			              text[at - 1] == '.'))
				continue;
			std::size_t i = at + 2;
			while(i < text.size() && text[i] == ' ') ++i;
			if(i >= text.size() || (text[i] != '\'' && text[i] != '"')) {
				computed = true;
				continue;
			}
			const std::size_t end = text.find(text[i], i + 1);
			if(end == std::string_view::npos) continue;
			const std::string key(text.substr(i + 1, end - i - 1));
			std::size_t next = end + 1;
			while(next < text.size() && text[next] == ' ') ++next;
			if(next < text.size() && (text[next] == ',' || text[next] == ')')) exact.insert(key);
			else prefixes.insert(key);
		}
	}

	/// @brief Whether a key is read.
	/// @param key The key.
	/// @return Whether it is.
	[[nodiscard]] bool has(const std::string& key) const {
		return exact.contains(key) ||
		       std::ranges::any_of(prefixes, [&](const std::string& prefix) { return key.starts_with(prefix); });
	}
};

/// @brief What the language files of a screen's own files hold and miss.
struct Coverage {
	std::map<std::string, std::set<std::string>> languages;  ///< The keys of each language.
	std::map<std::string, std::vector<std::string>> missing; ///< Each language's missing keys.
	std::vector<std::string> unused;                         ///< Keys nothing reads.
	bool computed = false;                                   ///< Keys are computed, so `unused` can't be told.
};

/// @brief Reads what a screen's own language files hold and miss.
/// @param app The screen.
/// @return It.
Coverage coverageOf(const App& app) {
	Coverage coverage;
	for(const OwnFile& file : app.files) {
		const json section = file.document.value("strings", json::object());
		if(!section.is_object()) continue;
		for(const auto& [language, table] : section.items())
			if(isLanguage(language, table))
				for(const auto& [key, message] : table.items()) coverage.languages[language].insert(key);
	}
	std::set<std::string> every;
	for(const auto& [language, keys] : coverage.languages) every.insert(keys.begin(), keys.end());
	for(const auto& [language, keys] : coverage.languages)
		for(const std::string& key : every)
			if(!keys.contains(key)) coverage.missing[language].push_back(key);
	KeysRead read;
	for(const auto& [section, value] : app.merged.items())
		if(section != "strings") read.read(value);
	coverage.computed = read.computed;
	for(const std::string& key : every)
		if(!read.has(key)) coverage.unused.push_back(key);
	return coverage;
}

/// @brief Applies changes to a text.
/// @param text The text.
/// @param splices The changes, which don't overlap.
/// @return The new text.
std::string spliced(std::string text, std::vector<Splice> splices) {
	std::ranges::sort(splices, std::greater<>{}, &Splice::at); // from the end, so the offsets before stay right
	for(const Splice& splice : splices) text.replace(splice.at, splice.length, splice.text);
	return text;
}

/// @brief The change that puts files first in a screen's "include": first, so that the screen's own text still wins
/// over what the other includes say, as it did while it held that text itself.
/// @param tree The screen, as written.
/// @param names The files to add.
/// @return The change.
Splice includeFirst(TextValue& tree, const std::vector<std::string>& names) {
	std::string listed;
	for(const std::string& name : names) listed += (listed.empty() ? "" : ", ") + json(name).dump();
	TextValue* include = memberOf(tree, "include");
	if(include == nullptr) {
		const TextValue* after = memberOf(tree, "version");
		if(after == nullptr) throw std::runtime_error("the screen has no \"version\"");
		return {after->end, 0, std::format(",{}\"include\": [{}]", tree.lead, listed)};
	}
	if(include->kind == TextValue::Kind::Scalar)
		return {include->start, include->end - include->start, std::format("[{}, {}]", listed, include->text)};
	if(include->members.empty()) return {include->start + 1, 0, listed};
	std::string each;
	for(const std::string& name : names) each += include->lead + json(name).dump() + ",";
	if(include->lead.empty()) each = listed + ", ";
	return {include->start + 1, 0, each};
}

/// @brief Adds messages to a language's file, making it if it isn't there yet.
/// @param folder The screen's folder.
/// @param language The language.
/// @param messages Its keys and messages.
/// @param extension The file's extension.
/// @return The file's name as an include names it, and whether it is new.
std::pair<std::string, bool> addToLanguageFile(const fs::path& folder, const std::string& language,
                                               const json& messages, std::string_view extension) {
	const std::string name = std::format("locales/{}{}", language, extension);
	const fs::path path = folder / name;
	const bool isNew = !fs::exists(path);
	json document = isNew ? json{{"strings", {{language, json::object()}}}} : readDocument(path);
	json& table = document["strings"][language];
	for(const auto& [key, message] : messages.items()) {
		if(table.contains(key) && table[key] != message)
			throw FileProblem(std::format("{}: '{}' already says something else", path.string(), key));
		table[key] = message;
	}
	fs::create_directories(path.parent_path());
	writeFile(path, printText(parseText(document.dump(1, '\t'))));
	return {name, isNew};
}

/// @brief The texts of the languages a file's "strings" holds, and the change that takes them out of its text.
/// @param tree The file, as written.
/// @param section Its "strings".
/// @return The changes.
std::vector<Splice> withoutLanguages(TextValue& tree, const json& section) {
	TextValue* written = memberOf(tree, "strings");
	if(written == nullptr || written->kind != TextValue::Kind::Object) return {};
	std::vector<Splice> splices;
	const auto& members = written->members;
	const auto isMoved = [&](const TextValue::Member& member) {
		const std::string key = json::parse(member.key).get<std::string>();
		return section.contains(key) && isLanguage(key, section[key]);
	};
	for(std::size_t i = 0; i < members.size(); ++i) {
		if(!isMoved(members[i])) continue;
		std::size_t last = i;
		while(last + 1 < members.size() && isMoved(members[last + 1])) ++last;
		if(last + 1 < members.size())
			splices.push_back({members[i].start, members[last + 1].start - members[i].start, ""});
		else if(i > 0)
			splices.push_back({members[i - 1].value.end, members[last].value.end - members[i - 1].value.end, ""});
		else // nothing else in it: "strings" goes too
			for(std::size_t m = 0; m < tree.members.size(); ++m)
				if(&tree.members[m].value == written) {
					const std::size_t from = m > 0 ? tree.members[m - 1].value.end : written->start;
					const std::size_t to = m + 1 < tree.members.size() && m == 0 ? tree.members[1].start : written->end;
					splices.push_back({from, to - from, ""});
				}
		i = last;
	}
	return splices;
}

/// @brief Refuses a file the Lisp notation holds: Lisp files are generated from the JSON ones.
/// @param path The file.
/// @throws FileProblem If it is one.
void refuseLisp(const fs::path& path) {
	if(notation::notationOf(path) == notation::Notation::Lisp)
		throw FileProblem(path.string() + ": a change starts from the JSON file; the Lisp notation can then be written "
		                                  "from it with `affrescoui convert`");
}

/// @brief `strings report` and `strings check`: lists what's written in place, and what the language files miss.
/// @param app The screen.
/// @param asJson Answer in JSON.
/// @param check Exit with Problems if there is anything to fix.
/// @return How it ended.
Exit report(const App& app, bool asJson, bool check) {
	const auto literals = literalsOf(app);
	const Coverage coverage = coverageOf(app);
	const bool clean = literals.empty() && coverage.missing.empty() && (coverage.computed || coverage.unused.empty());
	if(asJson) {
		json found = json::array();
		for(const auto& [file, literal] : literals)
			found.push_back({{"file", shownName(app, *file)},
			                 {"at", literal.pointer},
			                 {"text", literal.text},
			                 {"key", literal.key},
			                 {"built", literal.built}});
		json languages = json::object();
		for(const auto& [language, keys] : coverage.languages) languages[language] = keys.size();
		std::cout << json{{"file", app.entry.string()},
		                  {"ok", clean},
		                  {"literals", std::move(found)},
		                  {"languages", std::move(languages)},
		                  {"missing", coverage.missing},
		                  {"unused", coverage.computed ? json() : json(coverage.unused)}}
		                 .dump(1, '\t')
		          << '\n';
		return check && !clean ? Exit::Problems : Exit::Success;
	}
	for(const auto& [file, literal] : literals)
		std::cout << std::format("{}: {}: {}{}\n", shownName(app, *file), literal.pointer, json(literal.text).dump(),
		                         literal.built ? " (text around an expression: rewrite it as a message)" : "");
	for(const auto& [language, keys] : coverage.languages)
		std::cout << std::format("{}: {} keys\n", language, keys.size());
	for(const auto& [language, keys] : coverage.missing)
		for(const std::string& key : keys) std::cout << std::format("{}: no '{}'\n", language, key);
	if(coverage.computed) std::cout << "some keys are computed, so unused keys can't be told\n";
	else
		for(const std::string& key : coverage.unused) std::cout << std::format("nothing reads '{}'\n", key);
	std::cout << std::format("{} texts written in place\n", literals.size());
	return check && !clean ? Exit::Problems : Exit::Success;
}

/// @brief `strings extract`: moves literal text to the default language's file, each replaced by its key.
/// @param app The screen.
/// @param write Write the files; else only list.
/// @return How it ended.
Exit extract(const App& app, bool write) {
	refuseLisp(app.entry);
	const json section = stringsOf(app);
	const std::string language = section.value("default", std::string("en"));
	json messages = json::object();
	const json known = section.value(language, json::object());
	std::map<const OwnFile*, std::vector<Splice>> edits;
	std::map<const OwnFile*, TextValue> trees;
	std::map<const OwnFile*, std::string> texts;
	std::size_t left = 0;
	for(const auto& [file, literal] : literalsOf(app)) {
		if(literal.built) {
			std::cout << std::format("{}: {}: left: text around an expression: {}\n", shownName(app, *file),
			                         literal.pointer, json(literal.text).dump());
			++left;
			continue;
		}
		const std::string message = format::messageOfText(literal.text);
		std::string key = literal.key;
		for(int n = 2; (messages.contains(key) && messages[key] != message) || known.contains(key); ++n)
			key = std::format("{}-{}", literal.key, n);
		messages[key] = message;
		std::cout << std::format("{}: {} -> {}\n", shownName(app, *file), literal.pointer, key);
		if(!texts.contains(file)) {
			refuseLisp(file->path);
			texts[file] = readFile(file->path);
			trees[file] = parseText(texts[file]);
		}
		const TextValue* value = valueAt(trees[file], literal.pointer);
		if(value == nullptr) throw std::runtime_error(file->path.string() + ": nothing at " + literal.pointer);
		edits[file].push_back(
		    {value->start, value->end - value->start, json(std::format("{{{{t('{}')}}}}", key)).dump()});
	}
	std::cout << std::format("{} texts to move, {} left\n", messages.size(), left);
	if(!write || messages.empty()) return Exit::Success;
	const auto [name, isNew] = addToLanguageFile(app.entry.parent_path(), language, messages, ".json");
	const OwnFile* entry = &app.files.front();
	if(!texts.contains(entry)) {
		texts[entry] = readFile(entry->path);
		trees[entry] = parseText(texts[entry]);
	}
	if(isNew) edits[entry].push_back(includeFirst(trees[entry], {name}));
	for(auto& [file, splices] : edits) writeFile(file->path, spliced(texts[file], std::move(splices)));
	return Exit::Success;
}

/// @brief `strings split`: moves the languages a screen holds in its own "strings" to one file each.
/// @param app The screen.
/// @param write Write the files; else only list.
/// @return How it ended.
Exit split(const App& app, bool write) {
	refuseLisp(app.entry);
	const OwnFile& entry = app.files.front();
	const json section = entry.document.value("strings", json::object());
	std::vector<std::pair<std::string, const json*>> languages;
	for(const auto& [language, table] : section.items())
		if(isLanguage(language, table)) languages.emplace_back(language, &table);
	for(const auto& [language, table] : languages)
		std::cout << std::format("{}: {} keys -> locales/{}.json\n", language, table->size(), language);
	if(!write || languages.empty()) return Exit::Success;
	std::string text = readFile(entry.path);
	TextValue tree = parseText(text);
	std::vector<std::string> names;
	for(const auto& [language, table] : languages) {
		const auto [name, isNew] = addToLanguageFile(app.entry.parent_path(), language, *table, ".json");
		if(isNew) names.push_back(name);
	}
	std::vector<Splice> splices = withoutLanguages(tree, section);
	if(!names.empty()) splices.push_back(includeFirst(tree, names));
	writeFile(entry.path, spliced(std::move(text), std::move(splices)));
	return Exit::Success;
}

} // namespace

std::vector<std::string> requiredStringsProblems(const fs::path& file) {
	const App app = readApp(file);
	std::vector<std::string> problems;
	if(!stringsOf(app).value("required", false)) return problems;
	for(const auto& [own, literal] : literalsOf(app))
		problems.push_back(std::format("{}: {}: {} is written in place", shownName(app, *own), literal.pointer,
		                               json(literal.text).dump()));
	const Coverage coverage = coverageOf(app);
	for(const auto& [language, keys] : coverage.missing)
		for(const std::string& key : keys) problems.push_back(std::format("{} has no '{}'", language, key));
	return problems;
}

Exit stringsCommand(std::span<const std::string_view> args) {
	if(args.empty()) throw UsageError("strings takes report, check, extract or split");
	const std::string_view action = args[0];
	const Arguments arguments(args.subspan(1), {}, stringsFlags);
	const fs::path file = arguments.file();
	if(action == "report" || action == "check")
		return report(readApp(file), arguments.flag("--json"), action == "check");
	if(action == "extract") return extract(readApp(file), arguments.flag("--write"));
	if(action == "split") return split(readApp(file), arguments.flag("--write"));
	throw UsageError("strings takes report, check, extract or split");
}

} // namespace affresco::cli
