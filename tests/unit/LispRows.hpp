#pragma once

/// @file
/// @brief What the tests of the forms a binding writes as an expression share (modules/lisp/reference/bindings.md): a
/// binding's text, its error, and each row run both ways, as the program's function and as the expression, over values
/// of every kind.

#include "LispApp.hpp"

#include <affresco/affresco.hpp>
#include <affresco/lisp/Program.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lisprows {

using affresco::json;
namespace lisp = affresco::lisp;

/// @brief What a program builds.
/// @param text The program.
/// @return The tree.
inline json built(const std::string& text) {
	lisp::Program program;
	program.addFile(text, "app.lisp");
	return program.build();
}

/// @brief What a binding's form builds: `(defscreen main (node :x ~form))`'s `x`.
/// @param form The form.
/// @param definitions Definitions it uses.
/// @return The value.
inline json binding(const std::string& form, const std::string& definitions = "") {
	return built(definitions + "\n(defscreen main (node :x ~" + form + "))")["root"]["x"];
}

/// @brief The message of the error a binding's build gives.
/// @param form The binding's form.
/// @return The message, or "" if it builds.
inline std::string bindingError(const std::string& form) {
	try {
		static_cast<void>(binding(form));
	} catch(const lisp::LispError& error) {
		return error.what();
	}
	return "";
}

/// The values each row runs on, as the language writes them: one of each kind, and the edges of truth.
inline constexpr std::array<std::string_view, 14> values = {"nil", "true", "false", "0",       "1",  "-1",    "2.0",
                                                            "1.5", "\"\"", "\"a\"", "\" a \"", "[]", "[1 2]", "{}"};

/// @brief Writes each `x` in a row's form as another word.
/// @param form The form, its value `x` written alone or before a path (`x.title`).
/// @param word The word.
/// @return The form.
inline std::string with(const std::string& form, const std::string& word) {
	std::string text;
	for(std::size_t at = 0; at < form.size(); ++at) {
		const bool alone = form[at] == 'x' && (at == 0 || std::string_view("( [").contains(form[at - 1])) &&
		                   (at + 1 == form.size() || std::string_view(") ].").contains(form[at + 1]));
		text += alone ? word : std::string(1, form[at]);
	}
	return text;
}

/// @brief Writes a text as the language's string.
/// @param text The text.
/// @return The string, in quotes.
inline std::string lispString(const std::string& text) {
	std::string written = "\"";
	for(const char c : text) {
		if(c == '"' || c == '\\') written += '\\';
		written += c;
	}
	return written + "\"";
}

/// @brief What a row gave on one value, run one way.
struct Outcome {
	bool failed = false; ///< Whether it reported an error.
	json value;          ///< What it gave, if not.
};

/// @brief Runs rows on a value both ways: as the program's function `(defn row [x] form)`, called as `lisp('row', v)`,
/// and as the expression the row's binding is written as.
/// @param rows The rows' forms.
/// @param value The value, as the language writes it.
/// @return For each row, what the function gave and what the expression gave.
inline std::vector<std::pair<Outcome, Outcome>> runBothWays(const std::vector<std::string>& rows,
                                                            const std::string& value) {
	std::vector<std::string> texts;
	for(const std::string& row : rows) {
		try {
			texts.push_back(binding(with(row, "state.v")).get<std::string>());
		} catch(const lisp::LispError&) {
			texts.emplace_back("unset"); // a binding refused
		}
	}
	std::string program;
	std::string actions;
	for(std::size_t at = 0; at < rows.size(); ++at) {
		const std::string number = std::to_string(at);
		program += "(defn row" + number + " \"A row.\" [x] " + rows[at] + ")\n";
		actions += " :e" + number + " [{:set \"state.out\" :to " + lispString(texts[at]) + "}]";
		actions += " :f" + number;
		actions += R"( [{:set "state.out" :to "{{lisp('row)" + number;
		actions += R"(', state.v)}}"}])";
	}
	program += "(defscreen main :state {:v " + value +
	           " :out 0}\n  :actions {:clear [{:set \"state.out\" :to \"unset\"}]" + actions + "}\n  (column#root))\n";
	lispapp::App app(program);
	REQUIRE(app.errors.empty());
	const auto run = [&](const std::string& action) {
		const std::size_t before = app.errors.size();
		app.runtime->runAction("clear", app.window->root(), json::object());
		app.runtime->runAction(action, app.window->root(), json::object());
		app.settle();
		const bool failed = app.errors.size() != before || app.state("out") == "unset";
		return Outcome{failed, failed ? json() : app.state("out")};
	};
	std::vector<std::pair<Outcome, Outcome>> outcomes;
	outcomes.reserve(rows.size());
	for(std::size_t at = 0; at < rows.size(); ++at)
		outcomes.emplace_back(run("f" + std::to_string(at)), run("e" + std::to_string(at)));
	return outcomes;
}

/// @brief Where each row's expression gives another value than its function, or an error, where the function gives a
/// value. Where the function gives an error the expression may give a value: the binding is wrong either way, and the
/// rows before this package do the same (`(inc "a")` is "a1").
/// @param rows The rows.
/// @param over The values, as the language writes them; every kind's by default.
/// @return For each row, the values it disagrees on, each after a space, "" if none.
inline std::vector<std::string> disagreements(const std::vector<std::string>& rows,
                                              std::span<const std::string_view> over = values) {
	std::vector<std::string> found(rows.size());
	for(const std::string_view view : over) {
		const std::string value(view);
		const std::vector<std::pair<Outcome, Outcome>> outcomes = runBothWays(rows, value);
		for(std::size_t at = 0; at < rows.size(); ++at) {
			const auto& [function, expression] = outcomes[at];
			if(!function.failed && (expression.failed || function.value != expression.value)) found[at] += " " + value;
		}
	}
	return found;
}

/// @brief A row and the values its expression disagrees on.
struct Row {
	std::string form;    ///< The form, its value `x`.
	std::string differs; ///< The values, each after a space, as disagreements() gives them.
};

} // namespace lisprows
