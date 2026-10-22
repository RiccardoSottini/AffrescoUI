#pragma once

/// @file
/// @brief Forge's file buffers: the text of each open file, kept natively in a TextModel (never in the screen's JSON
/// state), shared by the editors that show it, with whether it differs from the file, save and revert. Plain C++ with
/// no window, so the unit tests use it too.

#include <affresco/core/DataModel.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/core/Signal.hpp>
#include <affresco/render/TextModel.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace forge {

/// @brief How much the buffers hold.
struct BufferLimits {
	std::size_t buffers = 64;                    ///< Files open at once.
	std::size_t fileBytes = std::size_t{256} << 20; ///< The largest file opened (a million lines fit easily).
	std::size_t keptBytes = std::size_t{8} << 20;   ///< Files up to this size keep their saved text, so an edit undone
	                                             ///< by hand is clean again; larger ones stay dirty until saved.
	std::size_t todos = 1000;                    ///< TODO and FIXME lines todos() lists at most.
};

/// @brief The open files' texts, by id (the path relative to the open folder, as FileIndex.hpp gives it).
///
/// A buffer's TextModel is shared (std::shared_ptr) because every editor group showing the file edits the same text,
/// and each editor holds it while it shows it. Changes are seen through the models' onChanged: the dirty handler is
/// called when a buffer starts or stops differing from its file, and models() lists the buffers for the screen.
class Buffers {
public:
	/// @brief Told when a buffer becomes dirty or clean by an edit (not by save() or revert(), whose callers know).
	using DirtyHandler = std::function<void(const std::string& id, bool dirty)>;

	/// @brief No buffers.
	/// @param limits How much they hold.
	explicit Buffers(BufferLimits limits = {});
	/// @brief Disconnects from the models, which editors may still hold.
	~Buffers();
	Buffers(const Buffers&) = delete;
	Buffers& operator=(const Buffers&) = delete;

	/// @brief Sets the dirty handler.
	/// @param handler It, or empty for none.
	void onDirty(DirtyHandler handler) { onDirty_ = std::move(handler); }

	/// @brief Opens a file of a folder into a buffer; one already open is left as it is.
	/// @param root The folder.
	/// @param id The file, relative to it.
	/// @return An error, or std::nullopt.
	[[nodiscard]] std::optional<std::string> open(const std::filesystem::path& root, const std::string& id);
	/// @brief Writes a buffer to its file (a new file beside it, then renamed over it, so a failure loses nothing).
	/// @param id The buffer.
	/// @return An error, or std::nullopt.
	[[nodiscard]] std::optional<std::string> save(const std::string& id);
	/// @brief Writes every dirty buffer.
	/// @return The first error, or std::nullopt; the others are still saved.
	[[nodiscard]] std::optional<std::string> saveAll();
	/// @brief Reads a buffer's file again, dropping its edits.
	/// @param id The buffer.
	/// @return An error, or std::nullopt.
	[[nodiscard]] std::optional<std::string> revert(const std::string& id);
	/// @brief Forgets a buffer (an editor still showing it keeps its text).
	/// @param id The buffer.
	/// @return false if there was none.
	bool close(const std::string& id);

	/// @brief A buffer's text.
	/// @param id The buffer.
	/// @return Its model, or nullptr if it isn't open.
	[[nodiscard]] std::shared_ptr<affresco::TextModel> find(std::string_view id) const;
	/// @brief Whether a buffer differs from its file.
	/// @param id The buffer.
	/// @return false for none.
	[[nodiscard]] bool dirty(std::string_view id) const;
	/// @brief The buffers that differ from their files.
	/// @return Their ids, in order.
	[[nodiscard]] std::vector<std::string> dirtyIds() const;
	/// @brief The buffers open.
	[[nodiscard]] std::size_t size() const noexcept { return buffers_.size(); }
	/// @brief The buffers, for `"repeat": {"in": "models.buffers"}`: `{ "id", "label", "dirty", "lines", "bytes" }`
	/// each, by id; a row changes when its buffer opens, closes, is saved or reverted, or turns dirty or clean.
	[[nodiscard]] std::shared_ptr<affresco::ListModel> model() const noexcept { return model_; }
	/// @brief The TODO and FIXME lines of the buffers up to keptBytes, for Problems.
	/// @return `[{ "severity": "info" | "warning", "message", "file", "label", "line" }]`, at most `todos` of them.
	[[nodiscard]] affresco::json todos() const;

private:
	/// @brief One open file.
	struct Buffer {
		std::shared_ptr<affresco::TextModel> text; ///< Shared with the editors showing it.
		std::filesystem::path path;                ///< The file.
		std::string label;                         ///< Its name.
		std::size_t savedSize = 0;                 ///< The file's size when last read or written.
		std::optional<std::string> saved;          ///< Its text then, if it is at most keptBytes.
		bool dirty = false;                        ///< See dirty().
		affresco::Connection connection = 0;       ///< To text->onChanged.
	};

	/// @brief Works out whether a buffer is dirty after an edit, and tells the handler if that changed.
	/// @param id The buffer.
	void changed(const std::string& id);
	/// @brief Takes a buffer's text as its file's, now clean.
	/// @param buffer The buffer.
	void markSaved(Buffer& buffer) const;
	/// @brief Lists the buffers again in model().
	void refreshModel() const;

	BufferLimits limits_;                                 ///< See the constructor.
	std::map<std::string, Buffer, std::less<>> buffers_; ///< By id.
	DirtyHandler onDirty_;                                ///< See onDirty().
	std::shared_ptr<affresco::ListModel> model_;          ///< See model(); shared with the Screen that shows it.
	bool quiet_ = false;                                  ///< While save() or revert() changes a text.
};

} // namespace forge
