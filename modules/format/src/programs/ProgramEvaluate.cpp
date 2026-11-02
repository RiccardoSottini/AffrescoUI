/// @file
/// @brief The loop of a running program (RFC 0005, stage 4): evaluating a form is submitting a revision, a text
/// arriving from elsewhere is a proposal, and describing what is defined obeys read masks.

#include "Programs.hpp"

#include "Engine.hpp"
#include "NodeKeys.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// The most text one evaluation takes: what a file of a person's definitions takes.
constexpr std::size_t maxEvaluatedBytes = std::size_t{4} * lisp::maxEntryBytes;

/// The file a definition with no file yet goes in, unless the step says.
constexpr std::string_view defaultFile = "evaluated.lisp";

/// @brief A JSON Pointer's segment.
/// @param name The name.
/// @return It, escaped.
std::string pointerSegment(std::string_view name) {
	std::string segment;
	for(const char c : name) {
		if(c == '~') segment += "~0";
		else if(c == '/') segment += "~1";
		else segment += c;
	}
	return segment;
}

/// @brief The file of a program's section that holds a definition.
/// @param section The section.
/// @param name The definition's address.
/// @return The file; "" if none does.
std::string fileHolding(const json& section, const std::string& name) {
	for(const auto& [file, entries] : section.items())
		if(entries.is_object() && entries.contains(name)) return file;
	return "";
}

/// @brief The operations that set definitions in a program: each in the file that has it, else a given one.
/// @param section The program's section.
/// @param definitions Each definition's address and text.
/// @param file The file for those with none.
/// @return The patch.
json definitionPatch(const json& section, const std::vector<std::pair<std::string, std::string>>& definitions,
                     const std::string& file) {
	json patch = json::array();
	bool fileAdded = section.contains(file);
	for(const auto& [name, text] : definitions) {
		std::string holder = fileHolding(section, name);
		if(holder.empty()) {
			holder = file;
			if(!fileAdded) {
				patch.push_back(
				    {{"op", "add"}, {"path", "/program/" + pointerSegment(file)}, {"value", json::object()}});
				fileAdded = true;
			}
		}
		patch.push_back({{"op", "add"},
		                 {"path", "/program/" + pointerSegment(holder) + "/" + pointerSegment(name)},
		                 {"value", text}});
	}
	return patch;
}

/// @brief The address of a form that defines something.
/// @param form The form's text.
/// @return The address.
std::string addressOf(const std::string& form) {
	const std::vector<lisp::Entry> entries = lisp::splitEntries(form, "");
	if(entries.size() != 1) fail("evaluate", "expected one form");
	return entries.front().name;
}

/// @brief Reads a text's value as a list of definitions, for what arrives from elsewhere: a Lisp text, or a change set.
/// @param value The text, or the change set.
/// @param section The program's section.
/// @param file The file for definitions that have none.
/// @return The patch.
/// @throws DocumentError If it is neither.
json patchOfArrival(const json& value, const json& section, const std::string& file) {
	if(value.is_object() && value.value("format", "") == "affrescoui-changes") {
		json patch = json::array();
		const auto revisions = value.find("revisions");
		if(revisions == value.end() || !revisions->is_array()) fail("propose", "a change set holds revisions");
		for(const json& revision : *revisions) {
			const auto ops = revision.find("patch");
			if(!revision.is_object() || ops == revision.end() || !ops->is_array())
				fail("propose", "each revision of a change set holds a patch");
			patch.insert(patch.end(), ops->begin(), ops->end());
		}
		return patch;
	}
	if(!value.is_string()) fail("propose", "expected Lisp text or a change set");
	std::vector<std::pair<std::string, std::string>> definitions;
	for(const lisp::FormAt& form : lisp::formsOf(value.get_ref<const std::string&>(), file)) {
		if(form.kind.empty())
			fail("propose", "a file that arrives holds definitions: what it runs by itself isn't one");
		definitions.emplace_back(form.name, form.text);
	}
	return definitionPatch(section, definitions, file);
}

/// @brief Runs an "evaluate" step: reads the text now, and evaluates it after the event, as revisions are.
/// @param s The step.
void evaluateStep(const Running& s) {
	Engine& engine = s.steps.engine();
	const std::string& where = s.where;
	if(!engine.window || !engine.window->routingOwnPress())
		fail(where, "\"evaluate\" runs only from the person's own input: a key or a click, never a timer, a watch, a "
		            "key pressed for the screen or a handler that fires by itself");
	if(engine.asks.deciding()) fail(where, "an agent's reply runs this action: a reply is never the person's input");
	if(engine.versions.previewing()) fail(where, "a proposal is being previewed: end that first");
	if(!engine.programs.hasProgram()) fail(where, "this screen has no program, so there is nothing to evaluate in");
	const std::string text = string(s.value("evaluate"), where + ".evaluate");
	if(text.size() > maxEvaluatedBytes) fail(where, "the text is longer than a program's file may be");
	const std::string file =
	    s.step.contains("file") ? string(s.value("file"), where + ".file") : std::string(defaultFile);
	const SharedScope kept = snapshot(s.locals);
	const auto outcome = std::make_shared<Scope>(Scope{kept.get(), kept, json::object(), {}, {}});
	auto then = s.steps.later(s.step.value("then", json::array()), s.sender, s.event, outcome, where + ".then");
	std::optional<StepAuthority> authority;
	if(engine.authority) authority = *engine.authority;
	// NOLINTNEXTLINE(bugprone-exception-escape): what a task throws, the scheduler's caller reports, as with any step
	engine.scheduler.defer([&engine, text, file, outcome, then = std::move(then), sender = &s.sender, event = s.event,
	                        authority = std::move(authority), hasThen = s.step.contains("then")] {
		Window* window = engine.window;
		if(!window) return;
		Panel& from = engine.reconciler.recordOf(sender) ? *sender : window->root();
		const AuthorityScope holder(engine, authority ? &*authority : nullptr);
		const DocumentContext::BudgetScope budget(engine.document);
		Programs::Evaluated evaluated = engine.programs.evaluate(text, file, from, event, *outcome);
		outcome->vars = {{"result", evaluated.printed},
		                 {"error", evaluated.error ? json(*evaluated.error) : json()},
		                 {"proposal", evaluated.proposal ? json(*evaluated.proposal) : json()}};
		if(evaluated.error && !hasThen) engine.report(*evaluated.error);
		then();
		engine.reconciler.requestStateUpdate();
	});
}

/// @brief Runs a "propose" step: Lisp text or a change set, from a file or anywhere else, becomes a proposal in the
/// name of where it came from, never the person's own typing.
/// @param s The step.
void proposeStep(const Running& s) {
	Engine& engine = s.steps.engine();
	if(!engine.programs.hasProgram()) fail(s.where, "this screen has no program, so there is nothing to propose to");
	const json value = s.value("propose");
	const std::string name = s.step.contains("as") ? string(s.value("as"), s.where + ".as") : "file";
	const std::string file =
	    s.step.contains("file") ? string(s.value("file"), s.where + ".file") : std::string(defaultFile);
	const json& source = engine.versions.sourceDocument();
	RevisionResult result;
	try {
		result =
		    engine.versions.propose({{"author", "plugin:" + name},
			                         {"message", "Load " + name},
			                         {"patch", patchOfArrival(value, source.value("program", json::object()), file)}});
	} catch(const DocumentError& error) { // text that can't be read, or that runs by itself: no proposal
		result.status = RevisionStatus::Rejected;
		result.error = error.what();
	}
	Scope outcome{&s.locals, nullptr, json::object(), {}, {}};
	if(result.status == RevisionStatus::Rejected) {
		outcome.vars = {{"error", result.error}};
		if(!s.step.contains("onError")) fail(s.where, result.error);
		s.run(s.step["onError"], outcome, s.where + ".onError");
		return;
	}
	outcome.vars = {{"proposal", result.proposal}};
	if(s.step.contains("then")) s.run(s.step["then"], outcome, s.where + ".then");
}

/// @brief Runs an "exportChanges" step: the commits on main that touch a path, as a change set.
/// @param s The step.
void exportChangesStep(const Running& s) {
	Engine& engine = s.steps.engine();
	const std::string name = string(s.value("exportChanges"), s.where + ".exportChanges");
	json changes = engine.versions.exportChanges(s.step.contains("from") ? s.value("from") : json(0), name);
	if(s.step.contains("only")) {
		const std::string only = string(s.value("only"), s.where + ".only");
		json kept = json::array();
		for(const json& revision : changes["revisions"]) {
			const json& patch = revision["patch"];
			const auto within = [&](const json& op) { return op.value("path", std::string()).starts_with(only); };
			if(!patch.empty() && std::ranges::all_of(patch, within)) kept.push_back(revision);
		}
		changes["revisions"] = std::move(kept);
	}
	const json::json_pointer pointer = s.target("into");
	json& place = statePlace(engine.versions.state(), pointer, s.where + ".into");
	engine.document.spendBytes(changes, s.where + ".into");
	expect::shallowEnough(changes, pointerDepth(pointer), s.where + ".into");
	place = std::move(changes);
	engine.document.markStateChanged(pointer);
}

/// @brief Runs a step of the loop.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runLoopStep(const Running& s) {
	if(s.step.contains("evaluate")) {
		evaluateStep(s);
	} else if(s.step.contains("propose")) {
		s.steps.checkControl(s.where);
		proposeStep(s);
	} else if(s.step.contains("exportChanges")) {
		s.steps.checkControl(s.where);
		exportChangesStep(s);
	} else if(s.step.contains("runHooks")) {
		std::vector<json> arguments;
		if(s.step.contains("with")) {
			const json with = s.value("with");
			if(!with.is_array()) fail(s.where + ".with", "expected a list: the hooks' arguments, in order");
			arguments.assign(with.begin(), with.end());
		}
		s.steps.engine().programs.runHooks(string(s.value("runHooks"), s.where + ".runHooks"), std::move(arguments),
		                                   s.sender, s.event, s.locals, s.where);
	} else {
		return std::nullopt;
	}
	return true;
}

} // namespace

void registerLoopSteps(StepRegistry& registry) {
	registry.add({"loop",
	              {{"evaluate", {"file", "then"}, true},
	               {"propose", {"as", "file", "then", "onError"}, true},
	               {"exportChanges", {"from", "only", "into"}, true},
	               {"runHooks", {"with"}}},
	              runLoopStep});
}

void Programs::noteTyped(const std::string& place, const json& before, const json& after) {
	if(!live_ || !after.is_string()) return; // only a program evaluates anything
	static const std::string nothing;
	typed_.typed(place, before.is_string() ? before.get_ref<const std::string&>() : nothing,
	             after.get_ref<const std::string&>());
}

Programs::FormsRead Programs::readForms(const std::vector<lisp::FormAt>& forms, Panel& sender, const json& event,
                                        const Scope& locals) {
	FormsRead read;
	const Entered entered(*this, {&sender, &event, &locals});
	lisp::Interpreter& interpreter = entered.interpreter();
	for(const lisp::FormAt& form : forms) {
		if(form.kind.empty() && !interpreter.expandsToDefinition(form.text)) {
			read.expressions.push_back(form.text);
			read.lastName.clear();
			continue;
		}
		read.lastName = form.kind.empty() ? addressOf(form.text) : form.name;
		read.definitions.emplace_back(read.lastName, form.text);
	}
	return read;
}

Programs::Evaluated Programs::proposeUntyped(const FormsRead& read, const std::string& file) {
	Evaluated out;
	if(!read.expressions.empty()) {
		out.error = "this text isn't what you typed here, so it doesn't run: what runs as you is what you type";
		return out;
	}
	if(read.definitions.empty()) return out;
	const RevisionResult result = engine_.versions.propose(
	    {{"author", "plugin:evaluated"},
	     {"message", "Evaluate " + read.definitions.back().first},
	     {"patch",
	      definitionPatch(engine_.versions.sourceDocument().value("program", json::object()), read.definitions, file)}});
	if(result.status == RevisionStatus::Rejected) {
		out.error = result.error;
		return out;
	}
	out.proposal = result.proposal;
	out.printed = "proposal " + std::to_string(result.proposal) +
	              " waits for your review: this text isn't what you typed here, so it isn't yours";
	return out;
}

Programs::Evaluated Programs::runTyped(const FormsRead& read, const std::string& file, Panel& sender, const json& event,
                                       const Scope& locals) {
	Evaluated out;
	if(!read.definitions.empty()) {
		const json revision = {
		    {"author", "user"},
		    {"message", "Evaluate " + read.definitions.back().first},
		    {"patch", definitionPatch(engine_.versions.sourceDocument().value("program", json::object()),
			                          read.definitions, file)}};
		if(const std::optional<std::string> refused = engine_.versions.commitOwnInput(revision)) {
			out.error = *refused;
			return out;
		}
	}
	out.printed = read.lastName;
	const Entered entered(*this, {&sender, &event, &locals});
	lisp::Interpreter& interpreter = entered.interpreter();
	ownInput_ = true;
	/// @brief Ends the person's own input with the evaluation, whatever ends it.
	class Own {
	public:
		/// @brief Remembers it. @param flag The flag.
		explicit Own(bool& flag) : flag_(flag) {}
		/// @brief Clears it.
		~Own() { flag_ = false; }
		Own(const Own&) = delete;
		Own& operator=(const Own&) = delete;
		Own(Own&&) = delete;
		Own& operator=(Own&&) = delete;

	private:
		bool& flag_; ///< The flag.
	};
	const Own own(ownInput_);
	for(const std::string& expression : read.expressions)
		out.printed = interpreter.print(interpreter.evaluate(expression, "evaluated", "user"));
	return out;
}

Programs::Evaluated Programs::evaluate(const std::string& text, const std::string& file, Panel& sender,
                                       const json& event, const Scope& locals) {
	try {
		const std::vector<lisp::FormAt> forms = lisp::formsOf(text, "evaluated");
		const json& state = engine_.versions.state();
		// Only what the person's own keystrokes made is theirs: anything else can have been written by anyone
		const bool typed = std::ranges::all_of(
		    forms, [&](const lisp::FormAt& form) { return typed_.holds(form.text, state); });
		const FormsRead read = readForms(forms, sender, event, locals);
		return typed ? runTyped(read, file, sender, event, locals) : proposeUntyped(read, file);
	} catch(const lisp::LispError& error) {
		return {.printed = {}, .error = error.message(), .proposal = std::nullopt};
	} catch(const DocumentError& error) {
		return {.printed = {}, .error = error.what(), .proposal = std::nullopt};
	}
}

/* Describing */

std::vector<json> Programs::limitedViews(const lisp::Interpreter& interpreter) const {
	std::vector<json> views;
	std::vector<std::string> ran = interpreter.authorsThatRan();
	if(engine_.authority) ran.insert(ran.end(), engine_.authority->authors.begin(), engine_.authority->authors.end());
	for(const std::string& author : ran)
		if(std::optional<json> view = engine_.versions.readable(author)) views.push_back(std::move(*view));
	return views;
}

namespace {

/// @brief Tells whether a limited reader's view shows a name: a definition, or an action or a key by what it holds.
/// @param view The view.
/// @param section The section: "program", "actions" or "keys".
/// @param name The name.
/// @return true if it does.
bool viewShows(const json& view, const char* section, const std::string& name) {
	const auto found = view.find(section);
	if(found == view.end() || !found->is_object()) return false;
	if(std::string_view(section) != "program") return found->contains(name);
	return !fileHolding(*found, name).empty();
}

/// @brief Adds what a command says of itself to what is known of it.
/// @param action The action.
/// @param out What is known.
void addCommandFields(const json& action, json& out) {
	if(!action.is_object()) return;
	for(const char* key : {"title", "category"})
		if(action.contains(key)) out[key] = action[key];
	if(out.value("doc", "").empty() && action.contains("description")) out["doc"] = action["description"];
}

} // namespace

json Programs::describeName(lisp::Interpreter& interpreter, const std::string& name,
                            const std::vector<json>& views) const {
	const json& document = engine_.versions.document();
	const auto actions = document.find("actions");
	const bool isAction = actions != document.end() && actions->is_object() && actions->contains(name);
	const std::optional<lisp::DefinitionInfo> info = interpreter.describe(name);
	for(const json& view : views) // a limited reader: what its view doesn't show isn't there
		if(!viewShows(view, info ? "program" : "actions", name)) return nullptr;
	json out = json::object();
	if(info) {
		out = {{"name", info->name}, {"kind", info->kind}, {"doc", info->doc}, {"author", info->author}};
		if(info->place.file) out["file"] = *info->place.file;
		out["line"] = info->place.line;
		if(const std::optional<lisp::Value> global = interpreter.global(name))
			out["arguments"] = lisp::Interpreter::parametersOf(*global);
	} else if(isAction) {
		out = {{"name", name}, {"kind", "action"}};
	} else {
		return nullptr;
	}
	if(isAction) addCommandFields((*actions)[name], out);
	return out;
}

lisp::Value Programs::describePrimitive(std::string_view what, lisp::Interpreter& interpreter,
                                        std::span<const lisp::Value> arguments) {
	if(arguments.size() != 1) throw lisp::Refusal("\"" + std::string(what) + "\" takes one name");
	const json named = interpreter.toTree(arguments[0]);
	if(!named.is_string()) throw lisp::Refusal("\"" + std::string(what) + "\" takes a name");
	const std::string word = named.get<std::string>();
	const std::vector<json> views = limitedViews(interpreter);
	if(what == "apropos") return viewOf(interpreter, aproposOf(interpreter, word, views));
	if(what == "program-file") return viewOf(interpreter, programFileOf(word, views));
	if(what != "describe-key") return viewOf(interpreter, describeName(interpreter, word, views));
	const json& document = engine_.versions.document();
	const auto keys = document.find("keys");
	if(keys == document.end() || !keys->is_object() || !keys->contains(word)) return {};
	for(const json& view : views)
		if(!viewShows(view, "keys", word)) return {};
	const json& bound = (*keys)[word];
	const json runs = bound.is_object() ? bound.value("run", json()) : bound;
	json out = {{"key", word}, {"runs", runs}};
	if(runs.is_string()) out["command"] = describeName(interpreter, runs.get<std::string>(), views);
	return viewOf(interpreter, std::move(out));
}

json Programs::aproposOf(lisp::Interpreter& interpreter, const std::string& text,
                         const std::vector<json>& views) const {
	std::vector<std::string> names = interpreter.definitionNames();
	const json& document = engine_.versions.document();
	if(const auto actions = document.find("actions"); actions != document.end() && actions->is_object())
		for(const auto& [name, action] : actions->items()) names.push_back(name);
	std::ranges::sort(names);
	const auto [last, end] = std::ranges::unique(names);
	names.erase(last, end);
	json found = json::array();
	for(const std::string& name : names) {
		if(!name.contains(text)) continue;
		if(json described = describeName(interpreter, name, views); !described.is_null())
			found.push_back(std::move(described));
	}
	return found;
}

json Programs::programFileOf(const std::string& file, const std::vector<json>& views) const {
	const json& source = engine_.versions.sourceDocument();
	const auto section = source.find("program");
	if(section == source.end() || !section->is_object() || !section->contains(file)) return "";
	std::string text;
	for(const auto& [name, entry] : (*section)[file].items()) {
		if(!std::ranges::all_of(views, [&](const json& view) { return viewShows(view, "program", name); })) continue;
		text += entry.get<std::string>() + "\n\n";
	}
	return text;
}

void Programs::defineLoopPrimitives(lisp::Interpreter& interpreter) {
	for(const char* word : {"describe-function", "describe-variable", "describe-key", "apropos", "program-file"})
		interpreter.definePrimitive(word, {.capability = "read",
		                                   .pure = false,
		                                   .call = [this, what = std::string(word)](
		                                               lisp::Interpreter& in, std::span<const lisp::Value> arguments) {
			                                   return describePrimitive(what, in, arguments);
		                                   }});
	interpreter.definePrimitive(
	    "run-hooks", {.capability = "step",
		              .pure = false,
		              .call = [this](lisp::Interpreter& in, std::span<const lisp::Value> arguments) {
		                  if(arguments.empty() || arguments[0].kind() != lisp::Kind::String)
			                  throw lisp::Refusal("\"run-hooks\" takes a hook, then what its functions are given");
		                  runHookPoint(in, std::string(arguments[0].text()), arguments.subspan(1));
		                  return lisp::Value();
	                  }});
}

} // namespace affresco::format
