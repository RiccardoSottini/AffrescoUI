#pragma once

/// @file
/// @brief What the tests of a document's live program share (RFC 0005): a screen on a program, on a headless window.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lispapp {

using affresco::json;
using affresco::Key;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::Runtime;
using affresco::vcs::Repository;
namespace lisp = affresco::lisp;

constexpr int controlKey = 0x2; ///< GLFW_MOD_CONTROL.

/// @brief A document's rules with the scan of what a revision adds to the triggers taken off: it is the first line
/// against these attacks, and these tests are of the second, which has to hold by itself, so an attacker's definitions
/// land as the real commits of the real author that a gap in the scan would let through.
class WithoutTheScan final : public affresco::DocumentRules {
public:
	/// @brief Sets it up.
	/// @param scan Whether to run the scan after all.
	explicit WithoutTheScan(bool scan) : scan_(scan) {}
	/// @brief Gives the rules the host's services, which judge the uses of them a revision adds.
	/// @param services They.
	void setServices(const affresco::services::Services* services) noexcept { rules_.setServices(services); }
	/// @copydoc DocumentRules::validate
	void validate(const json& document) const override { rules_.validate(document); }
	/// @copydoc DocumentRules::findNode
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override {
		return rules_.findNode(document, id);
	}
	/// @copydoc DocumentRules::permissions
	[[nodiscard]] affresco::Permissions permissions(const json& document) const override {
		return rules_.permissions(document);
	}
	/// @copydoc DocumentRules::built
	[[nodiscard]] std::optional<json> built(const json& document) const override { return rules_.built(document); }
	/// @copydoc DocumentRules::rewriteOperation
	void rewriteOperation(const json& document, json& operation) const override {
		rules_.rewriteOperation(document, operation);
	}
	/// @copydoc DocumentRules::checkChange
	void checkChange(const json& before, const json& after) const override {
		if(scan_) rules_.checkChange(before, after);
	}
	/// @copydoc DocumentRules::checkOwnChange
	void checkOwnChange(const json& before, const json& after) const override { rules_.checkOwnChange(before, after); }
	/// @copydoc DocumentRules::reviewLines
	[[nodiscard]] std::vector<std::string> reviewLines(const json& before, const json& after) const override {
		return rules_.reviewLines(before, after);
	}

private:
	affresco::format::Rules rules_; ///< The rules.
	bool scan_;                     ///< See the constructor.
};

/// @brief A screen on a program, on a headless window; the file on disk holds its text, so its author is the app's own.
struct App {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	WithoutTheScan rules;                     ///< What revisions are judged by.
	Repository repository;                    ///< The history.
	std::unique_ptr<Runtime> runtime;         ///< The running screen.
	int hits = 0;                             ///< How many times the application's function ran.
	std::vector<std::string> errors;          ///< What was reported.

	/// @brief Builds a screen on a program.
	/// @param text The program.
	/// @param scan Whether revisions are scanned for new ways to reach control steps.
	explicit App(const std::string& text, bool scan = false)
	    : window(createHeadlessWindow(400, 300, clock)), rules(scan),
	      repository(rules, lisp::sourceDocument(text, "app.lisp")),
	      runtime(std::make_unique<Runtime>(repository, "")) {
		runtime->onCall("hit", [this](const json&) {
			++hits;
			return json();
		});
		runtime->onError.connect([this](const std::string& message) { errors.push_back(message); });
		runtime->build(*window);
		window->render();
	}

	/// @brief Presses Ctrl and a key, the person's own input.
	/// @param key The key.
	void press(Key key) const {
		window->routeKey({key, 0, affresco::Action::Press, affresco::Mods{controlKey}});
		window->routeKey({key, 0, affresco::Action::Release, affresco::Mods{controlKey}});
		window->render();
	}

	/// @brief Types a text, a key at a time, as the person does, into what has the focus.
	/// @param text The text.
	void type(const std::string& text) const {
		for(const char c : text) {
			if(c == '\n') {
				window->routeKey({Key::Enter, 0, affresco::Action::Press, affresco::Mods{}});
				window->routeKey({Key::Enter, 0, affresco::Action::Release, affresco::Mods{}});
			} else {
				window->routeChar({static_cast<char32_t>(static_cast<unsigned char>(c))});
			}
		}
		window->render();
	}

	/// @brief Lets what was deferred from an event happen: a revision, an evaluation.
	void settle() const {
		window->render();
		window->render();
	}

	/// @brief Moves the clock on, with frames: timers fire by themselves.
	/// @param ms How long.
	void wait(double ms) {
		clock.advance(ms);
		window->render();
		window->render();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value.
	[[nodiscard]] json state(const char* key) const {
		return repository.state().contains(key) ? repository.state()[key] : json();
	}

	/// @brief A revision that sets a definition, by an author, which must apply.
	/// @param name The definition's address.
	/// @param text Its text.
	/// @param author Who writes it.
	void write(const std::string& name, const std::string& text, const std::string& author = "agent/x") {
		commit({{"author", author}, {"message", "m"}, {"set", {{"/program/app.lisp/" + name, text}}}});
	}

	/// @brief Commits a revision, which must apply.
	/// @param revision The revision.
	void commit(const json& revision) {
		const std::optional<std::string> error = repository.commit(revision);
		INFO(error.value_or(""));
		REQUIRE_FALSE(error.has_value());
	}

	/// @brief Tells whether a reason was reported holding a text.
	/// @param text The text.
	/// @return true if it was.
	[[nodiscard]] bool reported(const std::string& text) const {
		return std::ranges::any_of(errors, [&](const std::string& error) { return error.contains(text); });
	}
};

} // namespace lispapp
