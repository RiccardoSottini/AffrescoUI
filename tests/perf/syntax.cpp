/// @file
/// @brief The "syntax" workloads: the syntax engine (affresco/services/Syntax.hpp) on a generated C++ file of 100,000
/// lines: opened (parsed and its first screen highlighted), and one keystroke in its middle, which parses again only
/// what the edit changed and highlights the screen around it. No window.
///
/// With AFFRESCOUI_PERF_BUDGETS set, a keystroke whose 99th percentile is over 8 ms (key to colours) fails the run.

#include "Bench.hpp"

#include <affresco/services/Syntax.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco::bench {
namespace {

constexpr std::uint32_t fileLines = 100'000; ///< The file's lines.
constexpr std::uint32_t screenRows = 64;     ///< The lines a screen shows.
constexpr double keystrokeBudget = 8;        ///< A keystroke's 99th percentile, in milliseconds.

/// @brief The file: functions of ten lines, the same on every machine.
/// @return Its text.
std::string generated() {
	std::string text;
	for(std::uint32_t line = 0; line < fileLines; line += 10)
		text += "// Adds up part " + std::to_string(line) + "\nint part" + std::to_string(line) +
		        "(int count) {\n\tint total = 0;\n\tfor(int i = 0; i < count; ++i) {\n\t\ttotal += i * 2;\n"
		        "\t}\n\tconst char* name = \"part\";\n\treturn total + static_cast<int>(name[0]);\n}\n\n";
	return text;
}

/// @brief Reads a text kept whole.
/// @param text The text.
services::SyntaxReader readerOf(const std::string& text) {
	return [&text](std::size_t byte, services::SyntaxPoint) { return std::string_view(text).substr(byte); };
}

/// @brief The file, parsed, and the keystrokes typed into it.
struct Typed {
	std::string text = generated();                              ///< The text.
	std::unique_ptr<services::SyntaxDocument> document;          ///< It, parsed.
	std::vector<services::SyntaxSpan> spans;                     ///< The last highlight, reused.
	std::vector<double> samples;                                 ///< Each keystroke's time, in milliseconds.
	bool typed = false;                                          ///< The next keystroke erases the last.

	/// @brief Parses the text from scratch.
	void open() {
		document = services::SyntaxDocument::create("cpp");
		if(document->parse(readerOf(text), text.size()) != services::SyntaxParse::Parsed)
			throw std::runtime_error("syntax: the file wasn't parsed");
		highlight(0);
	}

	/// @brief Highlights a screen from a line.
	/// @param first Its first line.
	void highlight(std::uint32_t first) {
		spans.clear();
		document->highlight(readerOf(text), first, first + screenRows - 1, spans);
		if(spans.empty()) throw std::runtime_error("syntax: nothing highlighted");
	}

	/// @brief Types "x" into the middle line's `total` (or takes it out again), parses and highlights its screen.
	void keystroke() {
		const auto start = std::chrono::steady_clock::now();
		constexpr std::uint32_t row = fileLines / 2 + 2; // "\tint total = 0;"
		std::size_t lineStart = 0;
		for(std::uint32_t line = 0; line < row; ++line) lineStart = text.find('\n', lineStart) + 1;
		const auto at = static_cast<std::uint32_t>(lineStart + 5), column = std::uint32_t{5};
		services::SyntaxEdit edit{.startByte = at, .oldEndByte = at, .newEndByte = at, .start = {row, column},
		                          .oldEnd = {row, column}, .newEnd = {row, column}};
		if(typed) {
			text.erase(at, 1);
			edit.oldEndByte = at + 1;
			edit.oldEnd.column = column + 1;
		} else {
			text.insert(at, 1, 'x');
			edit.newEndByte = at + 1;
			edit.newEnd.column = column + 1;
		}
		typed = !typed;
		document->edit(edit);
		std::vector<services::SyntaxRows> changed;
		if(document->parse(readerOf(text), text.size(), &changed) != services::SyntaxParse::Parsed)
			throw std::runtime_error("syntax: a keystroke went over its parse budget");
		highlight(row - screenRows / 2);
		samples.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	}

	/// @brief Fails the run if the keystrokes' 99th percentile is over budget and budgets are on.
	void checkBudget() {
		if(std::getenv("AFFRESCOUI_PERF_BUDGETS") == nullptr || samples.empty()) return;
		std::ranges::sort(samples);
		const double p99 = samples[std::min(samples.size() - 1, samples.size() * 99 / 100)];
		samples.clear();
		if(p99 > keystrokeBudget)
			throw std::runtime_error("syntax/type-100k: 99th percentile " + std::to_string(p99) +
			                         " ms, over its budget of " + std::to_string(keystrokeBudget) + " ms");
	}
};

} // namespace

std::vector<Workload> syntaxWorkloads(Context&) {
	const auto typed = std::make_shared<Typed>();
	typed->open();
	std::vector<Workload> workloads;
	workloads.push_back({.name = "syntax/open-100k", .unit = "ms", .runs = 5, .step = [typed] { typed->open(); }});
	constexpr int keystrokes = 50;
	workloads.push_back({.name = "syntax/type-100k",
	                     .unit = "ms",
	                     .operations = keystrokes,
	                     .step = [typed] {
		                     for(int i = 0; i < keystrokes; ++i) typed->keystroke();
		                     typed->checkBudget();
	                     }});
	return workloads;
}

} // namespace affresco::bench
