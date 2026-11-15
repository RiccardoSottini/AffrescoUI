#pragma once

/// @file
/// @brief format::Picks: what the person pointed at with a "pick" step, kept by the engine until it is removed or
/// sent; describePick(), the record of a picked panel; sourceOf(), where a node is written (Runtime::source()).

#include "affresco/core/Json.hpp"
#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace affresco {
class Window;
} // namespace affresco

namespace affresco::format {

struct Engine;

/// @brief The pick records the person made and has neither removed ("unpick") nor sent ("ask" with "picks").
///
/// The engine makes every record (describePick()); a document only names one by its id, so what agents get is never
/// what a document wrote. Invariants: at most maxKept records, the oldest going first; ids are `pick-<n>`, counted
/// from 1 in each run, never reused. Callers: the "pick", "unpick" and "ask" steps, Runtime::picks(). Main thread only.
class Picks {
public:
	static constexpr std::size_t maxKept = 16; ///< Records kept; the oldest goes when one more is made.

	/// @brief Gives a record its id and keeps it.
	/// @param record What describePick() made.
	/// @return The record as kept, with its "id".
	const json& add(json record);
	/// @brief Forgets a record.
	/// @param id Its id; an unknown one is ignored.
	void remove(std::string_view id);
	/// @brief Whether a record is kept.
	/// @param id Its id.
	/// @return Whether it is.
	[[nodiscard]] bool holds(std::string_view id) const;
	/// @brief The records for a request; the request forgets them once it is sent (remove()).
	/// @param ids Their ids; one not held is left out.
	/// @return Copies of the records, in the order of @p ids.
	[[nodiscard]] json records(std::span<const std::string> ids) const;
	/// @brief The records kept, oldest first.
	/// @return `[record]`.
	[[nodiscard]] json current() const;

	/// @brief Notes the window's pick a step started, so cancelRunning() can end it.
	/// @param id Window::pick()'s id.
	void started(std::uint64_t id) noexcept { running_ = id; }
	/// @brief Notes that the pick ended (its callback ran).
	void ended() noexcept { running_ = 0; }
	/// @brief Ends the pick a step started, if it still runs, without its callback: its runtime goes away.
	/// @param window The window it runs in, or nullptr.
	void cancelRunning(Window* window) noexcept;

private:
	std::deque<json> records_;  ///< See current().
	std::uint64_t made_ = 0;    ///< Records made in this run, for their ids.
	std::uint64_t running_ = 0; ///< The window's pick a step started and that hasn't ended, or 0.
};

/// @brief The record of a picked panel, without its id (SCREEN_FORMAT.md, "Pointing at a node"): the node as a
/// revision addresses it, where it is written, its bindings as written, its repeats, its component and its source.
/// It never evaluates a binding, reads the state or a record, or copies what a panel shows.
/// @param engine The runtime.
/// @param panel The panel the person picked.
/// @return The record (at most 4 KiB as JSON), or null if no node built the panel or one of its ancestors, or if its
/// ids or pointer are too long to fit.
[[nodiscard]] json describePick(Engine& engine, const Panel& panel);

/// @brief The panel a pick offers for a panel under the pointer: the nearest one a node built.
/// @param engine The runtime.
/// @param panel The panel under the pointer.
/// @return It, an ancestor, or nullptr.
[[nodiscard]] Panel* pickCandidate(Engine& engine, Panel* panel);

/// @brief The caption beside a panel offered: its type, its id, and for a program the definition that wrote it
/// (`label #cardTitle · card-title`).
/// @param engine The runtime.
/// @param panel A panel pickCandidate() gave.
/// @return The caption.
[[nodiscard]] std::string pickCaption(Engine& engine, const Panel& panel);

/// @brief Where a node is written (Runtime::source()). It reads the versioned document only.
/// @param engine The runtime.
/// @param at "#id", a JSON Pointer to a node, or a definition's address (`/program/<file>/<name>`).
/// @return The source.
/// @throws DocumentError Naming @p at, if nothing is there or it is longer than 512 bytes.
[[nodiscard]] json sourceOf(const Engine& engine, std::string_view at);

/// @brief A text cut to a size, at a character boundary (UTF-8).
/// @param text The text.
/// @param most The most bytes.
/// @return The text, at most @p most bytes.
[[nodiscard]] std::string cutToCharacter(std::string text, std::size_t most);

/// @brief Where a node with an id is written in the document: in root, then screens, templates, overlays and
/// components, then the features' views.
/// @param document The document as built.
/// @param id The id.
/// @return Its JSON Pointer, or std::nullopt (an empty id has none).
[[nodiscard]] std::optional<std::string> nodePointer(const json& document, std::string_view id);

/// @brief The `source` of a node a program built: the definition that wrote it (lisp::nodeOrigins()).
/// @param engine The runtime.
/// @param id The node's id.
/// @return `{"address", "kind", "file", "line", "column", "shared"}`, or std::nullopt for a document without a
/// program or a node it didn't build.
[[nodiscard]] std::optional<json> programSource(const Engine& engine, std::string_view id);

/// @brief The name of the definition that built a node, for a caption.
/// @param engine The runtime.
/// @param id The node's id.
/// @return The name, or std::nullopt (programSource()).
[[nodiscard]] std::optional<std::string> definitionName(const Engine& engine, std::string_view id);

/// @brief The `source` of a node of a JSON document: its address, and the file that holds it if the host says.
/// @param engine The runtime.
/// @param node The node as a revision addresses it.
/// @param pointer Where it is written.
/// @return `{"address", "file"?, "shared"?}`.
[[nodiscard]] json documentSource(const Engine& engine, std::string_view node, std::string_view pointer);

} // namespace affresco::format
