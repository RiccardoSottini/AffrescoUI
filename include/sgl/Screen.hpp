#pragma once

#include "sgl/Color.hpp"
#include "sgl/Font.hpp"
#include "sgl/Panel.hpp"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sgl/Signal.hpp"

namespace sgl {

class Application;
class Channel;
class Window;

// A malformed screen file. what() names the place in the JSON, e.g. "root.children[1].anchors: unknown edge 'lefft'"
class ScreenError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

enum class RevisionStatus {
	Applied,  // committed: the screen changed
	Proposed, // waiting for the user's decision (the author's policy is "review")
	Rejected, // nothing changed; `error` says why
};

struct RevisionResult {
	RevisionStatus status;
	int revision = 0; // the current revision after the call
	int proposal = 0; // when proposed: its id
	std::string error;
};

// A UI described in the sGLGUI Screen Format (JSON, see SCREEN_FORMAT.md) and built into a Window.
//
// The file holds layout, state, bindings and actions; code can add behaviour too (native actions, ids, templates,
// custom types). A screen is live: when its state changes the UI is updated in place, and revisions (JSON Patch) can
// change the document while the app runs, e.g. sent by a program on the other end of a Channel.
// Keep the Screen alive while its window is used.
class Screen {
public:
	using Action = std::function<void(Panel& sender)>;
	using Factory = std::function<std::unique_ptr<Panel>()>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>;

	[[nodiscard]] static Screen fromFile(const std::filesystem::path& path); // throws ScreenError
	// `baseDirectory` is where relative paths (fonts) are resolved from
	[[nodiscard]] static Screen fromString(std::string_view json, std::filesystem::path baseDirectory = {});

	Screen(Screen&&) noexcept;
	Screen& operator=(Screen&&) noexcept;
	~Screen();

	/* Before building */
	void registerType(std::string name, Factory factory); // a custom "type" usable in the file
	void setPlaceholderTypes(bool enabled);               // unknown types become plain panels instead of an error

	/* Building */
	Window& createWindow(Application& app); // window from the "window" section, then build()
	void build(Window& window);             // fonts, then "root" into the window's root panel

	// Builds a copy of the template `name` as a new child of `parent` (a "repeat" in the file is usually simpler)
	Panel& instantiate(std::string_view name, Panel& parent);

	/* Behaviour */
	// A native action: called for "onClick": "name" or a {"call": "name"} step, when the file has no action `name`
	void onAction(std::string name, Action action);
	void onUnhandledAction(UnhandledAction fallback); // for actions nobody handles (default: report on stderr)

	// Runs the action `name` (from the file, else native) as if `sender` triggered it; `eventJson` becomes "event"
	void runAction(std::string_view name, Panel& sender, std::string_view eventJson = "{}");

	/* Live document: revisions (see GOVERNANCE.md) */
	// A revision {"author", "message", "parent"?, "patch": [JSON Patch ops]} from someone who must follow the file's
	// "permissions": applied, proposed for review, or rejected. On any error nothing changes.
	RevisionResult submitRevision(std::string_view revisionJson);
	// A revision from the application itself: not reviewed, but locks and the other rules still apply
	bool applyRevision(std::string_view revisionJson, std::string* error = nullptr);

	/* Review before apply */
	struct ProposalInfo {
		int id;
		std::string author, message;
		std::size_t operations;
		std::string time;
	};
	[[nodiscard]] std::vector<ProposalInfo> proposals() const;
	bool acceptProposal(int id, std::string* error = nullptr);
	bool rejectProposal(int id, std::string_view reason = {});
	bool previewProposal(int id, std::string* error = nullptr); // shows it live, without committing it
	void endPreview();
	[[nodiscard]] std::optional<int> previewedProposal() const noexcept;

	/* History */
	struct HistoryEntry {
		int number;
		std::string author, message, time;
	};
	[[nodiscard]] std::vector<HistoryEntry> history() const;
	// A new revision equal to an old one (the history keeps growing), applied as the application's own change
	bool restore(int revision, std::string* error = nullptr);
	RevisionResult submitRestore(int revision, std::string_view author); // the same, following `author`'s permissions
	bool undo(std::string* error = nullptr);                             // restore the previous revision
	[[nodiscard]] int revision() const noexcept;                         // 0 = the file as loaded

	// Change sets ("plugins"): the revisions after `fromRevision`, portable to other copies of the same screen
	[[nodiscard]] std::string exportChanges(int fromRevision = 0, std::string_view name = {}) const;
	bool importChanges(std::string_view changesJson, std::string* error = nullptr); // all or nothing

	[[nodiscard]] std::string documentJson() const;
	[[nodiscard]] std::string stateJson() const;
	Connection onEvent(std::function<void(const std::string& eventJson)> listener); // "emit" events

	// Talks to whatever is on the other end of `channel` (the channel must outlive this screen): sends the document
	// and "emit" events, receives revisions. Protocol: SCREEN_FORMAT.md, "Live screens".
	// Everything arriving there is authored by `author` (the name "permissions.authors" knows it by), whatever
	// name it gives itself: a channel can't claim to be someone it isn't.
	void connect(Channel& channel, std::string author = "channel");

	/* Lookup (after build) */
	[[nodiscard]] Panel* find(std::string_view id) noexcept;

	template <std::derived_from<Panel> T = Panel> [[nodiscard]] T& get(std::string_view id) {
		T* panel = dynamic_cast<T*>(find(id));
		if(!panel) throw ScreenError("sGLGUI screen: no panel with id '" + std::string(id) + "' of the requested type");
		return *panel;
	}

	[[nodiscard]] Color color(std::string_view name) const;                      // from "colors"
	[[nodiscard]] std::shared_ptr<const Font> font(std::string_view name) const; // from "fonts"

private:
	struct Impl;
	explicit Screen(std::unique_ptr<Impl> impl);
	[[nodiscard]] int currentRevision() const noexcept;

	std::unique_ptr<Impl> impl_;
};

} // namespace sgl
