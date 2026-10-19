/// @file
/// @brief `affrescoui export` and `affrescoui import`: change sets ("plugins") against a screen's history on disk,
/// the one affrescoui_host keeps (VERSION_CONTROL.md, "History on disk"), without opening a window.

#include "Cli.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>

#include <array>
#include <charconv>
#include <iostream>

namespace affresco::cli {

namespace {

/// The options both commands take.
constexpr std::array<std::string_view, 1> dataOption = {"--data"};

/// @brief A screen with its history on disk, opened for one command; what the library logs while it opens is kept,
/// so that a history another program writes (the screen then keeps changes in memory only) can be refused.
struct OpenedScreen {
	std::vector<std::string> warnings; ///< What the library logged.
	std::optional<Screen> screen;      ///< The screen.

	/// @brief Opens a screen file with its history.
	/// @param file The file.
	/// @param data The data folder (Screen::defaultDataDirectory() if not given).
	OpenedScreen(const std::filesystem::path& file, const std::optional<std::string>& data) {
		setLogSink([this](LogLevel level, std::string_view message) {
			if(level != LogLevel::Debug && level != LogLevel::Info) warnings.emplace_back(message);
		});
		try {
			screen.emplace(Screen::fromFile(file, data ? std::filesystem::path(*data) : Screen::defaultDataDirectory()));
		} catch(...) {
			setLogSink({});
			throw;
		}
	}

	OpenedScreen(const OpenedScreen&) = delete;
	OpenedScreen& operator=(const OpenedScreen&) = delete;

	/// @brief Gives the log back to stderr, first passing on what was kept.
	~OpenedScreen() {
		setLogSink({});
		for(const std::string& warning : warnings) log(LogLevel::Warning, warning);
	}

	/// @brief Tells whether changes reach the history on disk: no other program writes it.
	/// @return true if they do.
	[[nodiscard]] bool writesToDisk() const {
		return std::ranges::none_of(warnings, [](const std::string& w) { return w.contains("kept in memory only"); });
	}
};

} // namespace

Exit exportChanges(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 4> valued = {"--from", "--name", "-o", "--data"};
	const Arguments arguments(args, valued, {});
	const std::filesystem::path file = arguments.file();
	int from = 0;
	if(const std::optional<std::string> text = arguments.value("--from")) {
		const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), from);
		if(error != std::errc() || end != text->data() + text->size() || from < 0)
			throw UsageError("--from: expected a revision number");
	}

	const OpenedScreen opened(file, arguments.value("--data"));
	const json changes = json::parse(opened.screen->exportChanges(from, arguments.value("--name").value_or(file.stem().stem().string())));
	const std::string text = changes.dump(1, '\t') + "\n";
	if(const std::optional<std::string> out = arguments.value("-o")) writeFile(*out, text);
	else std::cout << text;
	std::cerr << "exported " << changes.value("revisions", json::array()).size() << " revisions after " << from
	          << " (main is at " << opened.screen->revision() << ")\n";
	return Exit::Success;
}

Exit importChanges(std::span<const std::string_view> args) {
	const Arguments arguments(args, dataOption, {});
	if(arguments.positional().size() != 2) throw UsageError("expected a screen file and a change set");
	const std::filesystem::path file = arguments.positional()[0];
	const std::string changes = readFile(arguments.positional()[1]);

	OpenedScreen opened(file, arguments.value("--data"));
	if(!opened.writesToDisk())
		throw std::runtime_error("another program writes this screen's history (is a host showing it?): close it first");
	Screen& screen = *opened.screen;
	if(screen.hasImported(changes)) {
		std::cout << "already applied: the history has this change set (revision " << screen.revision() << ")\n";
		return Exit::Success;
	}
	if(std::string error; !screen.importChanges(changes, &error)) {
		std::cout << "refused: " << error << '\n';
		return Exit::Problems;
	}
	std::cout << "applied: the screen is now at revision " << screen.revision() << '\n';
	return Exit::Success;
}

} // namespace affresco::cli
