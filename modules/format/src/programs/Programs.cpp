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
	 "audio",    "dismiss", "transform", "insert", "update",    "delete",   "undoData",   "redoData",    "runHooks"});

/// The steps that decide on changes: only ever from the person's own input.
constexpr auto reviewSteps =
    std::to_array<std::string_view>({"accept", "reject", "preview", "endPreview", "restore", "revert", "undo",
	                                 "evaluate", "allowRun", "denyRun", "undoRun"});

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

bool judgedByUse(std::string_view step, std::string_view name) {
	if(name.empty() || name.contains("{{")) return false;
	if(step == "invoke" || step == "request" || step == "subscribe") return true;
	return step == "image" && name != "open" && name != "save";
}

StepKind stepKind(std::string_view name) {
	if(std::ranges::find(reviewSteps, name) != reviewSteps.end()) return StepKind::Review;
	if(std::ranges::find(freeSteps, name) != freeSteps.end()) return StepKind::Free;
	return builtInSteps()->find(name) ? StepKind::Control : StepKind::None;
}

std::string_view stepOfWord(std::string_view word) noexcept {
	if(word == "remove-state") return "remove";
	if(word == "update-record") return "update";
	if(word == "run-hooks") return "runHooks";
	return word;
}

bool isPersonsAuthor(std::string_view author) noexcept {
	return author == "file" || author == "user" || author.starts_with("user/");
}

KeptAuthority keptAuthority(const Engine& engine) {
	return engine.authority ? KeptAuthority(*engine.authority) : std::nullopt;
}

AuthorityScope::AuthorityScope(Engine& engine, const StepAuthority* authority, bool judged)
    : engine_(engine), outer_(std::exchange(engine.authority, authority)) {
	// Only the outermost scope of a run's work judges it: one inside it (a Lisp step) is part of the same work
	if(judged && authority && authority->run && !(outer_ && outer_->run) && engine.runControl) {
		run_ = authority->run;
		before_ = engine.versions.state();
	}
}

// NOLINTNEXTLINE(bugprone-exception-escape): only building the report in its catch can throw, if memory runs out
AuthorityScope::~AuthorityScope() {
	engine_.authority = outer_;
	if(!run_) return;
	try {
		const json& state = engine_.versions.state();
		if(state == before_) return;
		const std::optional<std::string> refusal = engine_.runControl->stateDenial(run_->runner, before_, state);
		if(!refusal) return;
		engine_.versions.state() = std::move(before_);
		engine_.document.markStateChanged();
		engine_.reconciler.requestStateUpdate();
		engine_.report("what a run by '" + run_->runner + "' scheduled was put back: " + *refusal);
	} catch(const std::exception& error) { // never out of a destructor: the change stays, and is told
		engine_.report(std::string("what a run scheduled couldn't be judged: ") + error.what());
	}
}

WorkBudget& Programs::work() const {
	WorkBudget* running = engine_.document.budget();
	return running ? *running : ownBudget_;
}

Programs::Frame* Programs::enter(Frame* frame) {
	if(!frame_) ownBudget_ = {stepsPerEvent, operationsPerEvent, bytesPerEvent};
	return std::exchange(frame_, frame);
}

// A program whose features' code can't be loaded with it runs alone: the rules refuse such a revision, so it is only
// ever a document read from disk
void Programs::load() {
	const json& source = engine_.versions.sourceDocument();
	const auto own = source.find("program");
	if(own == source.end()) {
		live_.reset();
		extensions_.reset();
		loaded_ = nullptr;
		return;
	}
	const json section = runningProgram(source);
	if(live_ && loaded_ == section) return;
	try {
		loadSection(section);
	} catch(const std::exception& error) {
		if(section == *own) throw;
		engine_.report(std::string("the features' programs don't load with the app's, so they don't run: ") +
		               error.what());
		if(!live_ || loaded_ != *own) loadSection(*own);
	}
}

void Programs::loadSection(const json& section) {
	json authors = engine_.versions.programAuthors();
	creditTheRest(section, authors);
	creditFeatures(section, authors);
	const lisp::Program program = lisp::programOf(section, authors);
	lisp::Limits limits;
	limits.budget = {lisp::maxBuildOperations, lisp::maxBuildBytes};
	auto next = std::make_shared<lisp::Interpreter>(limits);
	definePrimitives(*next);
	next->setPure(true);        // macros run now, and values are worked out: neither reaches anything
	program.load(*next, false); // what runs: the build compiled everything, views too, and refused what didn't compile
	next->workOutDefinitions();
	next->setPure(false);
	loadExtensions(*next);
	next->setGate([this](const lisp::CallContext& call) { check(call); });
	live_ = std::move(next);
	extensions_ = std::move(extensionsNext_);
	loaded_ = section;
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
	const lisp::Value function = functionOf(interpreter, name, where);
	// The parameters past those given come by name from the variables around it, as run's do: a callback given its
	// first arguments ([opened path]) reads `result` or `error` so
	const std::vector<std::string> parameters = lisp::Interpreter::parametersOf(function);
	for(std::size_t at = arguments.size(); at < parameters.size(); ++at) {
		const json* value = parameters[at].empty() ? nullptr : locals.find(parameters[at]);
		arguments.push_back(value ? *value : parameters[at] == "event" ? event : json());
	}
	std::vector<lisp::Value> values;
	values.reserve(arguments.size());
	for(json& argument : arguments) values.push_back(viewOf(interpreter, std::move(argument)));
	static_cast<void>(interpreter.call(function, values));
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

bool Programs::hasProgram() const noexcept { return live_ != nullptr; }

std::optional<std::string> Programs::refusal(std::string_view what, StepKind kind,
                                             std::span<const std::string> ran) const {
	if(engine_.authority && engine_.authority->run && engine_.authority->run->refused)
		return "the run that scheduled this was refused, so what it scheduled runs no step";
	if(kind == StepKind::None || kind == StepKind::Free) return std::nullopt;
	const std::string step =
	    "the " + std::string(kind == StepKind::Review ? "review" : "control") + " step \"" + std::string(what) + "\"";
	if(ran.empty()) return "code with no author may not run " + step;
	const auto stranger = std::ranges::find_if(ran, [](const std::string& author) { return !isPersonsAuthor(author); });
	if(stranger != ran.end())
		return "code by \"" + *stranger + "\" led here, and only the person's own code and the app's file may run " +
		       step;
	const bool input = ownInput_ || (engine_.window && engine_.window->routingOwnPress());
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
	StepKind kind = call.capability == "review"    ? StepKind::Review
	                : call.capability == "control" ? StepKind::Control
	                                               : StepKind::Free;
	if(kind != StepKind::Free) checkFeatureCall(call.primitive, ran);
	// What the calling code names written out is judged by its use, as its text was (ServiceRules.cpp)
	if(kind == StepKind::Control && judgedByUse(call.primitive, call.named)) kind = StepKind::Free;
	if(const std::optional<std::string> why = refusal(call.primitive, kind, ran)) throw lisp::Refusal(*why);
}

void Programs::checkStep(const json& step, const std::string& where) const {
	if(!step.is_object()) return;
	checkFeatureStep(step, where);
	for(const auto& [key, value] : step.items()) {
		StepKind kind = stepKind(key);
		// A use written out in the document, judged by its use as the revision that wrote it was
		if(kind == StepKind::Control && value.is_string() && judgedByUse(key, value.get_ref<const std::string&>()))
			kind = StepKind::Free;
		if(const auto why = refusal(key, kind, engine_.authority->authors)) fail(where, *why);
	}
}

void Programs::checkNative(const std::string& name, const std::string& where) const {
	if(const auto why = refusal(name, StepKind::Control, engine_.authority->authors)) fail(where, *why);
}

} // namespace affresco::format
