/// @file
/// @brief Whose data a name in the data folder is (ScreenFiles.hpp, boundDataName()): an app's id, or a bare screen's
/// file name, is its own choice, so the name is bound to the place that first used it, and another place that names
/// itself the same gets data of its own. Screen::setDataAside() frees a place's names again, its data kept aside.

#include "ScreenFiles.hpp"

#include "affresco/Screen.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/core/Portable.hpp"
#include "affresco/core/Sha256.hpp"

#include <array>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace affresco::detail {

namespace {

constexpr std::size_t longestRecord = 64 * 1024; ///< A location record, at most 64 KiB.

/// @brief A place as its record keeps it: absolute, with its links followed, as UTF-8.
/// @param location The app's folder or the screen file.
/// @return The text.
std::string recordedText(const std::filesystem::path& location) {
	std::error_code error;
	std::filesystem::path canonical = std::filesystem::weakly_canonical(std::filesystem::absolute(location), error);
	if(error) canonical = std::filesystem::absolute(location).lexically_normal();
	const std::u8string text = canonical.u8string();
	return {text.begin(), text.end()};
}

/// @brief What a location record holds.
/// @param record The record.
/// @return Its text, or std::nullopt if there is none.
std::optional<std::string> readRecord(const std::filesystem::path& record) {
	std::ifstream file(record, std::ios::binary);
	if(!file) return std::nullopt;
	std::string text(longestRecord, '\0');
	file.read(text.data(), static_cast<std::streamsize>(text.size()));
	text.resize(static_cast<std::size_t>(file.gcount()));
	while(!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
	return text;
}

/// @brief Records a place as a name's.
/// @param record The record.
/// @param place recordedText() of the place.
void writeRecord(const std::filesystem::path& record, const std::string& place) {
	std::error_code error;
	std::filesystem::create_directories(record.parent_path(), error);
	std::ofstream file(record, std::ios::binary | std::ios::trunc);
	if(!error && file << place << '\n') return;
	affresco::log(LogLevel::Warning, "AffrescoUI screen: could not record whose data " + record.parent_path().string() +
	                                     " is (" + record.string() + ")");
}

} // namespace

std::string boundDataName(const std::function<std::filesystem::path(const std::string&)>& recordOf,
                          const std::string& name, const std::filesystem::path& location) {
	const std::string place = recordedText(location);
	const std::string own = name + "~" + sha256::hex(place).substr(0, 12);
	for(const std::string& candidate : {name, own}) {
		const std::filesystem::path record = recordOf(candidate);
		const std::optional<std::string> recorded = readRecord(record);
		if(recorded == place) return candidate;
		if(recorded) continue; // another place's
		// No record: a new name, or data kept before names were bound, which the first place to open it takes
		writeRecord(record, place);
		return candidate;
	}
	throw ScreenError("AffrescoUI screen: the data folder's " + own + " is recorded as another place's");
}

} // namespace affresco::detail

namespace affresco {

namespace {

/// @brief A path from the UTF-8 text a record holds.
/// @param text The text.
/// @return The path.
std::filesystem::path pathOfRecord(const std::string& text) { return {std::u8string(text.begin(), text.end())}; }

/// @brief Whether a record names a place, or a file inside it.
/// @param record The record.
/// @param place The place, as recordedText() gives it.
/// @return Whether it does; a record missing, or holding a relative path, doesn't.
bool namesPlace(const std::filesystem::path& record, const std::filesystem::path& place) {
	const std::optional<std::string> recorded = detail::readRecord(record);
	if(!recorded) return false;
	const std::filesystem::path inside = pathOfRecord(*recorded).lexically_relative(place); // "." for the place
	return !inside.empty() && *inside.begin() != "..";
}

/// @brief The current time, for a file name, as the history's own (HistoryStore::moveAside()).
/// @return UTC, e.g. "20261019T120000Z" (no ':', which Windows doesn't allow in names).
std::string fileNameTime() {
	const std::tm utc = utcTime(std::time(nullptr));
	std::array<char, 32> stamp{};
	std::strftime(stamp.data(), stamp.size(), "%Y%m%dT%H%M%SZ", &utc);
	return stamp.data();
}

/// @brief What is set aside, and what couldn't be.
struct SettingAside {
	std::string stamp;                       ///< fileNameTime(), the same for every one.
	std::vector<std::filesystem::path> made; ///< Where each one set aside is now.
	std::string failed;                      ///< The first one that couldn't be moved, and why; empty if none.

	/// @brief Renames a file or folder `<it>.replaced-<stamp>` (with "-2", "-3"... if that is taken).
	/// @param entry It; nothing is done if it isn't there.
	/// @return false if it is there and couldn't be moved.
	bool move(const std::filesystem::path& entry) {
		std::error_code error;
		if(!std::filesystem::exists(std::filesystem::symlink_status(entry, error))) return true;
		std::filesystem::path aside = entry;
		aside += ".replaced-" + stamp;
		for(int n = 2; std::filesystem::exists(std::filesystem::symlink_status(aside, error)); ++n) {
			aside = entry;
			aside += ".replaced-" + stamp + "-" + std::to_string(n);
		}
		std::filesystem::rename(entry, aside, error);
		if(error) {
			affresco::log(LogLevel::Warning,
			              "AffrescoUI screen: " + entry.string() + " could not be set aside (" + error.message() + ")");
			if(failed.empty()) failed = entry.string() + " (" + error.message() + ")";
			return false;
		}
		affresco::log(LogLevel::Info, "AffrescoUI screen: " + entry.string() + " set aside as " + aside.string());
		made.push_back(std::move(aside));
		return true;
	}
};

constexpr std::string_view screenRecord = ".location"; ///< A bare screen's record: "<name>.location".

/// @brief The bare screens whose data is a place's: each "<name>.location" in the data folder that names it.
/// @param dataDirectory The data folder.
/// @param place The place, as recordedText() gives it.
/// @return Their names ("habits", "habits~<hash>").
std::vector<std::string> screensOf(const std::filesystem::path& dataDirectory, const std::filesystem::path& place) {
	std::vector<std::string> screens;
	std::error_code error, ignored;
	for(std::filesystem::directory_iterator entry(dataDirectory, error), end; !error && entry != end;
	    entry.increment(error)) {
		const std::string name = entry->path().filename().string();
		if(name.size() > screenRecord.size() && name.ends_with(screenRecord) && !entry->is_symlink(ignored) &&
		   entry->is_regular_file(ignored) && namesPlace(entry->path(), place))
			screens.push_back(name.substr(0, name.size() - screenRecord.size()));
	}
	return screens;
}

/// @brief The apps' data folders that are a place's: each in `apps/` whose "location" names it.
/// @param dataDirectory The data folder.
/// @param place The place, as recordedText() gives it.
/// @return Them.
std::vector<std::filesystem::path> appsOf(const std::filesystem::path& dataDirectory,
                                          const std::filesystem::path& place) {
	std::vector<std::filesystem::path> apps;
	std::error_code error, ignored;
	for(std::filesystem::directory_iterator entry(dataDirectory / "apps", error), end; !error && entry != end;
	    entry.increment(error)) {
		// One set aside before keeps its record, and is never set aside again
		if(!entry->path().filename().string().contains(".replaced-") && !entry->is_symlink(ignored) &&
		   entry->is_directory(ignored) && namesPlace(entry->path() / "location", place))
			apps.push_back(entry->path());
	}
	return apps;
}

} // namespace

std::vector<std::filesystem::path> Screen::setDataAside(const std::filesystem::path& dataDirectory,
                                                        const std::filesystem::path& location) {
	const std::filesystem::path place = pathOfRecord(detail::recordedText(location));
	if(place.relative_path().empty())
		throw ScreenError("AffrescoUI screen: the data of " + place.string() +
		                  ", a file system's root, isn't set aside");
	// The records first, then the moves: the folders are never changed while they are read
	const std::vector<std::string> screens = screensOf(dataDirectory, place);
	const std::vector<std::filesystem::path> apps = appsOf(dataDirectory, place);
	SettingAside aside{fileNameTime(), {}, {}};
	for(const std::filesystem::path& app : apps) aside.move(app); // its record goes with it
	for(const std::string& name : screens) {
		// and the persisted values a start found no longer fitting (Impl::keepRejected())
		bool moved = aside.move(dataDirectory / (name + ".state.rejected.json"));
		for(const std::string_view suffix : detail::screenDataSuffixes)
			moved = aside.move(dataDirectory / (name + std::string(suffix))) && moved;
		if(moved) aside.move(dataDirectory / (name + std::string(screenRecord))); // last: the name is then free
	}
	if(!aside.failed.empty()) throw ScreenError("AffrescoUI screen: could not set aside " + aside.failed);
	return std::move(aside.made);
}

} // namespace affresco
