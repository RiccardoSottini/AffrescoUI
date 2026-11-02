/// @file
/// @brief Hooks and advice (RFC 0005, stage 5; SECURITY.md, "Hooks and advice"): each is its own entry of the program,
/// so taking the entry away takes it away.
///
/// A hook, and `:before` or `:after` advice, gives nothing back to what runs it, so its run is sealed: while it runs,
/// it has the least authority of whoever led to it, itself and whoever added it; when it ends, what runs next hasn't
/// been touched by it. It can't stop what it joins: failing is reported, naming who, and running out takes only its
/// share of the budget. `:around` holds what it wraps, so it isn't sealed, and it is reviewed as changing the function
/// is.

#include "Programs.hpp"

#include "Engine.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Tells whether any of a function's and an entry's authors isn't the person or the app's file: such code sees
/// neither the arguments nor the result of what it joins.
/// @param interpreter The interpreter.
/// @param function The function.
/// @param authors The entry's authors.
/// @return true if so.
bool byStranger(const lisp::Interpreter& interpreter, const lisp::Value& function,
                const std::vector<std::string>& authors) {
	const auto stranger = [](const std::string& author) { return !isPersonsAuthor(author); };
	return authors.empty() || std::ranges::any_of(authors, stranger) ||
	       std::ranges::any_of(interpreter.authorsOf(function), stranger);
}

} // namespace

/// @brief The share of what an event may still spend that each hook or `:before` or `:after` advice of one call gets:
/// together, half of it, so that none can take what the action itself needs, nor the others' turn.
struct Programs::Share {
	lisp::Budget each;     ///< Each one's operations and bytes.
	std::size_t steps = 0; ///< Each one's steps.
};

Programs::Share Programs::shareOf(const lisp::Interpreter& interpreter, std::size_t count) const {
	const lisp::Budget left = interpreter.budget();
	const std::size_t parts = 2 * std::max<std::size_t>(count, 1);
	return {{left.operations / parts, left.bytes / parts}, work().steps / parts};
}

void Programs::runSealed(lisp::Interpreter& interpreter, const ExtensionEntry& entry, const std::string& joined,
                         std::span<const lisp::Value> arguments, const Share& share) {
	const lisp::Budget had = interpreter.budget();
	WorkBudget& spent = work();
	const std::size_t stepsHad = spent.steps;
	interpreter.setBudget(share.each);
	spent.steps = std::min(stepsHad, share.steps);
	const std::size_t stepsGiven = spent.steps;
	const auto failed = [&](const std::string& why) {
		engine_.report(entry.label + " on \"" + joined + "\", by \"" + entry.authors.front() + "\", failed: " + why);
	};
	{
		const lisp::Interpreter::Sealed sealed(interpreter);
		try {
			interpreter.addAuthorsThatRan(entry.authors);
			const lisp::Value function = functionOf(interpreter, entry.function, entry.label);
			const bool stranger = byStranger(interpreter, function, entry.authors);
			static_cast<void>(interpreter.call(function, stranger ? std::span<const lisp::Value>() : arguments));
		} catch(const lisp::LispError& error) {
			failed(error.message());
		} catch(const DocumentError& error) {
			failed(error.what());
		} catch(const json::exception& error) {
			failed(error.what());
		}
	}
	const lisp::Budget left = interpreter.budget();
	const auto used = [](std::size_t given, std::size_t now) { return given > now ? given - now : 0; };
	const auto rest = [](std::size_t was, std::size_t gone) { return was > gone ? was - gone : 0; };
	interpreter.setBudget({rest(had.operations, used(share.each.operations, left.operations)),
	                       rest(had.bytes, used(share.each.bytes, left.bytes))});
	spent.steps = rest(stepsHad, used(stepsGiven, spent.steps));
}

lisp::Value Programs::callAround(lisp::Interpreter& interpreter, const std::vector<const ExtensionEntry*>& arounds,
                                 std::size_t index, const lisp::Value& original,
                                 const std::vector<lisp::Value>& arguments) {
	if(index == 0) return interpreter.call(original, arguments);
	const ExtensionEntry& entry = *arounds[index - 1];
	const lisp::Value function = functionOf(interpreter, entry.function, entry.label);
	const bool stranger = byStranger(interpreter, function, entry.authors);
	// What a stranger's advice is given can only go on as it was: it can't change the arguments, or see the result
	// NOLINTNEXTLINE(bugprone-exception-escape): a primitive may throw: the machine takes it as a condition
	const lisp::Value proceed = interpreter.makeFunction(
	    "proceed", {.capability = "read",
		            .pure = false,
		            .call = [this, &arounds, index, original, arguments, stranger](lisp::Interpreter& in, // NOLINT(bugprone-exception-escape)
		                                                                           std::span<const lisp::Value> given) {
		                const std::vector<lisp::Value> changed(given.begin(), given.end());
		                const lisp::Value result = callAround(in, arounds, index - 1, original,
						                                      stranger || changed.empty() ? arguments : changed);
		                return stranger ? lisp::Value() : result;
	                }});
	// Whoever added it decided that what it names runs here, so an agent can't get a person's function run this way
	interpreter.addAuthorsThatRan(entry.authors);
	std::vector<lisp::Value> values = {proceed};
	if(!stranger) values.insert(values.end(), arguments.begin(), arguments.end());
	return interpreter.call(function, values);
}

lisp::Value Programs::callAdvised(lisp::Interpreter& interpreter, const Extensions& table, const std::string& target,
                                  const lisp::Value& original, std::span<const lisp::Value> given) {
	const std::vector<lisp::Value> arguments(given.begin(), given.end());
	const std::vector<ExtensionEntry>& entries = table.advice.at(target);
	const auto of = [&](std::string_view how) {
		std::vector<const ExtensionEntry*> found;
		for(const ExtensionEntry& entry : entries)
			if(entry.how == how) found.push_back(&entry);
		return found;
	};
	const std::vector<const ExtensionEntry*> before = of("before"), around = of("around"), after = of("after");
	const Share share = shareOf(interpreter, before.size() + after.size());
	for(const ExtensionEntry* entry : before) runSealed(interpreter, *entry, target, arguments, share);
	const lisp::Value result = callAround(interpreter, around, around.size(), original, arguments);
	for(const ExtensionEntry* entry : after) runSealed(interpreter, *entry, target, arguments, share);
	return result;
}

void Programs::runHookPoint(lisp::Interpreter& interpreter, const std::string& hook,
                            std::span<const lisp::Value> arguments) {
	if(!extensions_) return;
	const std::shared_ptr<const Extensions> table = extensions_;
	const auto found = table->hooks.find(hook);
	if(found == table->hooks.end()) return;
	if(std::ranges::find(hooksRunning_, hook) != hooksRunning_.end()) {
		engine_.report("the hook \"" + hook + "\" was run from one of its own hooks: it runs once at a time");
		return;
	}
	hooksRunning_.push_back(hook);
	/// @brief Takes the hook off the running ones, whatever ends its run.
	class Done {
	public:
		/// @brief Remembers them. @param running The hooks running.
		explicit Done(std::vector<std::string>& running) : running_(running) {}
		/// @brief Takes it off.
		~Done() { running_.pop_back(); }
		Done(const Done&) = delete;
		Done& operator=(const Done&) = delete;
		Done(Done&&) = delete;
		Done& operator=(Done&&) = delete;

	private:
		std::vector<std::string>& running_; ///< The hooks running.
	};
	const Done done(hooksRunning_);
	const std::vector<lisp::Value> values(arguments.begin(), arguments.end());
	const Share share = shareOf(interpreter, found->second.size());
	for(const ExtensionEntry& entry : found->second) runSealed(interpreter, entry, hook, values, share);
}

void Programs::runHooks(const std::string& hook, std::vector<json> arguments, Panel& sender, const json& event,
                        const Scope& locals, const std::string& where) {
	if(!live_) fail(where, "this screen has no program, so it has no hooks");
	const Entered entered(*this, {&sender, &event, &locals});
	lisp::Interpreter& interpreter = entered.interpreter();
	std::vector<lisp::Value> values;
	values.reserve(arguments.size());
	for(json& argument : arguments) values.push_back(viewOf(interpreter, std::move(argument)));
	runHookPoint(interpreter, hook, values);
}

bool Programs::advisesAction(std::string_view name) const {
	return extensions_ && extensions_->advice.contains(name) && !defines(name);
}

void Programs::runAdvised(const std::string& name, Panel& sender, const json& event, const Scope& locals,
                          const std::function<void()>& body) {
	const std::shared_ptr<const Extensions> table = extensions_;
	const Entered entered(*this, {&sender, &event, &locals});
	lisp::Interpreter& interpreter = entered.interpreter();
	const lisp::Value original = interpreter.makeFunction(
	    name, {.capability = "read",
		       .pure = false,
		       .call = [this, &body](lisp::Interpreter& in, std::span<const lisp::Value>) {
		           // An agent's `:around` that goes on still led here: the action's steps are checked against it
		           std::vector<std::string> ran = in.authorsThatRan();
		           if(engine_.authority)
			           ran.insert(ran.end(), engine_.authority->authors.begin(), engine_.authority->authors.end());
		           const StepAuthority authority{std::move(ran)};
		           const AuthorityScope holder(engine_, authority.authors.empty() ? nullptr : &authority);
		           outside(in, body);
		           return lisp::Value();
	           }});
	static_cast<void>(callAdvised(interpreter, *table, name, original, {}));
}

void Programs::addExtension(const lisp::Interpreter& interpreter, const lisp::Extension& extension,
                            Extensions& table) const {
	const std::string& function = extension.targets.back();
	if(!interpreter.isProgramFunction(function))
		fail(extension.name, "\"" + function + "\" is no function of the program");
	std::vector<std::string> authors = extension.authors;
	if(authors.empty()) authors.emplace_back(unknownAuthor);
	const bool hook = extension.kind == "add-hook";
	ExtensionEntry entry{function, hook ? "" : extension.targets[1],
	                     (hook ? "the hook \"" : "the advice \"") + function + "\"", std::move(authors)};
	if(hook) {
		table.hooks[extension.targets[0]].push_back(std::move(entry));
		return;
	}
	const std::string& target = extension.targets[0];
	const json& document = engine_.versions.document();
	const auto actions = document.find("actions");
	const bool isAction = actions != document.end() && actions->is_object() && actions->contains(target);
	if(!interpreter.isProgramFunction(target) && !isAction)
		fail(extension.name, "advice joins a function of the program or an action, and \"" + target + "\" is neither");
	table.advice[target].push_back(std::move(entry));
}

void Programs::loadExtensions(lisp::Interpreter& interpreter) {
	auto table = std::make_shared<Extensions>();
	const std::vector<lisp::Extension> extensions = interpreter.extensions();
	for(const lisp::Extension& extension : extensions)
		if(extension.kind != "remove-hook") addExtension(interpreter, extension, *table);
	for(const lisp::Extension& extension : extensions) {
		if(extension.kind != "remove-hook") continue;
		const auto found = table->hooks.find(extension.targets[0]);
		if(found != table->hooks.end())
			std::erase_if(found->second,
			              [&](const ExtensionEntry& entry) { return entry.function == extension.targets[1]; });
	}
	for(const auto& [target, entries] : table->advice) {
		if(!interpreter.isProgramFunction(target)) continue;
		const std::shared_ptr<lisp::Value> original = std::make_shared<lisp::Value>();
		*original = interpreter.wrapGlobal(
		    target, {.capability = "read",
			         .pure = false,
			         .call = [this, table = std::shared_ptr<const Extensions>(table), name = target,
			                  original](lisp::Interpreter& in, std::span<const lisp::Value> arguments) {
			             return callAdvised(in, *table, name, *original, arguments);
		             }});
	}
	extensionsNext_ = std::move(table);
}

} // namespace affresco::format
