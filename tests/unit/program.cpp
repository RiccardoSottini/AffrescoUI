/// @file
/// @brief The services module's runProgram(): a program run once, its input whole, its output bounded, a time limit and
/// a way to stop it; each way it can end.

#include <affresco/services/Program.hpp>

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <string>

#ifndef _WIN32

using affresco::services::Launch;
using affresco::services::ProgramRun;
using namespace std::chrono_literals;

namespace {

/// @brief A shell command, with no shell of the host's: /bin/sh -c.
/// @param command The command.
/// @return Its launch.
Launch shell(const std::string& command) {
	Launch launch;
	launch.program = "/bin/sh";
	launch.arguments = {"-c", command};
	return launch;
}

} // namespace

TEST_CASE("program: the input goes in whole, the output and the status come back") {
	const ProgramRun run = affresco::services::runProgram(shell("cat; exit 3"), "hello", 10s, {}, 1024);
	CHECK(run.end == ProgramRun::End::Exited);
	CHECK(run.status == 3);
	CHECK(run.output == "hello");
}

TEST_CASE("program: one that can't start, one too slow, one stopped and one that prints too much each say so") {
	Launch missing;
	missing.program = "/nowhere/no-such-program";
	CHECK(affresco::services::runProgram(missing, "", 10s, {}, 1024).end == ProgramRun::End::NotStarted);

	const auto started = std::chrono::steady_clock::now();
	CHECK(affresco::services::runProgram(shell("sleep 30"), "", 300ms, {}, 1024).end == ProgramRun::End::TimedOut);
	CHECK(std::chrono::steady_clock::now() - started < 10s);

	std::atomic<int> asked = 0;
	const ProgramRun stopped =
	    affresco::services::runProgram(shell("sleep 30"), "", 30s, [&asked] { return ++asked > 2; }, 1024);
	CHECK(stopped.end == ProgramRun::End::Stopped);

	const ProgramRun large = affresco::services::runProgram(shell("yes"), "", 30s, {}, 4096);
	CHECK(large.end == ProgramRun::End::TooLarge);
	CHECK(large.output.size() <= 4096);
}

#endif
