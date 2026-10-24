#pragma once

/// @file
/// @brief format::DataLayer: the runtime's side of the data layer: the store the screen's records live in, kept in
/// step with the document's "models" as versions land, the `data` variable (each collection's records and each
/// declared query's results), and who the write steps write as.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/data/Store.hpp"

#include "Reads.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

struct Engine;

/// Where the records lie among the places values read (Reads.hpp): a change of the store marks "/~d", so an update
/// evaluates again only what reads `data`. "~d" is no key of a JSON Pointer, so it is never a place of the state.
inline constexpr std::string_view dataPlaces = "/~d";

/// @brief The runtime's data layer.
///
/// The store is the host's (setStore(), a file per app) or, until it gives one, an in-memory store of the layer's
/// own, so every screen has one. While a proposal is previewed, the screen reads and writes a copy of the store that
/// follows the proposal's models, dropped when the preview ends: the store itself only follows main.
/// Invariants: main thread only; the `data` value stays valid until the next state or store change.
class DataLayer {
public:
	/// @brief An in-memory store following the document's models.
	/// @param engine The runtime; outlives the layer.
	/// @throws DocumentError If the document's models or seeds are invalid.
	explicit DataLayer(Engine& engine);
	~DataLayer();
	DataLayer(const DataLayer&) = delete;
	DataLayer& operator=(const DataLayer&) = delete;
	DataLayer(DataLayer&&) = delete;
	DataLayer& operator=(DataLayer&&) = delete;

	/// @brief Uses the host's store from now on (it then follows the document's models), or an own in-memory one.
	/// @param store The store, which outlives the runtime or is replaced first; nullptr: an own in-memory store.
	/// @param oldState A state an earlier version of the app kept on disk, for the models' importFrom (or nullptr).
	/// @throws DocumentError If the store can't follow the models.
	void setStore(data::Store* store, const json* oldState = nullptr);

	/// @brief The store screens read and write now: the preview's copy while one is shown.
	/// @return It.
	[[nodiscard]] data::Store& store() noexcept;

	/// @brief The store of main (never the preview's copy).
	/// @return It.
	[[nodiscard]] data::Store& mainStore() noexcept { return *store_; }

	/// @brief A version landed: the store follows its models (main), or a copy does (a preview).
	/// @throws DocumentError To refuse the version: the records don't fit its models.
	void documentChanged();

	/// @brief The `data` variable: `{collection: [records], query: [records] or count}`.
	/// @return It, built if the state or the store changed since.
	[[nodiscard]] const json* variable();

	/// @brief The places of the state the declared queries read (their "where", "limit", ...): what reads `data`
	/// reads them too, so it is evaluated again when a query's results may change.
	/// @return They, as of the last variable().
	[[nodiscard]] const ReadSet& queryReads() const noexcept { return queryReads_; }

	/// @brief What agents see: each collection's version, fields and count, and the declared queries.
	/// @return `{"collections": {...}, "queries": [...]}`.
	[[nodiscard]] json outline() const;

	/// @brief Who the write steps write as: "app" (the person using the app) unless the host says otherwise.
	/// @return The author.
	[[nodiscard]] const std::string& author() const noexcept { return author_; }

	/// @brief Sets who the write steps write as, e.g. an agent's name while the host runs its reviewed steps; the
	/// screen's data rules then apply to its writes.
	/// @param author The author ("" for "app").
	void setAuthor(std::string author) { author_ = author.empty() ? "app" : std::move(author); }

	/// @brief Lets importCsv read files in a folder besides the screen's own.
	/// @param directory The folder.
	void allowDirectory(const std::filesystem::path& directory);

	/// @brief A file an importCsv step names, if it may be read: inside the screen's folder or an allowed one.
	/// @param file The path as written (relative).
	/// @param where Its place, for errors.
	/// @return The path to read.
	/// @throws DocumentError If it lies outside them.
	[[nodiscard]] std::filesystem::path allowedFile(const std::string& file, const std::string& where) const;

	/// @brief Refuses a write the screen's data rules don't let the author make (`/data/<collection>`).
	/// @param collection The collection written.
	/// @param where Its place, for errors.
	/// @throws DocumentError With the rule.
	void checkWrite(const std::string& collection, const std::string& where) const;

	/// @brief Groups the writes of the event running into one step of the store's undo, until the event has been
	/// delivered (a deferred task of the window ends the group). Without a window each write step is its own step.
	void groupEvent();
	/// @brief Ends the event's group: what follows is a step of undo of its own (an all-or-nothing write, which may
	/// undo itself).
	void endGroup() noexcept { eventBatch_.reset(); }

	/// @brief Evaluates an expression for the store (migrations, imports, CSV conversions).
	/// @param expression The expression.
	/// @param variables Its variables.
	/// @return Its value.
	[[nodiscard]] static json evaluate(const std::string& expression, const json& variables);

private:
	/// @brief Connects to a store's changes, which ask for an update.
	/// @param store The store.
	void watch(data::Store& store);
	/// @brief Follows the document's models with the store of main, or with the preview's copy.
	void follow();

	Engine& engine_;                                          ///< The runtime.
	std::unique_ptr<data::Store> own_;                        ///< The in-memory store, until the host gives one.
	data::Store* store_ = nullptr;                            ///< The store of main.
	std::unique_ptr<data::Store> preview_;                    ///< The copy a preview reads and writes.
	Connection connection_ = 0;                               ///< To the current store's onChange.
	data::Store* watched_ = nullptr;                          ///< The store connection_ is on.
	std::string author_ = "app";                              ///< See author().
	std::vector<std::filesystem::path> allowed_;              ///< See allowDirectory().
	json value_ = json::object();                             ///< See variable().
	std::uint64_t storeVersion_ = 1;                          ///< Bumped by every change of the store read.
	std::uint64_t builtStore_ = 0;                            ///< The store version value_ was built at.
	std::uint64_t builtState_ = 0;                            ///< The state version value_ was built at.
	bool building_ = false;                                   ///< variable() is running.
	ReadSet queryReads_;                                      ///< See queryReads().
	std::map<std::string, json, std::less<>> queryArguments_; ///< Each query's evaluated declaration, last built.
	std::unique_ptr<data::Store::Batch> eventBatch_; ///< The running event's writes (groupEvent()); goes first.
};

} // namespace affresco::format
