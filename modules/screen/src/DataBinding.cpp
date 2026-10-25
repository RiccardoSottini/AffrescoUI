/// @file
/// @brief Whose data a name in the data folder is (ScreenFiles.hpp, boundDataName()): an app's id, or a bare screen's
/// file name, is its own choice, so the name is bound to the place that first used it, and another place that names
/// itself the same gets data of its own.

#include "ScreenFiles.hpp"

#include "affresco/Screen.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/core/Sha256.hpp"

#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

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
