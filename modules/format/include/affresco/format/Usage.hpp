#pragma once

/// @file
/// @brief format::Usage: usage summaries (VERSION_CONTROL.md, "Usage"), off by default: how often each control's
/// actions ran and failed, the screens visited and the controls never touched, kept on the device. Only node ids,
/// action names, counts and times, never what anyone typed or saw; bounded.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace affresco::format {

/// @brief Counts per node id and action, and per screen, over a retention window.
///
/// Invariants: nothing is counted while it is off; at most maxEntries actions and maxScreens screens, names of at most
/// maxNameBytes printable bytes, counts at most maxCount; what is older than the retention window is dropped. Kept in
/// a file beside the "persist" state, with the one-time consent, and written when it ends, when cleared or switched,
/// and at most once a minute while counting. Main thread only.
class Usage {
public:
	static constexpr std::size_t maxEntries = 1024;           ///< Node and action pairs counted.
	static constexpr std::size_t maxScreens = 64;             ///< Screens counted.
	static constexpr std::size_t maxNameBytes = 64;           ///< The longest id, action or screen name.
	static constexpr std::size_t maxControls = 1024;          ///< Controls listed as never touched.
	static constexpr std::uint64_t maxCount = 1'000'000'000;  ///< A count stops there.
	static constexpr std::int64_t retentionSeconds = 2592000; ///< 30 days: older counts are dropped.
	static constexpr std::uintmax_t maxFileBytes = 1 << 20;   ///< A larger file isn't read.

	Usage() = default;
	/// @brief Writes what changed since the last write.
	~Usage();
	Usage(const Usage&) = delete;            ///< Not copied: it owns its file.
	Usage& operator=(const Usage&) = delete; ///< Not copied.

	/// @brief Seconds since 1970 on the system clock.
	/// @return Now.
	[[nodiscard]] static std::int64_t now() noexcept;

	/// @brief Sets the file it is kept in and loads it (the counts and the consent), dropping what is malformed,
	/// over the bounds or older than the window.
	/// @param file The file, e.g. `<data folder>/kanban.usage.json`.
	void setFile(std::filesystem::path file);
	/// @brief Whether it counts.
	/// @return true if the host's setting is on and the person consented.
	[[nodiscard]] bool enabled() const noexcept { return enabled_; }
	/// @brief Switches counting; it is on only once the person consented (giveConsent()).
	/// @param on The host's setting.
	void setEnabled(bool on);
	/// @brief Whether the person consented, once, on this device.
	/// @return true if so.
	[[nodiscard]] bool consented() const noexcept { return consent_ != 0; }
	/// @brief Records the person's consent, and writes it.
	void giveConsent();

	/// @brief Counts an action a control ran (nothing while off, or for a name out of bounds).
	/// @param node The node's "id" as written in the document.
	/// @param action The handler and its action, e.g. "onClick" or "onClick:addCard".
	/// @param failed Whether it reported an error.
	/// @param time When (now()).
	void ran(std::string_view node, std::string_view action, bool failed, std::int64_t time);
	/// @brief Counts a visit to a screen (nothing while off).
	/// @param screen The screen's name.
	/// @param time When (now()).
	void visited(std::string_view screen, std::int64_t time);
	/// @brief The summary: actions run and failed per node and action, screens visited, and the document's controls
	/// (nodes with an id and a handler or a "bind") never touched, within the window.
	/// @param document The document the controls are read from.
	/// @param time Now.
	/// @return `{"enabled", "consented", "since", "retentionSeconds", "actions", "screens", "untouched"}`.
	[[nodiscard]] json summary(const json& document, std::int64_t time);
	/// @brief Forgets every count (the consent stays), and writes that.
	void clear();
	/// @brief Writes the file now, if anything changed (no file: nothing).
	void save();

private:
	/// @brief A count and when it was first and last made.
	struct Count {
		std::uint64_t runs = 0;   ///< Times.
		std::uint64_t errors = 0; ///< Of which failed.
		std::int64_t first = 0;   ///< First, in seconds since 1970.
		std::int64_t last = 0;    ///< Last.
	};
	using Key = std::pair<std::string, std::string>; ///< A node id and an action.

	/// @brief Drops the counts older than the window.
	/// @param time Now.
	void expire(std::int64_t time);
	/// @brief Writes at most once a minute while counting.
	/// @param time Now.
	void saveSoon(std::int64_t time);
	/// @brief Reads a saved file's contents, bounded.
	/// @param saved The parsed file.
	void load(const json& saved);
	/// @brief What the file holds.
	/// @return The counts and the consent.
	[[nodiscard]] json toJson() const;

	std::filesystem::path file_;                        ///< Where it is kept (empty: nowhere).
	std::map<Key, Count, std::less<>> actions_;         ///< Per node id and action.
	std::map<std::string, Count, std::less<>> screens_; ///< Per screen.
	std::int64_t since_ = 0;                            ///< When counting started, or was last cleared.
	std::int64_t consent_ = 0;                          ///< When the person consented (0: not).
	std::int64_t written_ = 0;                          ///< When it was last written.
	bool setting_ = false;                              ///< The host's setting.
	bool enabled_ = false;                              ///< setting_ and consented.
	bool changed_ = false;                              ///< Something to write.
};

} // namespace affresco::format
