/// @file
/// @brief The parts of a program's tree (TreeParts.hpp): what each definition gives it, and the tree they make.

#include "TreeParts.hpp"

#include "Definitions.hpp"
#include "Objects.hpp"

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

bool Section::give(const std::string& name, json value) {
	const auto [found, added] = index_.emplace(name, members_.size());
	if(added) {
		members_.emplace_back(name, std::move(value));
		return true;
	}
	if(defaulted_.erase(name) == 0) return false;
	members_[found->second].second = std::move(value);
	return true;
}

bool Section::giveDefault(const std::string& name, json value) {
	if(index_.contains(name)) return !defaulted_.contains(name);
	index_.emplace(name, members_.size());
	members_.emplace_back(name, std::move(value));
	defaulted_.insert(name);
	return true;
}

namespace {

/// @brief Adds a section to the tree, if it has anything; `defscreen main` may have given some of it.
/// @param tree The tree.
/// @param key The section's key.
/// @param section The section.
/// @param defaulted Gets each default left, `{section, member}`: one that `defscreen main` gives yields to it.
/// @throws Problem If `defscreen main` gave something other than an object there, or a member both give.
void addSection(json& tree, const std::string& key, Section& section,
                std::set<std::pair<std::string, std::string>>& defaulted) {
	if(section.empty()) return;
	json object = tree.contains(key) ? std::move(tree[key]) : json::object();
	if(!object.is_object()) {
		if(std::ranges::all_of(section, [&](const auto& member) { return section.isDefault(member.first); }))
			return; // only defaults, which the value there replaces
		throw Problem("defscreen main gives \"" + key + "\" as something other than a map");
	}
	for(auto& [name, value] : section) {
		if(section.isDefault(name)) {
			if(object.contains(name)) continue;
			defaulted.emplace(key, name);
		} else if(object.contains(name)) {
			throw Problem("\"" + name + "\" of \"" + key + "\" is given by defscreen main and by a definition");
		}
		object[name] = std::move(value);
	}
	tree[key] = std::move(object);
}

/// @brief A command's action: its options, its docstring as the description, and the step that runs its function.
/// @param options What it was given.
/// @param name Its name, its function's too.
/// @param doc Its docstring.
/// @param parameters Its function's parameters, what a caller by name gives.
/// @return The action.
json commandAction(json options, const std::string& name, const std::string& doc,
                   const std::vector<std::string>& parameters) {
	if(!doc.empty() && !options.contains("description")) options["description"] = doc;
	if(!options.contains("do")) options["do"] = json::array({{{"lisp", name}}});
	if(!parameters.empty() && !options.contains("arguments")) options["arguments"] = parameters;
	return options;
}

/// @brief A setting: its `type`, its `default`, then its other options, and its docstring as the description.
/// @param options What it was given.
/// @param value Its value.
/// @param doc Its docstring.
/// @return The setting.
json customSetting(const json& options, json value, const std::string& doc) {
	json setting = json::object();
	if(options.contains("type")) setting["type"] = options["type"];
	setting["default"] = std::move(value);
	for(const auto& [key, member] : options.items()) setting[key] = member;
	if(!doc.empty() && !setting.contains("description")) setting["description"] = doc;
	return setting;
}

/// @brief Adds what `defpart` gives that isn't a map to the tree: a list joins the one there, a value must be the same.
/// @param tree The tree.
/// @param values The values.
void addValues(json& tree, std::map<std::string, json>& values) {
	for(auto& [key, value] : values) {
		const auto found = tree.find(key);
		if(found == tree.end()) tree[key] = std::move(value);
		else if(found->is_array() && value.is_array()) found->insert(found->end(), value.begin(), value.end());
		else if(*found != value)
			throw Problem("\"" + notation::Lexer::excerpt(key) + "\" is given by defscreen main and by defpart");
	}
}

/// @brief Builds what `defpart` gives: each section's members, as a definition of each would.
/// @param machine The machine.
/// @param definition The definition.
/// @param data Its options.
/// @param gifts Takes them.
void buildPart(Machine& machine, const Definition& definition, const Value& data, Gifts& gifts) {
	json options = valueToTree(machine, data, definition.name);
	const bool defaults = definition.kind == "defdefaults";
	for(auto& [key, value] : options.items()) {
		if(defaults) {
			if(!value.is_object()) throw Problem("defdefaults gives members of sections: \"" + key + "\" isn't a map");
			for(auto& [member, given] : value.items()) gifts.fallback(key, member, given);
			continue;
		}
		if(value.is_object()) {
			for(auto& [member, given] : value.items()) gifts.member(key, "defpart", member, given);
			continue;
		}
		gifts.value(key, value);
	}
}

/// @brief Builds the part of a form that defines a member of a section (sectionOfForm()).
/// @param machine The machine.
/// @param definition The definition.
/// @param data What it built.
/// @param gifts Takes it.
void buildSectionMember(Machine& machine, const Definition& definition, const Value& data, Gifts& gifts) {
	const std::string& name = definition.name;
	const std::string member = name.substr(name.find('.') + 1);
	const std::string section(sectionOfForm(definition.kind));
	if(definition.kind == "defstate") {
		gifts.member("state", definition.kind, member, valueToTree(machine, data.at(0), name));
		json options = valueToTree(machine, data.at(1), name);
		if(options.contains("type")) gifts.member("stateTypes", definition.kind, member, options["type"]);
	} else if(definition.kind == "deftokens") {
		json tokens = valueToTree(machine, data, name);
		if(!tokens.is_object()) throw Problem("deftokens takes a map of tokens");
		for(auto& [token, value] : tokens.items()) gifts.member(section, definition.kind, token, std::move(value));
	} else {
		gifts.member(section, definition.kind, member, valueToTree(machine, data, name));
	}
}

} // namespace

void Gifts::member(const std::string& section, std::string_view form, const std::string& name, json value) {
	take({Gift::Kind::Member, section, name, std::string(form), std::move(value)});
}

void Gifts::fallback(const std::string& section, const std::string& name, json value) {
	take({Gift::Kind::Default, section, name, "defdefaults", std::move(value)});
}

void Gifts::value(const std::string& key, json value) {
	take({Gift::Kind::Value, "", key, "defpart", std::move(value)});
}

void Gifts::main(const std::string& key, json value) {
	take({Gift::Kind::Main, "", key, "defscreen", std::move(value)});
}

void Gifts::take(Gift gift) {
	if(parts_) giveAgain(gift, *parts_);
	if(kept_) kept_->push_back(std::move(gift));
}

void giveAgain(const Gift& gift, BuiltParts& parts) {
	switch(gift.kind) {
	case Gift::Kind::Member:
		if(!parts.sections[gift.section].give(gift.name, gift.value))
			throw Problem("\"" + notation::Lexer::excerpt(gift.name) + "\" is given twice, the second time by " +
			              gift.form);
		return;
	case Gift::Kind::Default:
		if(!parts.sections[gift.section].giveDefault(gift.name, gift.value))
			throw Problem("\"" + notation::Lexer::excerpt(gift.name) +
			              "\" is given twice, the second time by defdefaults");
		return;
	case Gift::Kind::Value: {
		const auto [found, added] = parts.values.try_emplace(gift.name, gift.value);
		if(added) return;
		if(found->second.is_array() && gift.value.is_array())
			found->second.insert(found->second.end(), gift.value.begin(), gift.value.end());
		else if(found->second != gift.value)
			throw Problem("\"" + notation::Lexer::excerpt(gift.name) + "\" is given twice, differently, by defpart");
		return;
	}
	case Gift::Kind::Main:
		parts.main[gift.name] = gift.value;
		return;
	}
}

json treeOf(BuiltParts& parts, std::set<std::pair<std::string, std::string>>& defaulted) {
	json tree = {{"format", "affrescoui-screen"}, {"version", 2}};
	for(auto& [key, member] : parts.main.items()) tree[key] = std::move(member);
	defaulted.clear();
	for(const char* key : {"screens", "actions", "keys", "settings"}) // the format's own first
		if(const auto found = parts.sections.find(key); found != parts.sections.end()) {
			addSection(tree, key, found->second, defaulted);
			parts.sections.erase(found);
		}
	for(auto& [key, section] : parts.sections) addSection(tree, key, section, defaulted);
	addValues(tree, parts.values);
	return tree;
}

void buildDefinition(Machine& machine, const Definition& definition, Gifts& gifts) {
	if(!definition.data) return;
	const Value data = machine.runThunk(definition.data);
	const std::string& name = definition.name;
	if(definition.kind == "defkey") {
		gifts.member("keys", definition.kind, name.substr(std::string_view("defkey ").size()),
		             valueToTree(machine, data, name));
	} else if(definition.kind == "defcommand") {
		gifts.member("actions", definition.kind, name,
		             commandAction(valueToTree(machine, data, name), name, definition.doc,
		                           Interpreter::parametersOf(definition.global->value)));
	} else if(definition.kind == "defcustom") {
		const Value value = machine.force(*definition.global);
		gifts.member(
		    "settings", definition.kind, name,
		    customSetting(valueToTree(machine, data, name), valueToTree(machine, value, name), definition.doc));
	} else if(!sectionOfForm(definition.kind).empty()) {
		buildSectionMember(machine, definition, data, gifts);
	} else if(definition.kind == "defpart" || definition.kind == "defdefaults") {
		buildPart(machine, definition, data, gifts);
	} else if(name == "main") {
		// A named tree: a loop over a temporary's items() reads it after it is gone
		json settings = valueToTree(machine, data.at(0), name);
		for(auto& [key, member] : settings.items()) gifts.main(key, std::move(member));
		gifts.main("root", valueToTree(machine, data.at(1), name));
	} else {
		json screen = valueToTree(machine, data.at(1), name);
		gifts.member("screens", definition.kind, name, std::move(screen));
	}
}

void checkExtensionFunction(const Machine& machine, const Definition& definition) {
	if(definition.kind == "remove-hook" || definition.targets.empty()) return;
	const Definition* function = machine.definition(definition.targets.back());
	if(!function || (function->kind != "defn" && function->kind != "defcommand"))
		throw Problem("\"" + definition.name + "\" names \"" + notation::Lexer::excerpt(definition.targets.back()) +
		              "\", which is no function of the program");
}

namespace {

/// @brief Adds what a piece of code uses by name, and what the functions inside it use.
/// @param code The code, or nullptr.
/// @param uses Gets them.
void addNamesUsed(const Code* code, Uses& uses) {
	if(!code) return;
	std::vector<const Code*> waiting = {code};
	while(!waiting.empty()) {
		const Code* next = waiting.back();
		waiting.pop_back();
		std::vector<bool> named(next->globals.size(), false);
		for(const NamedCall& call : next->named) {
			named[call.global] = true;
			uses.called[next->globals[call.global]->name].insert(call.text);
		}
		for(std::size_t at = 0; at < next->globals.size(); ++at) {
			uses.names.insert(next->globals[at]->name);
			if(!named[at]) uses.otherwise.insert(next->globals[at]->name);
		}
		waiting.insert(waiting.end(), next->functions.begin(), next->functions.end());
	}
}

} // namespace

Uses namesUsedBy(const Definition& definition) {
	Uses uses;
	addNamesUsed(definition.data, uses);
	if(const Global* global = definition.global) {
		addNamesUsed(global->thunk, uses);
		if(global->value.kind() == Kind::Function)
			addNamesUsed(static_cast<const ClosureObject*>(global->value.pointer())->code, uses);
	}
	return uses;
}

CallsByName namedCallsOf(const Uses& uses) {
	CallsByName named;
	for(const auto& [name, texts] : uses.called) {
		named.texts.emplace(name, std::vector<std::string>(texts.begin(), texts.end()));
		if(uses.otherwise.contains(name)) named.otherwise.insert(name);
	}
	return named;
}

} // namespace affresco::lisp
