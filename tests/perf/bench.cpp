/// @file
/// @brief affrescoui_bench: runs the workloads and prints one JSON object per workload (tests/perf/README.md).
///
/// Usage: affrescoui_bench [--only a,b] [--skip a,b] [--runs n] [--callgrind] [--screens dir] [--edits dir]
///
/// Each workload runs once to warm up, then a number of measured runs; the value is the median. With --callgrind
/// (under `valgrind --tool=callgrind --instr-atstart=no`), each workload has one measured run, the only code
/// callgrind instruments, and its counts are dumped under the workload's name: run.py reads them.

#include "Bench.hpp"

#if defined(__has_include) && __has_include(<valgrind/callgrind.h>)
#if defined(__GNUC__)
#pragma GCC diagnostic push // the macros are C: casts, and conversions between integer types
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <valgrind/callgrind.h>
#define AFFRESCOUI_BENCH_CALLGRIND 1
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using affresco::bench::Workload;

#if defined(AFFRESCOUI_BENCH_CALLGRIND)
/// @brief Whether this build can tell callgrind where a measured run starts and ends.
constexpr bool callgrindAvailable = true;

/// @brief Starts counting: instruments the code from here on, from zero.
void startCounting() {
	CALLGRIND_START_INSTRUMENTATION;
	CALLGRIND_ZERO_STATS;
}

/// @brief Stops counting, and writes the counts since startCounting() to a dump named after the workload.
/// @param name The workload.
void stopCounting(const std::string& name) {
	CALLGRIND_DUMP_STATS_AT(name.c_str());
	CALLGRIND_STOP_INSTRUMENTATION;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#else
constexpr bool callgrindAvailable = false; ///< Built without valgrind's header: --callgrind is refused.
/// @brief Does nothing: built without valgrind's header.
void startCounting() {}
/// @brief Does nothing: built without valgrind's header.
void stopCounting(const std::string&) {}
#endif

/// @brief What the command line asked for.
struct Options {
	std::vector<std::string> only; ///< Run only the workloads whose names start with one of these.
	std::vector<std::string> skip; ///< Leave out the workloads whose names start with one of these.
	int runs = 0;                  ///< Measured runs for every workload, instead of each one's own (0: its own).
	bool callgrind = false;        ///< One counted run per workload, for callgrind.
	std::filesystem::path screens = AFFRESCOUI_EXAMPLES_ASSETS "/screens"; ///< The template screens.
	std::filesystem::path edits = AFFRESCOUI_PERF_DIR "/edits";            ///< The canonical agent edits.
};

/// @brief Splits a comma-separated list.
/// @param text E.g. "update,frame".
/// @return Its items.
std::vector<std::string> splitList(std::string_view text) {
	std::vector<std::string> items;
	std::stringstream stream{std::string(text)};
	for(std::string item; std::getline(stream, item, ',');)
		if(!item.empty()) items.push_back(item);
	return items;
}

/// @brief Reads the command line.
/// @param arguments The arguments, without the program's name.
/// @return The options.
/// @throws std::invalid_argument If an argument is unknown or lacks its value.
Options parseOptions(const std::vector<std::string_view>& arguments) {
	Options options;
	for(std::size_t i = 0; i < arguments.size(); ++i) {
		const std::string_view argument = arguments[i];
		if(argument == "--callgrind") {
			options.callgrind = true;
			continue;
		}
		if(i + 1 == arguments.size()) throw std::invalid_argument("missing value after " + std::string(argument));
		const std::string_view value = arguments[++i];
		if(argument == "--only") options.only = splitList(value);
		else if(argument == "--skip") options.skip = splitList(value);
		else if(argument == "--runs") options.runs = std::stoi(std::string(value));
		else if(argument == "--screens") options.screens = value;
		else if(argument == "--edits") options.edits = value;
		else throw std::invalid_argument("unknown argument " + std::string(argument));
	}
	return options;
}

/// @brief Whether a name starts with one of the prefixes.
/// @param name A workload's name.
/// @param prefixes The prefixes.
/// @return true if one matches.
bool startsWithAny(std::string_view name, const std::vector<std::string>& prefixes) {
	return std::ranges::any_of(prefixes, [name](const std::string& prefix) { return name.starts_with(prefix); });
}

/// @brief Whether the options select a workload.
/// @param options The options.
/// @param name Its name.
/// @return true to run it.
bool selected(const Options& options, std::string_view name) {
	return (options.only.empty() || startsWithAny(name, options.only)) && !startsWithAny(name, options.skip);
}

/// @brief Whether the options may select one of a group's workloads, so it is worth setting the group up.
/// @param options The options.
/// @param prefix The group's prefix.
/// @return true to create its workloads.
bool groupSelected(const Options& options, const std::string& prefix) {
	const bool wanted =
	    options.only.empty() || startsWithAny(prefix, options.only) ||
	    std::ranges::any_of(options.only, [&](const std::string& only) { return only.starts_with(prefix); });
	return wanted && !startsWithAny(prefix, options.skip);
}

/// @brief The time now, in nanoseconds.
/// @param cpuTime The main thread's CPU time instead of the wall clock.
/// @return The time, from an arbitrary start.
std::int64_t now(bool cpuTime) {
#if !defined(_WIN32)
	if(cpuTime) {
		timespec time{};
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
		return static_cast<std::int64_t>(time.tv_sec) * 1'000'000'000 + time.tv_nsec;
	}
#else
	static_cast<void>(cpuTime);
#endif
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

/// @brief The median of some numbers.
/// @param values The numbers; not empty.
/// @return Their median.
double median(std::vector<double> values) {
	std::ranges::sort(values);
	const std::size_t middle = values.size() / 2;
	return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

/// @brief Rounds a number for the output.
/// @param value The number.
/// @param decimals Decimals to keep.
/// @return The rounded number.
double rounded(double value, int decimals) {
	const double scale = std::pow(10.0, decimals);
	return std::round(value * scale) / scale;
}

/// @brief Nanoseconds in one of a workload's units.
/// @param unit "ns", "us" or "ms".
/// @return The factor.
/// @throws std::invalid_argument If the unit is unknown.
double nanosecondsIn(const std::string& unit) {
	if(unit == "ns") return 1.0;
	if(unit == "us") return 1e3;
	if(unit == "ms") return 1e6;
	throw std::invalid_argument("unknown unit '" + unit + "'");
}

/// @brief Measures a workload: a warm-up run, then its measured runs.
/// @param workload The workload.
/// @param options The options.
/// @return Its result, as one JSON object.
affresco::json measure(const Workload& workload, const Options& options) {
	const int runs = options.callgrind ? 1 : options.runs > 0 ? options.runs : workload.runs;
	const double operations = workload.operations;
	if(workload.prepare) workload.prepare();
	workload.step(); // warm up: caches, compiled expressions, lazily created objects

	std::vector<double> times, allocations, retained;
	for(int run = 0; run < runs; ++run) {
		if(workload.prepare) workload.prepare();
		const std::uint64_t allocationsBefore = affresco::bench::allocationCount();
		const std::int64_t bytesBefore = affresco::bench::liveBytes();
		if(options.callgrind) startCounting();
		const std::int64_t start = now(workload.cpuTime);
		workload.step();
		const std::int64_t end = now(workload.cpuTime);
		if(options.callgrind) stopCounting(workload.name);
		times.push_back(static_cast<double>(end - start) / operations);
		allocations.push_back(static_cast<double>(affresco::bench::allocationCount() - allocationsBefore) / operations);
		retained.push_back(static_cast<double>(affresco::bench::liveBytes() - bytesBefore) / operations);
	}
	return {{"name", workload.name},
	        {"value", rounded(median(times) / nanosecondsIn(workload.unit), 3)},
	        {"unit", workload.unit},
	        {"runs", runs},
	        {"operations", workload.operations},
	        {"allocations", rounded(median(allocations), 2)},
	        {"retainedBytes", rounded(median(retained), 1)}};
}

/// @brief Runs the selected workloads of every group, printing each result as it comes.
/// @param options The options.
/// @param context What the groups share.
/// @return EXIT_SUCCESS, or EXIT_FAILURE if a workload failed.
int runAll(const Options& options, affresco::bench::Context& context) {
	std::vector<affresco::bench::Group> groups = affresco::bench::engineGroups();
	std::ranges::move(affresco::bench::screenGroups(), std::back_inserter(groups));

	int status = EXIT_SUCCESS;
	auto fail = [&status](const std::string& where, const std::exception& error) {
		std::cerr << "affrescoui_bench: " << where << ": " << error.what() << '\n';
		status = EXIT_FAILURE;
	};
	for(const affresco::bench::Group& group : groups) {
		if(!groupSelected(options, group.prefix)) continue;
		std::vector<Workload> workloads;
		try {
			workloads = group.create(context);
		} catch(const std::exception& error) {
			fail(group.prefix, error);
		}
		for(const Workload& workload : workloads) {
			if(!selected(options, workload.name)) continue;
			try {
				std::cout << measure(workload, options).dump() << std::endl; // flushed: a later crash keeps it
			} catch(const std::exception& error) {
				fail(workload.name, error);
			}
		}
	}
	return status;
}

} // namespace

int main(int argc, char** argv) {
	// Every message to stderr: stdout carries only the results
	affresco::setLogSink([](affresco::LogLevel, std::string_view message) { std::cerr << message << '\n'; });
	try {
		const Options options = parseOptions(std::vector<std::string_view>(argv + 1, argv + argc));
		if(options.callgrind && !callgrindAvailable)
			throw std::invalid_argument("--callgrind needs a build that found valgrind/callgrind.h");
		affresco::bench::Context context(options.screens, options.edits);
		const int status = runAll(options, context);
		// Ends without tearing the windows down. The Application owns them until it goes, after their screens; on
		// macOS, closing one window redraws the others, whose screens are already gone.
		std::cout.flush();
		std::_Exit(status);
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_bench: " << error.what() << '\n'
		          << "usage: affrescoui_bench [--only a,b] [--skip a,b] [--runs n] [--callgrind] [--screens dir] "
		             "[--edits dir]\n";
		return 2;
	}
}
