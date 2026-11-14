/// @file
/// @brief The conformance runner, `affrescoui_conformance` (README.md): runs test files of the Clojure suite against
/// the language, each in a fresh interpreter with low limits and the shim loaded, and prints one JSON object per file;
/// or, with `--everyday`, runs the everyday probes (everyday.lisp) and prints their results. The files are untrusted
/// text: each runs within its budget, reaches nothing but the primitives below, and reads no file but those given and
/// the suite's own support files under `--suite`.
#include "prepare.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lisp = affresco::lisp;
using affresco::json;
using affresco::conformance::PreparedFile;
using affresco::conformance::TopForm;

namespace {

/// @brief The largest file the runner reads: the suite's largest is far below.
constexpr std::size_t maxFileBytes = 4U << 20U;
/// @brief How long a text in a report may be, in bytes.
constexpr std::size_t maxReportText = 200;
/// @brief How many failures a file's object lists.
constexpr std::size_t maxFailures = 20;
/// @brief The prefix of the condition `(%out reason)` raises.
constexpr std::string_view outPrefix = "out of scope: ";
/// @brief The namespace of the suite's portability file, which the shim stands for.
constexpr std::string_view portability = "clojure.core-test.portability";

/// @brief The limits of a file's interpreter: low, so that a file that loops ends soon.
/// @return Them.
lisp::Limits fileLimits() {
	lisp::Limits limits;
	limits.budget = {.operations = 5'000'000, .bytes = 64U << 20U};
	limits.callDepth = 2000;
	return limits;
}

/// @brief A file's text.
/// @param path The path.
/// @return The text, or std::nullopt if it can't be read or is too large.
std::optional<std::string> readFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if(!in) return std::nullopt;
	std::string text;
	text.resize(maxFileBytes + 1);
	in.read(text.data(), static_cast<std::streamsize>(text.size()));
	text.resize(static_cast<std::size_t>(in.gcount()));
	if(text.size() > maxFileBytes) return std::nullopt;
	return text;
}

/// @brief A text cut to a report's length, never inside a UTF-8 character.
/// @param text The text.
/// @return It, cut.
std::string cut(std::string_view text) {
	if(text.size() <= maxReportText) return std::string(text);
	std::size_t end = maxReportText;
	while(end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) --end;
	return std::string(text.substr(0, end)) + "...";
}

/// @brief The reason in a condition `(%out reason)` raised, wherever its message carries it.
/// @param message The message.
/// @return The reason, or "".
std::string outReasonOf(std::string_view message) {
	const auto at = message.find(outPrefix);
	if(at == std::string_view::npos) return {};
	std::string reason(message.substr(at + outPrefix.size()));
	for(const std::string_view known : affresco::conformance::outReasons())
		if(reason.starts_with(known)) return std::string(known);
	return {};
}

/// @brief What one file gave.
struct FileResult {
	std::string file;                       ///< Its name, as given.
	bool read = false;                      ///< Every top-level form read.
	bool defined = false;                   ///< The function it tests is defined.
	std::string tested;                     ///< That function.
	std::size_t assertions = 0;             ///< The assertions written in it.
	std::size_t passed = 0;                 ///< Assertions that held.
	std::size_t failed = 0;                 ///< Assertions that didn't.
	std::size_t errors = 0;                 ///< Assertions that raised an error, and forms or tests that did.
	std::size_t reported = 0;               ///< Assertions that reported (passed, failed, raised or out).
	std::map<std::string, std::size_t> out; ///< Assertions out of scope, by reason.
	std::vector<json> failures;             ///< The first failures and errors.
	bool exhausted = false;                 ///< It ran past its budget.
	std::string error;                      ///< Why it couldn't be run at all, or "".

	/// @brief As the JSON object the runner prints. @return It.
	[[nodiscard]] json toJson() const {
		if(exhausted) return {{"file", file}, {"exhausted", true}, {"tested", tested}};
		std::size_t outCount = 0;
		for(const auto& [reason, count] : out) outCount += count;
		const std::size_t notRun = assertions > reported ? assertions - reported : 0;
		json result = {{"file", file},           {"read", read},     {"defined", defined}, {"tested", tested},
		               {"written", assertions},  {"passed", passed}, {"failed", failed},   {"errors", errors},
		               {"outOfScope", outCount}, {"out", out},       {"notRun", notRun},   {"failures", failures}};
		if(!error.empty()) result["error"] = error;
		return result;
	}
};

/// @brief Runs one file of the suite: the state the primitives report into.
class FileRun {
public:
	/// @brief Starts on a file.
	/// @param name Its name, for the report.
	/// @param shim The shim's text.
	/// @param suite The suite's test folder, for support files, or "".
	/// @param options What the language reads.
	FileRun(std::string name, const std::string& shim, std::string suite, const affresco::conformance::Options& options)
	    : interpreter_(fileLimits()), shim_(shim), suite_(std::move(suite)), options_(options) {
		result_.file = std::move(name);
	}

	/// @brief Runs the file. @param text Its text. @return What it gave.
	FileResult run(std::string_view text) {
		try {
			const PreparedFile file = affresco::conformance::prepare(text, options_);
			result_.assertions = file.assertions;
			result_.tested = file.tested;
			if(!file.error.empty()) {
				result_.error = file.error;
				return result_;
			}
			result_.read = std::ranges::all_of(file.forms, [this](const TopForm& form) { return readable(form); });
			if(file.unreadLine != 0 && result_.read) {
				result_.read = false;
				result_.error = "line " + std::to_string(file.unreadLine) + ": the reader doesn't take " + file.unread;
			}
			setUp(file);
			for(const TopForm& form : file.forms) runTopForm(form);
			for(const std::string& test : file.tests) runTest(test, file);
		} catch(const Exhausted&) {
			result_.exhausted = true;
		} catch(const std::exception& problem) {
			result_.error = cut(problem.what());
		}
		return result_;
	}

private:
	/// @brief Thrown when the file ran past its budget: the run of the file ends.
	struct Exhausted {};

	/// @brief Whether the language's reader reads a top-level form, as prepared; the first that doesn't is recorded.
	/// @param form The form.
	/// @return true if it reads.
	bool readable(const TopForm& form) {
		try {
			(void)lisp::formsOf(form.text, result_.file);
			return true;
		} catch(const lisp::LispError& error) {
			if(result_.error.empty()) result_.error = "line " + std::to_string(form.line) + ": " + cut(error.message());
			return false;
		}
	}

	/// @brief Makes the interpreter ready: the namespaces, the primitives, the shim, the file's aliases.
	/// @param file The file, prepared.
	void setUp(const PreparedFile& file) {
		interpreter_.addNamespace("clojure.test", true);
		interpreter_.addNamespace(std::string(portability), true);
		definePrimitives();
		interpreter_.eval(shim_, "shim.lisp");
		for(const auto& requirement : file.requirements) {
			if(requirement.library.starts_with("clojure.core-test.") && requirement.library != portability)
				loadSupport(requirement.library);
			if(requirement.alias.empty()) continue;
			try {
				interpreter_.addAlias(requirement.alias, requirement.library);
			} catch(const std::invalid_argument&) {
				interpreter_.addNamespace(requirement.library, false);
				interpreter_.addAlias(requirement.alias, requirement.library);
			}
		}
		interpreter_.setBudget(fileLimits().budget); // the shim's loading isn't the file's
		interpreter_.workOutDefinitions();
	}

	/// @brief Loads one of the suite's support files (`clojure.core-test.number-range`), whose definitions the file
	/// uses through an alias. Its name must be a plain one, and its file is looked for under the suite's folder only.
	/// @param library Its name.
	void loadSupport(const std::string& library) {
		const bool plain = std::ranges::all_of(
		    library, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-'; });
		if(suite_.empty() || !plain || library.find("..") != std::string::npos) return;
		std::string path = library;
		std::ranges::replace(path, '.', '/');
		std::ranges::replace(path, '-', '_');
		const auto text = readFile(suite_ + "/" + path + ".cljc");
		if(!text) return;
		interpreter_.addNamespace(library, true);
		const PreparedFile support = affresco::conformance::prepare(*text, options_);
		for(const TopForm& form : support.forms) evaluate(form, "support", true);
	}

	/// @brief Defines the primitives the shim calls: `%report`, `%out`, `%defined?`, `%absent`, `%enter`,
	/// `%leave`.
	void definePrimitives() {
		interpreter_.definePrimitive("%report", {.call = [this](lisp::Interpreter& in, std::span<const lisp::Value> a) {
			                             report(in, a);
			                             return lisp::Value();
		                             }});
		interpreter_.definePrimitive("%out",
		                             {.pure = true, .call = [](lisp::Interpreter&, std::span<const lisp::Value> a) {
			                              const std::string reason(a.empty() ? "" : a[0].text());
			                              throw lisp::Refusal(std::string(outPrefix) + reason);
			                              return lisp::Value();
		                              }});
		interpreter_.definePrimitive("%defined?",
		                             {.pure = true, .call = [this](lisp::Interpreter&, std::span<const lisp::Value> a) {
			                              return lisp::Value::boolean(!a.empty() && defines(a[0].text()));
		                              }});
		interpreter_.definePrimitive(
		    "%absent", {.call = [](lisp::Interpreter&, std::span<const lisp::Value>) { return lisp::Value(); }});
		interpreter_.definePrimitive("%enter", {.call = [this](lisp::Interpreter&, std::span<const lisp::Value> a) {
			                             contexts_.emplace_back(a.empty() ? "" : a[0].text());
			                             return lisp::Value();
		                             }});
		interpreter_.definePrimitive("%leave", {.call = [this](lisp::Interpreter&, std::span<const lisp::Value> a) {
			                             if(!contexts_.empty()) contexts_.pop_back();
			                             return a.empty() ? lisp::Value() : a[0];
		                             }});
	}

	/// @brief Whether a name is defined: a function, a macro or a form of the language, or a definition of the file,
	/// written bare or with a namespace (`clojure.core/inc`, `str/blank?`). The name alone is evaluated in a fresh
	/// interpreter, where only a name that isn't defined fails with "is not defined" (a form fails as "not a value").
	/// @param word The name: a word of the file, never code.
	/// @return true if it is.
	bool defines(std::string_view word) {
		const std::string name(word);
		if(name.empty() || name.find_first_of("()[]{}\"\\;'`~@^# \t\n") != std::string::npos) return false;
		if(interpreter_.global(name).has_value()) return true;
		try {
			if(!probe_) probe_ = std::make_unique<lisp::Interpreter>(fileLimits());
			(void)probe_->evaluate(name, "probe", "");
			return true;
		} catch(const lisp::LispError& error) {
			return error.message().find("is not defined") == std::string::npos;
		} catch(const std::exception&) {
			return false;
		}
	}

	/// @brief Takes one assertion's report: (line status a b).
	/// @param interpreter The interpreter.
	/// @param arguments Its arguments.
	void report(lisp::Interpreter& interpreter, std::span<const lisp::Value> arguments) {
		if(arguments.size() < 2) return;
		const std::string status(arguments[1].text());
		const auto textAt = [&](std::size_t index) {
			if(index >= arguments.size()) return std::string();
			const lisp::Value& value = arguments[index];
			return cut(value.kind() == lisp::Kind::String ? std::string(value.text()) : interpreter.print(value));
		};
		++result_.reported;
		if(status == "pass") ++result_.passed;
		else if(status == "out") ++result_.out[textAt(2)];
		else {
			++(status == "fail" ? result_.failed : result_.errors);
			if(result_.failures.size() < maxFailures) {
				json failure = {{"test", test_}, {"line", arguments[0].numberAsJson()}, {"status", status}};
				if(!contexts_.empty()) failure["testing"] = contexts_;
				if(status == "fail") failure["expected"] = textAt(2), failure["actual"] = textAt(3);
				else failure["message"] = textAt(2);
				result_.failures.push_back(std::move(failure));
			}
		}
	}

	/// @brief Runs a top-level form: `when-var-exists`'s body form by form when its name is defined, anything else as
	/// written.
	/// @param form The form.
	void runTopForm(const TopForm& form) {
		if(form.head != "when-var-exists") {
			evaluate(form, "", false);
			return;
		}
		if(!defines(form.name)) return; // its assertions are not run
		if(form.name == result_.tested) result_.defined = true;
		for(const TopForm& inner : form.body) evaluate(inner, "", false);
	}

	/// @brief Evaluates a form at the top level, on its own line, so an error names its place.
	/// @param form The form.
	/// @param file The file's name for errors, or "" for this file's.
	/// @param quiet Whether an error is ignored (a support file's form for another platform).
	void evaluate(const TopForm& form, const std::string& file, bool quiet) {
		const std::size_t before = result_.reported;
		try {
			(void)interpreter_.eval(std::string(form.line - 1, '\n') + form.text, file.empty() ? result_.file : file);
		} catch(const lisp::LispError& error) {
			if(error.exhausted()) throw Exhausted{};
			// a test's assertions run when it is called, not when its deftest is evaluated
			if(!quiet) aborted(error.message(), form.head == "deftest" ? 0 : form.assertions, before, form.line);
		}
	}

	/// @brief Calls one test, when its `deftest` defined it.
	/// @param name Its name.
	/// @param file The file, for the assertions the test holds.
	void runTest(const std::string& name, const PreparedFile& file) {
		std::size_t written = 0;
		for(const TopForm& form : file.forms) written += assertionsOfTest(form, name);
		const std::size_t before = result_.reported;
		test_ = name;
		contexts_.clear();
		try {
			const auto function = interpreter_.global("test--" + name);
			if(function) (void)interpreter_.call(*function, {});
		} catch(const lisp::LispError& error) {
			if(error.exhausted()) throw Exhausted{};
			aborted(error.message(), written, before, error.line());
		}
		test_.clear();
	}

	/// @brief The assertions written in a test's `deftest`, wherever it is among the top-level forms.
	/// @param form A top-level form.
	/// @param name The test's name.
	/// @return Their count.
	static std::size_t assertionsOfTest(const TopForm& form, const std::string& name) {
		if(form.head == "deftest" && form.name == name) return form.assertions;
		std::size_t count = 0;
		for(const TopForm& inner : form.body) count += assertionsOfTest(inner, name);
		return count;
	}

	/// @brief Records a form or a test that stopped with an error before its assertions all reported: those that
	/// didn't are out of scope when `(%out reason)` stopped it, else not run, and the error is one error.
	/// @param message The error's message.
	/// @param written The assertions written in it.
	/// @param before How many had reported when it began.
	/// @param line Where.
	void aborted(const std::string& message, std::size_t written, std::size_t before, std::size_t line) {
		const std::size_t done = result_.reported - before;
		const std::size_t left = written > done ? written - done : 0;
		if(const std::string reason = outReasonOf(message); !reason.empty()) {
			result_.out[reason] += left;
			result_.reported += left;
			return;
		}
		++result_.errors;
		if(result_.failures.size() < maxFailures)
			result_.failures.push_back(
			    {{"test", test_}, {"line", line}, {"status", "error"}, {"message", cut(message)}, {"notRun", left}});
	}

	lisp::Interpreter interpreter_;            ///< The file's interpreter.
	std::unique_ptr<lisp::Interpreter> probe_; ///< A fresh interpreter, to tell the language's names: see defines().
	const std::string& shim_;                  ///< The shim's text; outlives it.
	std::string suite_;                        ///< The suite's test folder, or "".
	affresco::conformance::Options options_;   ///< What the language reads.
	FileResult result_;                        ///< What it gave so far.
	std::string test_;                         ///< The test running, or "".
	std::vector<std::string> contexts_;        ///< The `testing` texts around the assertion running.
};

/// @brief Whether the language's reader takes a set `#{...}`. @return true if it does.
bool readsSets() {
	try {
		(void)lisp::formsOf("#{1}", "probe");
		return true;
	} catch(const lisp::LispError&) {
		return false;
	}
}

/// @brief Prints how to call the program. @return The exit status for a wrong call.
int usage() {
	std::cerr << "usage: affrescoui_conformance --shim shim.lisp [--suite TESTDIR] FILE...\n"
	             "       affrescoui_conformance --everyday everyday.lisp [--min N]\n";
	return 2;
}

/// @brief Runs the files given, printing one JSON object per line.
/// @param shimPath The shim's path.
/// @param suite The suite's test folder, or "".
/// @param files The files.
/// @return The exit status: 0, or 1 if the shim can't be read.
int runFiles(const std::string& shimPath, const std::string& suite, const std::vector<std::string>& files) {
	const auto shim = readFile(shimPath);
	if(!shim) {
		std::cerr << "affrescoui_conformance: can't read " << shimPath << "\n";
		return 1;
	}
	const affresco::conformance::Options options{.readsSets = readsSets()};
	for(const std::string& path : files) {
		std::string name = path;
		if(!suite.empty() && name.starts_with(suite + "/")) name = name.substr(suite.size() + 1);
		const auto text = readFile(path);
		FileResult result;
		if(text) result = FileRun(name, *shim, suite, options).run(*text);
		else result.file = name, result.error = "can't be read";
		std::cout << result.toJson().dump() << "\n" << std::flush;
	}
	return 0;
}

/// @brief One probe of everyday.lisp, and what it gave.
struct Probe {
	std::string kind;   ///< `probe`, `absent` or `differs`.
	std::size_t line;   ///< Its line.
	std::string form;   ///< Its form, printed.
	bool holds = false; ///< A probe gave Clojure's value; a difference gave ours. An absent form always holds.
	std::string detail; ///< The value it gave, or the error it raised.
	std::string reason; ///< Why it is absent or differs.
};

/// @brief Runs the everyday probes (everyday.lisp), one top-level form at a time, and prints what each gave.
/// @param path The file.
/// @param minimum How many probes must give Clojure's value.
/// @return 0 when at least that many do and every difference gives our value, else 1.
int runEveryday(const std::string& path, std::size_t minimum) {
	const auto text = readFile(path);
	if(!text) {
		std::cerr << "affrescoui_conformance: can't read " << path << "\n";
		return 1;
	}
	const PreparedFile file = affresco::conformance::prepare(*text, {.markOut = false});
	if(!file.error.empty()) {
		std::cerr << "affrescoui_conformance: " << path << ": " << file.error << "\n";
		return 1;
	}
	std::vector<Probe> probes;
	std::size_t line = 0;
	lisp::Interpreter interpreter(fileLimits());
	interpreter.definePrimitive(
	    "%everyday", {.call = [&](lisp::Interpreter& in, std::span<const lisp::Value> a) {
		    const auto textAt = [&](std::size_t index) {
			    if(index >= a.size()) return std::string();
			    return cut(a[index].kind() == lisp::Kind::String ? std::string(a[index].text()) : in.print(a[index]));
		    };
		    probes.push_back({textAt(0), line, textAt(1), a.size() > 2 && a[2].truthy(), textAt(3), textAt(4)});
		    return lisp::Value();
	    }});
	interpreter.definePrimitive(
	    "%run-text", {.call = [](lisp::Interpreter& in, std::span<const lisp::Value> a) {
		    // A form and its expected value written as text, read and run in an
		    // interpreter of their own: [equal printed], or [false message]
		    const std::string form(a.empty() ? "" : a[0].text());
		    const std::string expected(a.size() < 2 ? "" : a[1].text());
		    try {
			    lisp::Interpreter alone(fileLimits());
			    const lisp::Value outcome = alone.evaluate("(let [cf--value " + form + "\n] [(= cf--value " + expected +
				                                               "\n) (pr-str cf--value)])",
				                                           "probe", "");
			    const std::array<lisp::Value, 2> items{lisp::Value::boolean(outcome.at(0).truthy()),
				                                       in.makeString(outcome.at(1).text())};
			    return in.makeVector(items);
		    } catch(const lisp::LispError& error) {
			    const std::array<lisp::Value, 2> items{lisp::Value::boolean(false),
				                                       in.makeString(cut(error.message()))};
			    return in.makeVector(items);
		    }
	    }});
	for(const TopForm& form : file.forms) {
		line = form.line;
		const std::size_t before = probes.size();
		try {
			interpreter.setBudget(fileLimits().budget);
			(void)interpreter.eval(std::string(form.line - 1, '\n') + form.text, path);
		} catch(const lisp::LispError& error) {
			const bool probe =
			    form.head == "probe" || form.head == "probe-text" || form.head == "differs" || form.head == "absent";
			if(!probe) {
				std::cerr << "affrescoui_conformance: " << error.what() << "\n";
				return 1;
			}
			if(probes.size() == before)
				probes.push_back(
				    {form.head, form.line, cut(form.text), form.head == "absent", cut(error.message()), ""});
		}
	}
	std::size_t clojure = 0;
	std::size_t absent = 0;
	std::size_t differs = 0;
	bool ok = true;
	for(const Probe& probe : probes) {
		if(probe.kind == "probe" && probe.holds) ++clojure;
		if(probe.kind == "absent") ++absent;
		if(probe.kind == "differs") ++differs;
		if(probe.holds) continue;
		if(probe.kind == "differs") ok = false;
		std::cout << path << ":" << probe.line << ": " << (probe.kind == "probe" ? "gap" : "difference no longer holds")
		          << ": " << probe.form << (probe.detail.empty() ? "" : " gave " + probe.detail) << "\n";
	}
	std::cout << json{{"probes", probes.size()},
	                  {"clojure", clojure},
	                  {"absent", absent},
	                  {"differs", differs},
	                  {"gaps", probes.size() - clojure - absent - differs},
	                  {"minimum", minimum}}
	                 .dump()
	          << "\n";
	return ok && clojure >= minimum ? 0 : 1;
}

} // namespace

/// @brief The program: see usage().
/// @param count The arguments' count.
/// @param arguments The arguments.
/// @return 0, 1 on a problem, 2 on a wrong call.
int main(int count, char** arguments) {
	try {
		const std::vector<std::string> args(arguments + 1, arguments + count);
		std::string shim;
		std::string suite;
		std::vector<std::string> files;
		std::string everyday;
		std::size_t minimum = 170; // of everyday.lisp's 186 probes
		for(std::size_t at = 0; at < args.size(); ++at) {
			if(args[at] == "--everyday" && at + 1 < args.size()) everyday = args[++at];
			else if(args[at] == "--min" && at + 1 < args.size()) minimum = std::stoul(args[++at]);
			else if(args[at] == "--shim" && at + 1 < args.size()) shim = args[++at];
			else if(args[at] == "--suite" && at + 1 < args.size()) suite = args[++at];
			else files.push_back(args[at]);
		}
		if(!everyday.empty()) return runEveryday(everyday, minimum);
		if(shim.empty()) return usage();
		return runFiles(shim, suite, files);
	} catch(const std::exception& problem) {
		std::cerr << "affrescoui_conformance: " << problem.what() << "\n";
		return 1;
	}
}
