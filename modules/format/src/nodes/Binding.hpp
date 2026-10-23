#pragma once

/// @file
/// @brief format::Binding: a value written in a document, compiled once and evaluated on every update.

#include "Expression.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace affresco::format {

namespace detail {
struct CompiledValue;
} // namespace detail

/// @brief A value written in a document, compiled once: its templates split, its operators lowered.
///
/// evaluate() gives what format::evaluate() gives for the value as written, the same errors at the same places, without
/// parsing or hashing its text again; the place is written out only when an error is reported. A binding refers to the
/// value as written, which must outlive it (a compiled node keeps its own copy). Main thread only, like the compile
/// cache it reads.
class Binding {
public:
	/// @brief Compiles a value that is evaluated.
	/// @param written The value as written; outlives the binding.
	explicit Binding(const json& written);
	/// @brief A value that is never evaluated: structural keys ("bind", handlers) are read as written.
	/// @param written The value as written; outlives the binding.
	/// @return The binding.
	[[nodiscard]] static Binding constant(const json& written);

	Binding(const Binding&) = delete;
	Binding& operator=(const Binding&) = delete;
	/// @brief Takes over another binding.
	Binding(Binding&&) noexcept;
	/// @brief Takes over another binding.
	/// @return This one.
	Binding& operator=(Binding&&) noexcept;
	/// @brief Frees its compiled form.
	~Binding();

	/// @brief Whether evaluating it may give something other than the value as written.
	[[nodiscard]] bool dynamic() const noexcept { return compiled_ != nullptr; }
	/// @brief The value as written.
	[[nodiscard]] const json& written() const noexcept { return *written_; }

	/// @brief Evaluates the value.
	/// @param context What paths can see.
	/// @param where Its place, for errors; with @p key, the place of the node that has it.
	/// @param key The key it is under in that node ("<where>.<key>"), or empty.
	/// @param name A key under that one ("<where>.<key>.<name>", e.g. "with.title"), or empty.
	/// @return The value (a copy of the value as written, if it isn't dynamic()).
	/// @throws DocumentError If a template, expression or operator in it is invalid.
	[[nodiscard]] json evaluate(const Context& context, const std::string& where, std::string_view key = {},
	                            std::string_view name = {}) const;

private:
	/// @brief A binding to a value as written.
	/// @param written The value.
	/// @param compiled Its compiled form; null when it is never evaluated, or evaluating it gives it back.
	Binding(const json& written, std::unique_ptr<const detail::CompiledValue> compiled) noexcept;

	const json* written_;                                   ///< The value as written.
	std::unique_ptr<const detail::CompiledValue> compiled_; ///< Null when it isn't dynamic().
};

} // namespace affresco::format
