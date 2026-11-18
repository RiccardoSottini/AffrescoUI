/// @file
/// @brief The parts of a program's tree (Program::build()): what each definition gives it, each section's members in
/// the order of their definitions. A definition's gifts are kept (Gift), so a build in part (Program::rebuild()) puts
/// together the same tree from what the definitions it didn't run gave the last time.
#pragma once

#include "Machine.hpp"

#include <affresco/lisp/Program.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::lisp {

/// @brief A section of the tree: its members in the order their definitions come in the program, as a JSON file's are
/// in the order written (an overlay's place, the first key of a command, the order of watches), so a change to one
/// definition never moves another. A `defdefaults` member takes its place where its definition is, and a definition
/// that gives it later replaces its value there, as a JSON file's include gives a member its place and the file its
/// value.
class Section {
public:
	using Member = std::pair<std::string, json>; ///< A member: its name and value.

	/// @brief Gives a member: added last, or replacing a default of that name where it is.
	/// @param name The name.
	/// @param value The value.
	/// @return false if a definition gave it already.
	bool give(const std::string& name, json value);
	/// @brief Gives a default: added last, unless a member of that name is there already, which it yields to.
	/// @param name The name.
	/// @param value The value.
	/// @return false if a default gave it already.
	bool giveDefault(const std::string& name, json value);
	/// @brief Whether a member is a default no definition replaced.
	/// @param name The name.
	/// @return true if so.
	[[nodiscard]] bool isDefault(const std::string& name) const { return defaulted_.contains(name); }
	/// @brief Whether it has no member.
	/// @return true if so.
	[[nodiscard]] bool empty() const noexcept { return members_.empty(); }
	/// @brief The first member.
	/// @return An iterator.
	[[nodiscard]] std::vector<Member>::iterator begin() noexcept { return members_.begin(); }
	/// @brief Past the last member.
	/// @return An iterator.
	[[nodiscard]] std::vector<Member>::iterator end() noexcept { return members_.end(); }

private:
	std::vector<Member> members_;              ///< In order.
	std::map<std::string, std::size_t> index_; ///< Each member's place.
	std::set<std::string> defaulted_;          ///< The members a default gave, that nothing has replaced.
};

/// @brief What a build gathers: the tree's parts, each in its order.
struct BuiltParts {
	json main = json::object(); ///< `defscreen main`'s properties and `root`.
	/// Each section's members (`screens`, `actions`, `keys`, `settings`, and what `defstate`, `defcomputed`,
	/// `defpart`, `defdefaults`... add to), each in the order of its definitions.
	std::map<std::string, Section> sections;
	std::map<std::string, json> values; ///< What `defpart` gives that isn't a map: a list joins, a value is given once.
};

/// @brief One thing a definition gives the tree, as its build gave it.
struct Gift {
	/// @brief What it gives.
	enum class Kind : std::uint8_t {
		Member,  ///< A section's member, which no other definition may give.
		Default, ///< A section's member from `defdefaults`, which a definition that gives it replaces.
		Value,   ///< What `defpart` gives that isn't a map: a list joins, a value is given once.
		Main,    ///< A property of `defscreen main`, `root` among them.
	};
	Kind kind = Kind::Member; ///< What it gives.
	std::string section;      ///< Its section; "" for a Value and Main.
	std::string name;         ///< The member's name, or the key.
	std::string form;         ///< The form that gives it, for errors (`defstate`, `defpart`...).
	json value;               ///< Its value.

	/// @brief Compares two. @param other The other. @return true if they give the same.
	[[nodiscard]] bool operator==(const Gift& other) const = default;
};

/// @brief Takes what a definition's build gives: puts it into the parts, keeps it, or both.
class Gifts {
public:
	/// @brief Takes them.
	/// @param parts The parts to put them into, or nullptr.
	/// @param kept Gets a copy of each, or nullptr.
	Gifts(BuiltParts* parts, std::vector<Gift>* kept) : parts_(parts), kept_(kept) {}

	/// @brief Gives a section's member: one given twice, as `deftokens` can, is an error naming it.
	/// @param section The section.
	/// @param form The form that gives it.
	/// @param name The member.
	/// @param value Its value.
	/// @throws Problem If the section has it already.
	void member(const std::string& section, std::string_view form, const std::string& name, json value);
	/// @brief Gives a section's member from `defdefaults`.
	/// @param section The section.
	/// @param name The member.
	/// @param value Its value.
	/// @throws Problem If a default gave it already.
	void fallback(const std::string& section, const std::string& name, json value);
	/// @brief Gives what `defpart` gives that isn't a map.
	/// @param key Its key.
	/// @param value Its value.
	/// @throws Problem If it was given differently, and isn't a list.
	void value(const std::string& key, json value);
	/// @brief Gives a property of `defscreen main`.
	/// @param key Its key.
	/// @param value Its value.
	void main(const std::string& key, json value);

private:
	/// @brief Puts a gift into the parts, if there are any, and keeps it, if asked.
	/// @param gift The gift.
	void take(Gift gift);

	BuiltParts* parts_;       ///< See the constructor.
	std::vector<Gift>* kept_; ///< See the constructor.
};

/// @brief Puts a gift into the parts, as the build that gave it did.
/// @param gift The gift.
/// @param parts The parts.
/// @throws Problem If a member is given twice, as the build would say.
void giveAgain(const Gift& gift, BuiltParts& parts);

/// @brief Puts the gathered parts together into the document's tree: the format's own sections first.
/// @param parts The parts; their members are taken.
/// @param defaulted Gets each section's member that is a default (Program::defaulted()); the state's keys a `defstate`
/// starts are the caller's to add.
/// @return The tree.
/// @throws Problem If `defscreen main` and a definition give the same.
[[nodiscard]] json treeOf(BuiltParts& parts, std::set<std::pair<std::string, std::string>>& defaulted);

/// @brief Builds one definition's part of the tree, its code already worked out.
/// @param machine The machine.
/// @param definition The definition.
/// @param gifts Takes what it gives.
/// @throws Problem If it can't be built.
void buildDefinition(Machine& machine, const Definition& definition, Gifts& gifts);

/// @brief Refuses a hook or advice whose function the program doesn't define: a host's primitive is no hook, so a step
/// can't be run this way, and neither can what isn't there yet.
/// @param machine The machine.
/// @param definition The hook or advice.
/// @throws Problem If it isn't a function of the program.
void checkExtensionFunction(const Machine& machine, const Definition& definition);

/// @brief What a definition's code uses by name, and how.
struct Uses {
	std::set<std::string> names;                         ///< Every name.
	std::map<std::string, std::set<std::string>> called; ///< Those called with a text written out first: the texts.
	std::set<std::string> otherwise;                     ///< Those used any other way.
};

/// @brief What a definition's code uses by name, macros' output and the functions inside it included.
/// @param definition The definition.
/// @return The names, and how.
[[nodiscard]] Uses namesUsedBy(const Definition& definition);

/// @brief The names a definition calls with a text written out first, those texts, and which it also uses otherwise.
/// @param uses What it uses.
/// @return Them.
[[nodiscard]] CallsByName namedCallsOf(const Uses& uses);

} // namespace affresco::lisp
