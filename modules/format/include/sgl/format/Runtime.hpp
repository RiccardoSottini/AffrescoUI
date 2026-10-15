#pragma once

#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"
#include "sgl/core/VersionControl.hpp"
#include "sgl/render/Application.hpp"
#include "sgl/render/Color.hpp"
#include "sgl/render/Font.hpp"
#include "sgl/render/Panel.hpp"
#include "sgl/render/Window.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace sgl::format {

// Brings a screen document to life with the rendering engine: builds it into a Window's panel tree, keeps the panels
// in line with the document and the state (reconciliation), and runs actions when the user interacts.
//
// The document and the state come from a VersionControl (which must outlive the runtime): the runtime observes it,
// rebuilding when a revision lands, and drives it from control steps (accept, preview, restore, ...).
class Runtime {
public:
	using Action = std::function<void(Panel& sender)>;
	using Factory = std::function<std::unique_ptr<Panel>()>;
	using UnhandledAction = std::function<void(const std::string& action, Panel& sender)>;

	// `baseDirectory` is where relative paths (fonts) are resolved from. Throws DocumentError (e.g. a missing font).
	Runtime(VersionControl& versions, std::filesystem::path baseDirectory);
	~Runtime();

	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	/* Before building */
	void registerType(std::string name, Factory factory);
	void setPlaceholderTypes(bool enabled);

	/* Building */
	Window& createWindow(Application& app); // from the "window" section, then build()
	void build(Window& window);
	Panel& instantiate(std::string_view name, Panel& parent);

	/* Behaviour */
	void onAction(std::string name, Action action);
	void onUnhandledAction(UnhandledAction fallback);
	void runAction(std::string_view name, Panel& sender, const json& event);

	/* Lookup */
	[[nodiscard]] Panel* find(std::string_view id) noexcept;
	[[nodiscard]] Color color(std::string_view name) const;
	[[nodiscard]] std::shared_ptr<const Font> font(std::string_view name) const;

	Signal<const json&> onEvent;        // "emit" steps: {"type": "event", "name", "data", "revision"}
	Signal<const std::string&> onError; // errors in handlers and deferred changes (also printed on stderr)

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace sgl::format
