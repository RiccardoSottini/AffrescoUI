/// @file
/// @brief format::Programs: loading a document's program, calling its functions, and the checks by author.

#include "Programs.hpp"

#include "Engine.hpp"

#include "affresco/core/Log.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace affresco::format {

namespace {

/// The steps any author's code may run: what a revision may add to a handler by itself. The record steps are among them
/// as a revision that adds code using one is always reviewed by the person (DataRules.cpp). What isn't here is a
/// control step, so a step added later is refused until someone says otherwise.
constexpr auto freeSteps = std::to_array<std::string_view>(
    {"set",      "push",    "remove",    "toggle", "increment", "if",       "forEach",    "while",       "run",
	 "stop",     "note",    "log",       "focus",  "scrollTo",  "navigate", "edit",       "press",       "after",
	 "debounce", "animate", "copy",      "emit",   "cancel",    "camera",   "openWindow", "closeWindow", "lisp",
	 "audio",    "dismiss", "transform", "insert", "update",    "delete",   "undoData",   "redoData"});

/// The steps that decide on changes: only ever from the person's own input.
constexpr auto reviewSteps =
    std::to_array<std::string_view>({"accept", "reject", "preview", "endPreview", "restore", "revert", "undo"});

/// @brief Who a program's entry that nobody is credited with counts as: no one, with the least authority.
constexpr std::string_view unknownAuthor = "unknown";

/// @brief Credits the entries of a program no author is known for, so that none runs as if nobody wrote it.
/// @param section The section.
/// @param authors The authors.
void creditTheRest(const json& section, json& authors) {
	if(!authors.is_object()) authors = json::object();
	for(const auto& [file, entries] : section.items()) {
		json& ofFile = authors[file];
		if(!ofFile.is_object()) ofFile = json::object();
		for(const auto& [name, text] : entries.items())
			if(!ofFile.contains(name) || !ofFile[name].is_string() || ofFile[name].empty())
				ofFile[name] = unknownAuthor;
	}
}

} // namespace

StepKind stepKind(std::string_view name) {
	if(std::ranges::find(reviewSteps, name) != reviewSteps.end()) return StepKind::Review;
	if(std::ranges::find(freeSteps, name) != freeSteps.end()) return StepKind::Free;
	return builtInSteps()->find(name) ? StepKind::Control : StepKind::None;
}

std::string_view stepOfWord(std::string_view word) noexcept {
	if(word == "remove-state") return "remove";
	if(word == "update-record") return "update";
	return word;
}

bool isPersonsAuthor(std::string_view author) noexcept {
	return author == "file" || author == "user" || author.starts_with("user/");
}

AuthorityScope::AuthorityScope(Engine& engine, const StepAuthority* authority) noexcept
    : engine_(engine), outer_(std::exchange(engine.authority, authority)) {}

AuthorityScope::~AuthorityScope() { engine_.authority = outer_; }

/// @brief Links a running interpreter's budget to the event's while it lives: the event's going in, and back when it
/// ends, whatever ends it.
class Programs::Linked {
public:
	/// @brief Gives the interpreter what the event may still do.
	/// @param programs The programs.
	/// @param interpreter The interpreter.
	Linked(const Programs& programs, lisp::Interpreter& interpreter)
	    : work_(programs.work()), interpreter_(interpreter) {
		interpreter_.setBudget({work_.operations, work_.bytes});
	}
	/// @brief Gives the event what is left.
	~Linked() {
		const lisp::Budget left = interpreter_.budget();
		work_.operations = left.operations;
		work_.bytes = left.bytes;
	}
	Linked(const Linked&) = delete;
	Linked& operator=(const Linked&) = delete;
	Linked(Linked&&) = delete;
	Linked& operator=(Linked&&) = delete;

private:
	WorkBudget& work_;               ///< The event's.
	lisp::Interpreter& interpreter_; ///< The interpreter's.
};

/// @brief A call of a function, as long as it lasts: it holds its interpreter, whatever happens to the program
/// meanwhile, what it allocates goes when it ends, and what it spends is the event's.
class Programs::Entered {
public:
	/// @brief Enters.
	/// @param programs The programs.
	/// @param frame What it is called from.
	explicit Entered(Programs& programs, Frame frame)
	    : programs_(programs), interpreter_(programs.live_), frame_(frame), outer_(programs.enter(&frame_)),
	      transient_(*interpreter_), linked_(programs, *interpreter_) {}
	/// @brief Leaves.
	~Entered() { programs_.frame_ = outer_; }
	Entered(const Entered&) = delete;
	Entered& operator=(const Entered&) = delete;
	Entered(Entered&&) = delete;
	Entered& operator=(Entered&&) = delete;

	/// @brief The interpreter. @return It.
	[[nodiscard]] lisp::Interpreter& interpreter() const noexcept { return *interpreter_; }

private:
	Programs& programs_;                             ///< The programs.
	std::shared_ptr<lisp::Interpreter> interpreter_; ///< Running code keeps its version.
	Frame frame_;                                    ///< What it is called from.
	Frame* outer_;                                   ///< The call around it.
	lisp::Interpreter::Transient transient_;         ///< What it allocates.
	Linked linked_;                                  ///< What it spends.
};

WorkBudget& Programs::work() const {
	WorkBudget* running = engine_.document.budget();
	return running ? *running : ownBudget_;
}

Programs::Frame* Programs::enter(Frame* frame) {
	if(!frame_) ownBudget_ = {stepsPerEvent, operationsPerEvent, bytesPerEvent};
	return std::exchange(frame_, frame);
}

void Programs::load() {
	const json& source = engine_.versions.sourceDocument();
	const auto section = source.find("program");
	if(section == source.end()) {
		live_.reset();
		loaded_ = nullptr;
		return;
	}
	if(live_ && loaded_ == *section) return;
	json authors = engine_.versions.programAuthors();
	creditTheRest(*section, authors);
	const lisp::Program program = lisp::programOf(*section, authors);
	lisp::Limits limits;
	limits.budget = {lisp::maxBuildOperations, lisp::maxBuildBytes};
	auto next = std::make_shared<lisp::Interpreter>(limits);
	definePrimitives(*next);
	next->setPure(true); // macros run now, and values are worked out: neither reaches anything
	program.load(*next);
	next->workOutDefinitions();
	next->setPure(false);
	next->setGate([this](const lisp::CallContext& call) { check(call); });
	live_ = std::move(next);
	loaded_ = *section;
}

bool Programs::defines(std::string_view name) const { return live_ && live_->isProgramFunction(name); }

lisp::Value Programs::functionOf(lisp::Interpreter& interpreter, const std::string& name, const std::string& where) {
	const std::optional<lisp::Value> function = interpreter.global(name);
	if(!function) fail(where, "the program has no function '" + name + "'");
	return *function;
}

void Programs::run(const std::string& name, Panel& sender, const json& event, const Scope& locals,
                   const std::string& where) {
	if(!defines(name)) { // a callback of a definition taken away: it goes with it
		log(LogLevel::Info, where + ": the program no longer has '" + name + "'");
		return;
	}
	std::vector<json> arguments;
	for(const std::string& parameter : lisp::Interpreter::parametersOf(functionOf(*live_, name, where))) {
		const json* value = parameter.empty() ? nullptr : locals.find(parameter);
		if(value) arguments.push_back(*value);
		else arguments.push_back(parameter == "event" ? event : json());
	}
	runWith(name, std::move(arguments), sender, event, locals, where);
}

void Programs::runWith(const std::string& name, std::vector<json> arguments, Panel& sender, const json& event,
                       const Scope& locals, const std::string& where) {
	if(!defines(name)) {
		log(LogLevel::Info, where + ": the program no longer has '" + name + "'");
		return;
	}
	const Entered entered(*this, {&sender, &event, &locals});
	lisp::Interpreter& interpreter = entered.interpreter();
	std::vector<lisp::Value> values;
	values.reserve(arguments.size());
	for(json& argument : arguments) values.push_back(viewOf(interpreter, std::move(argument)));
	static_cast<void>(interpreter.call(functionOf(interpreter, name, where), values));
}

json Programs::read(const std::string& name, std::vector<json> arguments, const Scope& locals, const json& event,
                    const std::string& where) {
	if(!live_) fail(where, "this screen has no program, so lisp() has nothing to call");
	const Entered entered(*this, {nullptr, &event, &locals});
	lisp::Interpreter& interpreter = entered.interpreter();
	++reading_;
	try {
		std::vector<lisp::Value> values;
		values.reserve(arguments.size());
		for(json& argument : arguments) values.push_back(viewOf(interpreter, std::move(argument)));
		json result = interpreter.toTree(interpreter.call(functionOf(interpreter, name, where), values));
		--reading_;
		return result;
	} catch(...) {
		--reading_;
		throw;
	}
}

std::optional<std::string> Programs::refusal(std::string_view what, StepKind kind,
                                             std::span<const std::string> ran) const {
	if(kind == StepKind::None || kind == StepKind::Free) return std::nullopt;
	const std::string step =
	    "the " + std::string(kind == StepKind::Review ? "review" : "control") + " step \"" + std::string(what) + "\"";
	if(ran.empty()) return "code with no author may not run " + step;
	const auto stranger = std::ranges::find_if(ran, [](const std::string& author) { return !isPersonsAuthor(author); });
	if(stranger != ran.end())
		return "code by \"" + *stranger + "\" led here, and only the person's own code and the app's file may run " +
		       step;
	const bool input = engine_.window && engine_.window->routingInput();
	if(kind == StepKind::Review && !input) return step + " runs only from the person's own input";
	if(engine_.versions.previewing() && !input)
		return "while a proposal is previewed, control steps run only from the user's input";
	return std::nullopt;
}

void Programs::check(const lisp::CallContext& call) const {
	if(call.capability == "read") return;
	if(reading_ > 0)
		throw lisp::Refusal("a value read for the screen only reads, so \"" + std::string(call.primitive) +
		                    "\" can't run there");
	std::vector<std::string> ran(call.ran.begin(), call.ran.end());
	if(engine_.authority) ran.insert(ran.end(), engine_.authority->authors.begin(), engine_.authority->authors.end());
	const StepKind kind = call.capability == "review"    ? StepKind::Review
	                      : call.capability == "control" ? StepKind::Control
	                                                     : StepKind::Free;
	if(const std::optional<std::string> why = refusal(call.primitive, kind, ran)) throw lisp::Refusal(*why);
}

void Programs::checkStep(const json& step, const std::string& where) const {
	if(!step.is_object()) return;
	for(const auto& [key, value] : step.items())
		if(const auto why = refusal(key, stepKind(key), engine_.authority->authors)) fail(where, *why);
}

void Programs::checkNative(const std::string& name, const std::string& where) const {
	if(const auto why = refusal(name, StepKind::Control, engine_.authority->authors)) fail(where, *why);
}

} // namespace affresco::format
