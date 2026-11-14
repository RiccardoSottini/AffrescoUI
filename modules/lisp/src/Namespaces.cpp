/// @file
/// @brief The namespaces (Namespaces.hpp), and how the machine resolves a qualified word (Machine::resolveQualified()).

#include "Namespaces.hpp"

#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <initializer_list>
#include <utility>

namespace affresco::lisp {

Namespaces::Namespaces()
    : spaces_{{"clojure.core", true}, {"clojure.set", false}, {"clojure.string", false}},
      aliases_{{"set", "clojure.set"}, {"str", "clojure.string"}, {"string", "clojure.string"}} {}

void Namespaces::add(std::string name, bool bare) { spaces_.insert_or_assign(std::move(name), bare); }

bool Namespaces::alias(std::string alias, std::string_view name) {
	if(!spaces_.contains(name)) return false;
	aliases_.insert_or_assign(std::move(alias), std::string(name));
	return true;
}

NamespaceFound Namespaces::find(std::string_view prefix, const AliasMap* aliases) const {
	std::string_view name = prefix;
	for(const AliasMap* table : {aliases, &aliases_}) { // a file's own first
		if(!table) continue;
		if(const auto alias = table->find(prefix); alias != table->end()) {
			name = alias->second;
			break;
		}
	}
	const auto found = spaces_.find(name);
	return found == spaces_.end() ? NamespaceFound{} : NamespaceFound{found->first, found->second};
}

std::optional<std::string_view> Machine::resolveQualified(std::string_view word, const AliasMap* aliases) {
	if(!isQualified(word)) return std::nullopt;
	const std::size_t slash = word.find('/');
	const std::string_view prefix = word.substr(0, slash);
	const NamespaceFound space = namespaces_.find(prefix, aliases);
	if(space.name.empty())
		throw Problem("\"" + notation::Lexer::excerpt(word) + "\": no namespace \"" + notation::Lexer::excerpt(prefix) +
		              "\" here; the language has clojure.core, clojure.string (str/) and clojure.set (set/)");
	const std::string_view name = word.substr(slash + 1);
	if(space.bare) return global(name).name;
	return global(std::string(space.name) + "/" + std::string(name)).name;
}

} // namespace affresco::lisp
