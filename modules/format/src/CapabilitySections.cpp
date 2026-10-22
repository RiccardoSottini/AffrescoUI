/// @file
/// @brief The sections that grant capabilities, which revisions can never change: "channels", "services", "persist"
/// and "permissions".

#include "SectionRegistry.hpp"

#include "affresco/core/DocumentRules.hpp"

#include <algorithm>
#include <format>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief "channels": each a socket, and whether to reconnect.
/// @param value The value.
/// @param name "channels".
void checkChannels(const json& value, const std::string& name) {
	for(const auto& [channel, definition] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, channel);
		expect::object(definition, at);
		if(!definition.contains("socket")) expect::fail(at, "a channel needs a \"socket\"");
		for(const auto& [key, item] : definition.items())
			if(key != "socket" && key != "reconnect" && key != "note")
				expect::fail(std::format("{}.{}", at, key), "unknown property (expected socket or reconnect)");
	}
}

/// @brief Fails if a value holds an expression anywhere: a capability's configuration is literal.
/// @param value The value.
/// @param at Its place.
void checkLiteral(const json& value, const std::string& at) {
	if(value.is_string() && value.get_ref<const std::string&>().contains("{{"))
		expect::fail(at, "a service's configuration is literal: no {{ }}");
	if(value.is_object())
		for(const auto& [key, item] : value.items()) checkLiteral(item, at + "." + key);
	if(value.is_array())
		for(std::size_t i = 0; i < value.size(); ++i) checkLiteral(value[i], at + "[" + std::to_string(i) + "]");
}

/// @brief "services": each a provider's name and its configuration, literal. The host's services check the rest
/// when the screen loads (Services::load), within what it grants.
/// @param value The value.
/// @param name "services".
void checkServices(const json& value, const std::string& name) {
	for(const auto& [service, definition] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, service);
		if(service.empty() || service.contains('.') || service.contains(':'))
			expect::fail(at, "a service's name has no \".\" or \":\"");
		expect::object(definition, at);
		if(!definition.contains("provider") || !definition["provider"].is_string())
			expect::fail(at, "a service needs a \"provider\", e.g. \"jsonrpc\", \"process\" or \"fs\"");
		checkLiteral(definition, at);
	}
}

/// @brief "persist": a list of paths under state.
/// @param value The value.
/// @param name "persist".
void checkPersist(const json& value, const std::string& name) {
	if(!value.is_array()) expect::fail(name, "expected a list of state paths, e.g. [\"state.habits\"]");
	for(std::size_t i = 0; i < value.size(); ++i) {
		const std::string at = name + "[" + std::to_string(i) + "]";
		if(!expect::string(value[i], at).starts_with("state."))
			expect::fail(at, "expected a path under state, e.g. \"state.habits\"");
	}
}

/// @brief Tells whether a text is a duration: a whole number of minutes, hours or days ("30m", "8h", "7d").
/// @param text The text.
/// @return true if it is one.
bool isDuration(const std::string& text) {
	return text.size() >= 2 && text.size() <= 7 && text[0] >= '1' && text[0] <= '9' &&
	       std::string_view("mhd").find(text.back()) != std::string_view::npos &&
	       std::ranges::all_of(text.substr(0, text.size() - 1), [](char c) { return c >= '0' && c <= '9'; });
}

/// @brief "permissions.review": the review policy (VERSION_CONTROL.md, "Review policies").
/// @param value The value.
/// @param name "permissions.review".
void checkReview(const json& value, const std::string& name) {
	for(const auto& [key, item] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, key);
		if(key == "approvals" || key == "behaviourApprovals") {
			if(!item.is_number_integer() || item.get<long long>() < 1 || item.get<long long>() > 20)
				expect::fail(at, "expected a number of approvals from 1 to 20");
		} else if(key == "grantsLast") {
			if(!isDuration(expect::string(item, at))) expect::fail(at, "expected a duration, e.g. \"8h\" or \"7d\"");
		} else if(key == "owners") {
			for(const auto& [pattern, owners] : expect::object(item, at).items()) {
				const std::string where = at + "." + pattern;
				if(!parsePermissionRule("*(" + pattern + ")"))
					expect::fail(where, "expected a JSON Pointer or \"#id\" path, e.g. \"#board\"");
				if(!owners.is_array() || owners.empty()) expect::fail(where, "expected a list of author names");
				for(std::size_t i = 0; i < owners.size(); ++i)
					(void)expect::string(owners[i], where + "[" + std::to_string(i) + "]");
			}
		} else expect::fail(at, "unknown property (expected approvals, behaviourApprovals, owners or grantsLast)");
	}
}

/// @brief Checks the author of a budget or a data rule: "*", a channel, an author or "role:<name>".
/// @param item The budget or rule.
/// @param at Where it is.
void checkPolicyAuthor(const json& item, const std::string& at) {
	if(!item.contains("author") || !item["author"].is_string() ||
	   item["author"].get<std::string>().find('(') != std::string::npos ||
	   !parsePermissionRule(item["author"].get<std::string>()))
		expect::fail(at + ".author", "expected an author, a channel, \"*\" or \"role:<name>\", e.g. \"agent\"");
}

/// @brief "permissions.budgets": the most nodes an author adds, removes and changes in one revision
/// (VERSION_CONTROL.md, "Budgets").
/// @param value The value.
/// @param name "permissions.budgets".
void checkBudgets(const json& value, const std::string& name) {
	if(!value.is_array() || value.size() > 64) expect::fail(name, "expected a list of at most 64 budgets");
	for(std::size_t i = 0; i < value.size(); ++i) {
		const std::string at = name + "[" + std::to_string(i) + "]";
		bool limited = false;
		for(const auto& [key, item] : expect::object(value[i], at).items()) {
			const std::string where = std::format("{}.{}", at, key);
			if(key == "added" || key == "removed" || key == "changed") {
				if(!item.is_number_integer() || item.get<long long>() < 0 || item.get<long long>() > 100000)
					expect::fail(where, "expected a number of nodes from 0 to 100000");
				limited = true;
			} else if(key == "over") {
				if(item != "ask" && item != "deny") expect::fail(where, "expected \"ask\" or \"deny\"");
			} else if(key != "author")
				expect::fail(where, "unknown property (expected author, added, removed, changed or over)");
		}
		checkPolicyAuthor(value[i], at);
		if(!limited) expect::fail(at, "expected at least one of added, removed or changed");
	}
}

/// @brief "permissions.data": the paths an author may read, write or never touch (VERSION_CONTROL.md, "Data rules").
/// @param value The value.
/// @param name "permissions.data".
void checkDataRules(const json& value, const std::string& name) {
	if(!value.is_array() || value.size() > 64) expect::fail(name, "expected a list of at most 64 data rules");
	for(std::size_t i = 0; i < value.size(); ++i) {
		const std::string at = name + "[" + std::to_string(i) + "]";
		for(const auto& [key, paths] : expect::object(value[i], at).items()) {
			const std::string where = std::format("{}.{}", at, key);
			if(key == "author") continue;
			if(key != "read" && key != "write" && key != "never")
				expect::fail(where, "unknown property (expected author, read, write or never)");
			if(!paths.is_array() || paths.size() > 64) expect::fail(where, "expected a list of at most 64 paths");
			for(std::size_t j = 0; j < paths.size(); ++j) {
				const std::string path = expect::string(paths[j], where + "[" + std::to_string(j) + "]");
				if(!path.starts_with('/') || path.size() > 256 || !parsePermissionRule("*(" + path + ")"))
					expect::fail(where + "[" + std::to_string(j) + "]",
					             "expected a JSON Pointer, e.g. \"/state/cards\"");
			}
		}
		checkPolicyAuthor(value[i], at);
	}
}

/// @brief "permissions": allow, ask and deny rules, the reviewers and the review policy.
/// @param value The value.
/// @param name "permissions".
void checkPermissions(const json& value, const std::string& name) {
	for(const auto& [key, list] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, key);
		if(key == "review" || key == "budgets" || key == "data") {
			(key == "review" ? checkReview : key == "budgets" ? checkBudgets : checkDataRules)(list, at);
			continue;
		}
		if(key != "allow" && key != "ask" && key != "deny" && key != "reviewers")
			expect::fail(at, "unknown property (expected allow, ask, deny, reviewers, review, budgets or data)");
		if(!list.is_array())
			expect::fail(at, key == "reviewers" ? "expected a list of author names" : "expected a list of rules");
		for(std::size_t i = 0; i < list.size(); ++i) {
			const std::string item = expect::string(list[i], at + "[" + std::to_string(i) + "]");
			if(key != "reviewers" && !parsePermissionRule(item))
				expect::fail(at + "[" + std::to_string(i) + "]",
				             "expected \"author\" or \"author(pattern)\", the pattern a JSON Pointer or \"#id\" path, "
				             "e.g. \"agent(#board)\"");
		}
	}
}

} // namespace

SectionType channelsSection() { return {.name = "channels", .validate = checkChannels, .capability = true}; }

SectionType servicesSection() { return {.name = "services", .validate = checkServices, .capability = true}; }

SectionType persistSection() { return {.name = "persist", .validate = checkPersist, .capability = true}; }

SectionType permissionsSection() { return {.name = "permissions", .validate = checkPermissions, .capability = true}; }

} // namespace affresco::format
