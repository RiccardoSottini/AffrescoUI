/// @file
/// @brief format::Usage: counting, the bounds, the retention window and the file beside the "persist" state.

#include "affresco/format/Usage.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <tuple>

namespace affresco::format {

namespace {

/// @brief Whether a name may be counted: 1 to Usage::maxNameBytes printable ASCII bytes.
/// @param name The name.
/// @return true if so.
bool countable(std::string_view name) {
	return !name.empty() && name.size() <= Usage::maxNameBytes &&
	       std::ranges::all_of(name, [](char c) { return c >= ' ' && c < 127; });
}

/// @brief Adds one to a count, up to Usage::maxCount.
/// @param count The count.
void addOne(std::uint64_t& count) noexcept { count = std::min(count + 1, Usage::maxCount); }

/// @brief Collects the controls of a document: nodes with an "id" and a handler ("on…") or a "bind".
/// @param value A part of the document.
/// @param depth How deep it is.
/// @param into The ids, at most Usage::maxControls.
void collectControls(const json& value, int depth, std::set<std::string>& into) {
	if(depth > 64 || into.size() >= Usage::maxControls) return;
	if(value.is_array())
		for(const json& each : value) collectControls(each, depth + 1, into);
	if(!value.is_object()) return;
	const auto id = value.find("id");
	if(id != value.end() && id->is_string() && countable(id->get_ref<const std::string&>())) {
		const bool control = value.contains("bind") || std::ranges::any_of(value.items(), [](const auto& item) {
			                     return item.key().size() > 2 && item.key().starts_with("on") &&
								        std::isupper(static_cast<unsigned char>(item.key()[2]));
		                     });
		if(control) into.insert(id->get<std::string>());
	}
	for(const auto& item : value.items())
		if(item.value().is_structured()) collectControls(item.value(), depth + 1, into);
}

/// @brief A count as saved and summarised.
/// @param runs Times.
/// @param errors Of which failed.
/// @param first When first.
/// @param last When last.
/// @return `{"runs", "errors", "first", "last"}`.
json countJson(std::uint64_t runs, std::uint64_t errors, std::int64_t first, std::int64_t last) {
	return {{"runs", runs}, {"errors", errors}, {"first", first}, {"last", last}};
}

/// @brief A saved whole number.
/// @param saved The saved object.
/// @param key Its key.
/// @return It, or 0 if it is missing or not a whole number.
std::int64_t integerOf(const json& saved, const char* key) {
	const auto found = saved.find(key);
	return found != saved.end() && found->is_number_integer() ? found->get<std::int64_t>() : 0;
}

/// @brief A saved text.
/// @param saved The saved object.
/// @param key Its key.
/// @return It, or "" if it is missing or not a text.
std::string textOf(const json& saved, const char* key) {
	const auto found = saved.find(key);
	return found != saved.end() && found->is_string() ? found->get<std::string>() : std::string();
}

/// @brief A saved list.
/// @param saved The saved object.
/// @param key Its key.
/// @return It, or an empty list if it is missing or not a list.
const json& listOf(const json& saved, const char* key) {
	static const json none = json::array();
	const auto found = saved.find(key);
	return found != saved.end() && found->is_array() ? *found : none;
}

/// @brief Reads a saved count, bounded.
/// @param saved The saved entry.
/// @return Its runs, errors, first and last, clamped.
std::tuple<std::uint64_t, std::uint64_t, std::int64_t, std::int64_t> readCount(const json& saved) {
	const auto number = [&](const char* key) { return integerOf(saved, key); };
	const auto runs = static_cast<std::uint64_t>(std::clamp<std::int64_t>(number("runs"), 0, Usage::maxCount));
	const auto errors = std::min(runs, static_cast<std::uint64_t>(std::max<std::int64_t>(number("errors"), 0)));
	return {runs, errors, number("first"), number("last")};
}

} // namespace

Usage::~Usage() {
	try {
		save();
	} catch(...) { // NOLINT(bugprone-empty-catch): a destructor doesn't throw; save() logs what it can't write
	}
}

std::int64_t Usage::now() noexcept {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
	    .count();
}

void Usage::setFile(std::filesystem::path file) {
	file_ = std::move(file);
	std::error_code error;
	const auto size = std::filesystem::file_size(file_, error);
	if(error || size > maxFileBytes) return;
	const std::ifstream in(file_);
	std::stringstream text;
	text << in.rdbuf();
	try {
		load(parseUntrusted(text.str()));
	} catch(const std::exception& damaged) {
		log(LogLevel::Warning,
		    "AffrescoUI usage: " + file_.string() + " is unreadable and is started again: " + damaged.what());
	}
	expire(now());
}

void Usage::setEnabled(bool on) {
	setting_ = on;
	enabled_ = on && consented();
	if(enabled_ && since_ == 0) since_ = now();
	changed_ = true;
	save();
}

void Usage::giveConsent() {
	consent_ = now();
	enabled_ = setting_;
	if(enabled_ && since_ == 0) since_ = consent_;
	changed_ = true;
	save();
}

void Usage::ran(std::string_view node, std::string_view action, bool failed, std::int64_t time) {
	if(!enabled_ || !countable(node) || !countable(action)) return;
	auto found = actions_.find(std::pair(node, action));
	if(found == actions_.end()) {
		if(actions_.size() >= maxEntries) return;
		found = actions_.emplace(Key(node, action), Count{0, 0, time, time}).first;
	}
	addOne(found->second.runs);
	if(failed) addOne(found->second.errors);
	found->second.last = time;
	changed_ = true;
	saveSoon(time);
}

void Usage::visited(std::string_view screen, std::int64_t time) {
	if(!enabled_ || !countable(screen)) return;
	auto found = screens_.find(screen);
	if(found == screens_.end()) {
		if(screens_.size() >= maxScreens) return;
		found = screens_.emplace(std::string(screen), Count{0, 0, time, time}).first;
	}
	addOne(found->second.runs);
	found->second.last = time;
	changed_ = true;
	saveSoon(time);
}

json Usage::summary(const json& document, std::int64_t time) {
	expire(time);
	std::set<std::string> untouched;
	collectControls(document, 0, untouched);
	for(const auto& each : actions_) untouched.erase(each.first.first);
	json result = toJson();
	result.erase("consent");
	result["enabled"] = enabled_;
	result["consented"] = consented();
	result["retentionSeconds"] = retentionSeconds;
	result["untouched"] = untouched;
	return result;
}

void Usage::clear() {
	actions_.clear();
	screens_.clear();
	since_ = enabled_ ? now() : 0;
	changed_ = true;
	save();
}

void Usage::expire(std::int64_t time) {
	const auto old = [&](const auto& entry) { return entry.second.last < time - retentionSeconds; };
	changed_ = std::erase_if(actions_, old) + std::erase_if(screens_, old) > 0 || changed_;
	if(since_ != 0 && since_ < time - retentionSeconds) since_ = time - retentionSeconds;
}

void Usage::saveSoon(std::int64_t time) {
	if(time - written_ >= 60) save();
}

void Usage::save() {
	if(file_.empty() || !changed_) return;
	written_ = now();
	changed_ = false;
	const std::filesystem::path temporary = file_.string() + ".tmp";
	{
		std::ofstream out(temporary, std::ios::trunc);
		if(!out || !(out << toJson().dump(1, '\t') << '\n')) {
			log(LogLevel::Error, "AffrescoUI usage: could not write " + temporary.string());
			return;
		}
	}
	std::error_code error;
	std::filesystem::rename(temporary, file_, error);
	if(error) log(LogLevel::Error, "AffrescoUI usage: could not write " + file_.string() + ": " + error.message());
}

void Usage::load(const json& saved) {
	if(!saved.is_object()) return;
	consent_ = std::max<std::int64_t>(integerOf(saved, "consent"), 0);
	since_ = std::max<std::int64_t>(integerOf(saved, "since"), 0);
	for(const json& entry : listOf(saved, "actions")) {
		if(actions_.size() >= maxEntries || !entry.is_object()) break;
		const std::string node = textOf(entry, "node");
		const std::string action = textOf(entry, "action");
		if(!countable(node) || !countable(action)) continue;
		const auto [runs, errors, first, last] = readCount(entry);
		actions_[Key(node, action)] = {runs, errors, first, last};
	}
	for(const json& entry : listOf(saved, "screens")) {
		if(screens_.size() >= maxScreens || !entry.is_object()) break;
		const std::string screen = textOf(entry, "screen");
		if(!countable(screen)) continue;
		const auto [runs, errors, first, last] = readCount(entry);
		screens_[screen] = {runs, 0, first, last};
	}
}

json Usage::toJson() const {
	json actions = json::array();
	for(const auto& [key, count] : actions_) {
		json entry = countJson(count.runs, count.errors, count.first, count.last);
		entry["node"] = key.first;
		entry["action"] = key.second;
		actions.push_back(std::move(entry));
	}
	json screens = json::array();
	for(const auto& [name, count] : screens_) {
		json entry = countJson(count.runs, 0, count.first, count.last);
		entry["screen"] = name;
		screens.push_back(std::move(entry));
	}
	return {{"consent", consent_}, {"since", since_}, {"actions", actions}, {"screens", screens}};
}

} // namespace affresco::format
