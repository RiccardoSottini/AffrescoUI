/// @file
/// @brief Where a program's nodes come from (lisp::nodeOrigins()): the definition that holds the form that built each
/// node with an id written out, and the line and column of that form inside the definition's own text.

#include <affresco/affresco.hpp>
#include <affresco/lisp/Document.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace lisp = affresco::lisp;
using affresco::json;

namespace {

/// @brief The origins of a program's nodes, the program given as one file's text.
/// @param text The program.
/// @return The origins.
lisp::NodeOrigins originsOf(const char* text) {
	return *lisp::nodeOrigins(lisp::sourceDocument(text, "app.lisp")["program"]);
}

/// @brief The byte where a line and a column, in characters, are in a text.
/// @param text The text.
/// @param line The line, from 1.
/// @param column The column, from 1, in characters.
/// @return The byte, or text.size() if the text is shorter.
std::size_t offsetOf(std::string_view text, std::uint32_t line, std::uint32_t column) {
	std::size_t at = 0;
	for(std::uint32_t lines = 1; lines < line && at < text.size(); ++at)
		if(text[at] == '\n') ++lines;
	for(std::uint32_t columns = 1; columns < column && at < text.size(); ++columns) {
		++at; // past one character: its first byte and those that continue it
		while(at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0U) == 0x80U) ++at;
	}
	return at;
}

/// @brief Reads the items directly inside a list, one byte at a time (itemsOfList()).
struct ListScan {
	std::string_view text{};                    ///< The text.
	std::vector<std::string> items{};           ///< The items read so far.
	int depth = 0;                              ///< How many brackets the byte read is inside.
	std::size_t start = std::string_view::npos; ///< Where the item being read started, or npos for none.

	/// @brief Starts an item at a byte, unless one is being read; only at the list's own depth.
	/// @param at The byte.
	void open(std::size_t at) {
		if(depth == 1 && start == std::string_view::npos) start = at;
	}

	/// @brief Ends the item being read, if it is at the list's own depth.
	/// @param end Where it ends.
	void close(std::size_t end) {
		if(depth != 1) return;
		if(start != std::string_view::npos) items.emplace_back(text.substr(start, end - start));
		start = std::string_view::npos;
	}

	/// @brief Reads a string, an item of its own at the list's depth.
	/// @param at Where its '"' is.
	/// @return Where its closing '"' is, or the text's size.
	std::size_t string(std::size_t at) {
		open(at);
		for(++at; at < text.size() && text[at] != '"'; ++at)
			if(text[at] == '\\') ++at;
		if(depth == 1) close(at + 1);
		return at;
	}

	/// @brief Reads a comment, which ends the item before it.
	/// @param at Where its ';' is.
	/// @return Where the line ends, or the text's size.
	std::size_t comment(std::size_t at) {
		close(at);
		while(at < text.size() && text[at] != '\n') ++at;
		return at;
	}

	/// @brief Reads a closing bracket.
	/// @param at Where it is.
	/// @return true if it closes the list.
	bool shut(std::size_t at) {
		if(depth == 1) close(at);
		if(--depth == 1) close(at + 1);
		return depth == 0;
	}
};

/// @brief The words, strings and bracketed forms directly inside the list that starts at a byte, each as its text.
/// @param text The text.
/// @param at Where the list's '(' is.
/// @return Them; empty if no list starts there.
std::vector<std::string> itemsOfList(std::string_view text, std::size_t at) {
	constexpr std::string_view opening = "([{";
	constexpr std::string_view closing = ")]}";
	constexpr std::string_view spaces = " \n\t,";
	ListScan scan{.text = text};
	if(at >= text.size() || text[at] != '(') return scan.items;
	for(std::size_t i = at; i < text.size(); ++i) {
		const char c = text[i];
		if(c == '"') {
			i = scan.string(i);
		} else if(c == ';') {
			i = scan.comment(i);
		} else if(opening.contains(c)) {
			scan.open(i);
			++scan.depth;
		} else if(closing.contains(c)) {
			if(scan.shut(i)) return scan.items;
		} else if(spaces.contains(c)) {
			scan.close(i);
		} else {
			scan.open(i);
		}
	}
	return scan.items;
}

/// @brief Whether a list's items give a node that id, written out: `type#id`, or `:id "id"`.
/// @param items The list's items (itemsOfList()).
/// @param id The id.
/// @return true if so.
bool writesId(const std::vector<std::string>& items, const std::string& id) {
	if(items.empty()) return false;
	if(const std::size_t hash = items[0].find('#'); hash != std::string::npos) return items[0].substr(hash + 1) == id;
	for(std::size_t at = 1; at + 1 < items.size(); ++at)
		if(items[at] == ":id") return items[at + 1] == json(id).dump();
	return false;
}

/// @brief Whether a list's items give a node an id with an expression: `:id ~name` or `:id ~(str "row" i)`.
/// @param items The list's items (itemsOfList()).
/// @return true if so.
bool computesId(const std::vector<std::string>& items) {
	for(std::size_t at = 1; at + 1 < items.size(); ++at)
		if(items[at] == ":id") return items[at + 1].starts_with('~');
	return false;
}

/// @brief The ids of the nodes in a tree: those of the objects with a `type`, a `use` or `children` beside an `id`.
/// @param tree The tree.
/// @param ids Gets them.
void collectNodeIds(const json& tree, std::set<std::string>& ids) {
	if(tree.is_object()) {
		if(const auto id = tree.find("id");
		   id != tree.end() && id->is_string() &&
		   (tree.contains("type") || tree.contains("use") || tree.contains("children")))
			ids.insert(id->get<std::string>());
		for(const auto& [key, member] : tree.items()) collectNodeIds(member, ids);
	} else if(tree.is_array()) {
		for(const json& item : tree) collectNodeIds(item, ids);
	}
}

} // namespace

TEST_CASE("lisp origins: a view's node, and a helper view's, are where their forms are") {
	const lisp::NodeOrigins origins = originsOf(R"lisp(
(defview toolbar "The bar." []
  `(row#bar
     (button :id "save" :text "Save")))
(defscreen main
  (column#page
    ~(toolbar)))
)lisp");
	REQUIRE(origins.contains("bar"));
	CHECK(origins.at("bar").file == "app.lisp");
	CHECK(origins.at("bar").definition == "toolbar");
	CHECK(origins.at("bar").kind == "defview");
	CHECK(origins.at("bar").line == 2);
	CHECK(origins.at("bar").column == 4);
	REQUIRE(origins.contains("save"));
	CHECK(origins.at("save").definition == "toolbar"); // the helper, not main that calls it
	CHECK(origins.at("save").line == 3);
	CHECK(origins.at("save").column == 6);
	REQUIRE(origins.contains("page"));
	CHECK(origins.at("page").definition == "main");
	CHECK(origins.at("page").kind == "defscreen");
	CHECK(origins.at("page").line == 2);
	CHECK(origins.at("page").column == 3);
}

TEST_CASE("lisp origins: a view called twice has one origin, its own") {
	const lisp::NodeOrigins origins = originsOf(R"lisp(
(defview badge [] `(label#badge :text "1"))
(defscreen main (column (row ~(badge)) (row ~(badge))))
)lisp");
	REQUIRE(origins.size() == 1);
	CHECK(origins.at("badge").definition == "badge");
	CHECK(origins.at("badge").line == 1);
	CHECK(origins.at("badge").column == 20);
}

TEST_CASE("lisp origins: a repeat's node, a component's inner node and a macro's template") {
	const lisp::NodeOrigins origins = originsOf(R"lisp(
(defcomponent chip "A chip." {:label ""}
  `(row
     (label#chip-label :text ~label)))
(defmacro framed [& body]
  `'(box#frame ~@body))
(defscreen main
  (column#list
    ~(repeat [item state.items] `(label :id "row" :text ~item.name))
    ~(framed (label#inside :text "x"))))
)lisp");
	REQUIRE(origins.contains("row"));
	CHECK(origins.at("row").definition == "main");
	CHECK(origins.at("row").line == 3);
	CHECK(origins.at("row").column == 34);
	REQUIRE(origins.contains("chip-label"));
	CHECK(origins.at("chip-label").definition == "components.chip");
	CHECK(origins.at("chip-label").kind == "defcomponent");
	CHECK(origins.at("chip-label").line == 3);
	REQUIRE(origins.contains("frame"));
	CHECK(origins.at("frame").definition == "framed"); // the macro whose template built it
	CHECK(origins.at("frame").kind == "defmacro");
	CHECK(origins.at("frame").line == 2);
	CHECK(origins.at("frame").column == 5);
	REQUIRE(origins.contains("inside"));
	CHECK(origins.at("inside").definition == "main"); // written in main, handed to the macro
	CHECK(origins.at("inside").line == 4);
}

TEST_CASE("lisp origins: a computed id is at its form, a binding is written out, a map or a function's node has none") {
	const lisp::NodeOrigins origins = originsOf(R"lisp(
(defscreen main
  (column#top
    ~(repeat [i state.rows] `(label :id ~(str "row" i)))
    (label :id "{{state.which}}")
    ~{:type "label" :id "mapped"}
    ~(list 'label :id (str "made"))))
)lisp");
	CHECK(origins.size() == 3);
	CHECK(origins.contains("top"));
	REQUIRE(origins.contains("{{state.which}}")); // the tree's id, as written
	CHECK(origins.at("{{state.which}}").line == 4);
	CHECK(origins.at("{{state.which}}").id == lisp::NodeOrigin::Id::Form);
	REQUIRE(origins.contains("row{{i}}")); // the repeat's template, which its copies fill in
	CHECK(origins.at("row{{i}}").id == lisp::NodeOrigin::Id::Expression);
	CHECK(origins.at("row{{i}}").line == 3);
	CHECK(origins.at("row{{i}}").column == 30);
}

TEST_CASE("lisp origins: an id a call hands to a view is where the call writes it") {
	const lisp::NodeOrigins origins = originsOf(R"lisp(
(defview tool [name] `(button :id ~name :text "x"))
(defscreen main
  (row#tools
    ~(tool "save")
    ~(tool "open")))
)lisp");
	REQUIRE(origins.contains("save"));
	CHECK(origins.at("save").definition == "main");
	CHECK(origins.at("save").id == lisp::NodeOrigin::Id::Call);
	CHECK(origins.at("save").line == 3);
	CHECK(origins.at("save").column == 12);
	REQUIRE(origins.contains("open"));
	CHECK(origins.at("open").line == 4);
	CHECK(origins.at("tools").id == lisp::NodeOrigin::Id::Form);
}

TEST_CASE("lisp origins: definitions of one file are told apart") {
	const lisp::NodeOrigins origins = originsOf(R"lisp(
(defview first-view []
  `(label#one :text "1"))
(defview second-view []
  `(label#two :text "2"))
(defscreen main (column ~(first-view) ~(second-view)))
)lisp");
	REQUIRE(origins.contains("one"));
	REQUIRE(origins.contains("two"));
	CHECK(origins.at("one").definition == "first-view");
	CHECK(origins.at("one").line == 2);
	CHECK(origins.at("two").definition == "second-view");
	CHECK(origins.at("two").line == 2);
	CHECK(origins.at("two").column == 4);
}

TEST_CASE("lisp origins: the recorded build makes the same tree, and an unbuildable section gives none") {
	const json document = lisp::sourceDocument(R"lisp(
(defview bar [] `(row#bar (button#go :text "Go")))
(defscreen main (column#page ~(bar)))
)lisp",
	                                           "app.lisp");
	lisp::Program program = lisp::programOf(document["program"]);
	program.recordOrigins(true);
	CHECK(program.build() == lisp::builtProgram(document["program"])->tree);
	CHECK(program.nodePlaces().size() == 3);
	program.recordOrigins(false);
	static_cast<void>(program.build());
	CHECK(program.nodePlaces().empty());
	CHECK(lisp::nodeOrigins(document["program"]) == lisp::nodeOrigins(document["program"])); // kept, built once

	const json broken = lisp::sourceDocument("(defscreen main (column#page ~(undefined-view)))", "app.lisp");
	CHECK(lisp::nodeOrigins(broken["program"])->empty());
	CHECK(lisp::nodeOrigins(json{{"app.lisp", 1}})->empty()); // not a section's shape
}

TEST_CASE("lisp origins: every example's written ids have their origin, at a form that writes that id") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int programs = 0;
	for(const auto& folder : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_regular_file(folder.path() / "lisp" / "app.lisp")) continue;
		CAPTURE(folder.path().filename().string());
		// As the host loads it: its entry screen, with the program files it includes
		affresco::Screen screen =
		    affresco::Screen::fromApp(folder.path() / "lisp", {}, affresco::Screen::History::InMemory);
		const json& document = screen.versions().sourceDocument();
		if(!document.contains("program")) continue;
		++programs;
		const json& section = document["program"];
		const lisp::NodeOrigins& origins = *lisp::nodeOrigins(section);
		for(const auto& [id, origin] : origins) {
			CAPTURE(id);
			CAPTURE(origin.definition);
			const std::string entry = section.at(origin.file).at(origin.definition).get<std::string>();
			const std::size_t at = offsetOf(entry, origin.line, origin.column);
			const std::vector<std::string> items = itemsOfList(entry, at);
			if(origin.id == lisp::NodeOrigin::Id::Call)
				CHECK(entry.substr(at).starts_with(json(id).dump())); // the call's argument: the id, in quotes
			else if(origin.id == lisp::NodeOrigin::Id::Expression) CHECK(computesId(items));
			else CHECK(writesId(items, id));
		}
		std::set<std::string> ids;
		collectNodeIds(lisp::builtProgram(section)->tree, ids);
		std::size_t found = 0;
		for(const std::string& id : ids) found += origins.contains(id) ? 1U : 0U;
		MESSAGE(folder.path().filename().string()
		        << ": " << found << " of " << ids.size() << " node ids have an origin");
		for(const std::string& id : ids)
			if(!origins.contains(id)) MESSAGE("  without one: " << id);
	}
	CHECK(programs > 0);
}
