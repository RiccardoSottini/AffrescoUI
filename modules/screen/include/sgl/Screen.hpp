#pragma once

#include "sgl/core/Channel.hpp"
#include "sgl/core/Executor.hpp"
#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"
#include "sgl/core/VersionControl.hpp"
#include "sgl/render/Application.hpp"
#include "sgl/render/Color.hpp"
#include "sgl/render/Font.hpp"
#include "sgl/render/Panel.hpp"
#include "sgl/render/Window.hpp"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

// A malformed screen file. what() names the place in the JSON, e.g. "root.children[1].anchors: unknown edge 'lefft'"
using ScreenError = DocumentError;

// A live UI described in the sGLGUI Screen Format (JSON, see SCREEN_FORMAT.md), built into a Window.
//
// A Screen puts the library's modules together, each talking to the others only through the interfaces in
// sgl/core:
//   format::Rules      the screen format's rules          (DocumentRules)
//   vcs::Repository    document, state, history, review   (VersionControl, uses DocumentRules)
//   format::Runtime    panels, reconciliation, actions    (observes VersionControl, uses the rendering engine)
//   channel::Session   the live protocol, one per peer    (uses VersionControl, any Channel)
// Use the modules directly to put them together differently. Keep the Screen alive while its window is used.
class Screen {
public:
	using Action = std::function<void(Panel& sender)>;
	using Factory = std::function<std::unique_ptr<Panel>()>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>;
	using ProposalInfo = sgl::ProposalInfo;
	using HistoryEntry = sgl::HistoryEntry;

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
	[[nodiscard]] std::vector<ProposalInfo> proposals() const;
	bool acceptProposal(int id, std::string* error = nullptr);
	bool rejectProposal(int id, std::string_view reason = {});
	bool previewProposal(int id, std::string* error = nullptr); // shows it live, without committing it
	void endPreview();
	[[nodiscard]] std::optional<int> previewedProposal() const noexcept;

	/* History */
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

	/* Peers: programs that read and change the screen over the live protocol (SCREEN_FORMAT.md, "Live screens").
	   The screen doesn't care what they are: a client app, an AI agent's MCP server, a script. Everything a peer sends
	   is authored by the name it was connected with (the name "permissions" knows it by), whatever name it gives
	   itself. Every peer hears about commits, proposals and "emit" events; "request"/"reply" messages are passed on
	   to the other peers. */

	// A peer at the other end of `channel` (which must outlive this screen)
	void connect(Channel& channel, std::string author = "channel");

	// Accepts any number of peers on a local socket at `path` (POSIX). Each one is authored as `author`: give
	// different kinds of peer different sockets, e.g. "studio.sock" for the user's client and "agent.sock" for an
	// AI agent. Throws std::runtime_error if the socket can't be created.
	void listen(Executor& executor, const std::filesystem::path& path, std::string author);

	// See VersionControl::setAutoAccept: changes that would wait for review are applied right away
	void setAutoAccept(bool enabled);

	/* The modules, for code that needs more than this class offers */
	[[nodiscard]] VersionControl& versions() noexcept;

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

	std::unique_ptr<Impl> impl_;
};

} // namespace sgl
