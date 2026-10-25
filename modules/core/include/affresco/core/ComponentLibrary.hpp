#pragma once

/// @file
/// @brief The ComponentLibrary interface: the components and design tokens every document may use without defining
/// them (the kits and themes of modules/themes), and the one library a program installs for its documents.

#include "affresco/core/Json.hpp"

#include <atomic>
#include <string_view>

namespace affresco {

/// @brief Components and design tokens a program offers every document: a document's `"use"` of a name its own
/// `components` doesn't define, and an `"@token"` its `tokens` don't, fall back on them.
///
/// The format resolves names through this interface and never links the module that provides it (modules/themes, or
/// a program's own): the three layers are applications (documents), kits and themes (an implementation of this), and
/// the engine (the format, which reads it). Everything it returns is immutable and lives as long as the library.
class ComponentLibrary {
public:
	virtual ~ComponentLibrary() = default;

	/// @brief A component, as a document's `components` would define it: `{params, node}`, and `runsActions` when it
	/// runs the actions its parameters name (the parameter's name, or true when it runs actions it keeps in the state).
	/// @param name Its name.
	/// @return Its definition, or nullptr if there is none by that name.
	[[nodiscard]] virtual const json* component(std::string_view name) const = 0;

	/// @brief Every component.
	/// @return An object, name → definition, in the order the library lists them.
	[[nodiscard]] virtual const json& components() const = 0;

	/// @brief Every design token, as the library describes it.
	/// @return An object, name → `{dark, light, about}` for a colour (dark is the base) or `{value, about}` for a size,
	/// in the order the library lists them.
	[[nodiscard]] virtual const json& tokens() const = 0;

	/// @brief The tokens in the shape of a document's `tokens` section, for a document's defaults.
	/// @param themed true: each colour with its light and dark variants (`{"dark": ..., "light": ...}`); false: the
	/// dark (base) values alone.
	/// @return The section.
	[[nodiscard]] virtual const json& tokenDefaults(bool themed) const = 0;
};

namespace detail {

/// @brief Where the installed library is kept: a pointer written once at start-up and read from any thread.
/// @return The slot.
inline std::atomic<const ComponentLibrary*>& componentLibrarySlot() noexcept {
	static std::atomic<const ComponentLibrary*> slot{nullptr};
	return slot;
}

} // namespace detail

/// @brief Installs the library every document falls back on (with the log sink, the documented exceptions to "no
/// global state": a program has one set of kits and themes, which every document, checker and tool shares).
///
/// The composition root calls it at start-up (affresco::Screen does, with modules/themes), and so does every other
/// entry point that reads documents without a Screen (the command-line tool, the document server, tests). Installing
/// the same library again changes nothing.
/// @param library The library; it must live until the program ends (or until another is installed), as documents
/// keep pointers into it. nullptr: none, so only a document's own components and tokens exist.
inline void installComponentLibrary(const ComponentLibrary* library) noexcept {
	detail::componentLibrarySlot().store(library, std::memory_order_release);
}

/// @brief The installed library.
/// @return It, or nullptr if none was installed.
[[nodiscard]] inline const ComponentLibrary* componentLibrary() noexcept {
	return detail::componentLibrarySlot().load(std::memory_order_acquire);
}

} // namespace affresco
