/// @file
/// @brief Fuzzes an undo that keeps later changes (VERSION_CONTROL.md, "Undo that keeps what came later"): a few
/// revisions committed to a vcs::Repository loaded with the JSON template, or with a document that holds a program,
/// then the plan and the undo of one of them with the input's choices.
///
/// The input: `{"revisions": [...], "target": n, "drop": [...] | "all", "keep": [...] | "all", "then": [...]}`.
/// Besides not crashing, what VERSION_CONTROL.md promises must hold:
/// - undoPlan() changes nothing (the document, the state, main's head, the proposals), and gives the same plan twice;
/// - a plan that is clean or ready lands with its head; any other is refused (when worked out, or by the rules when
///   submitted), and the refusal changes nothing;
/// - an undo that landed adds one commit to main, whose undo mark names the commit undone; its document is valid;
/// - undoing it again, as it is now, lands nothing new on what the first took back;
/// - written to a history folder and opened again, the head and its undo mark are the same.

#include "Fuzz.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>

#include <unistd.h>

using affresco::json;
using affresco::RevisionStatus;
using affresco::vcs::Repository;

namespace {

/// Most revisions an input commits before the undo: enough for later changes on later changes, and quick.
constexpr std::size_t maxRevisions = 8;

/// @brief What an undo must not change unless it lands.
struct Snapshot {
	std::string document;  ///< The document, as JSON text (so the order of keys counts).
	std::string state;     ///< The live state, likewise.
	std::string head;      ///< main's head commit.
	std::string proposals; ///< The open proposals.

	/// @brief Takes one.
	/// @param repository The repository.
	explicit Snapshot(const Repository& repository)
	    : document(repository.document().dump()), state(repository.state().dump()), head(repository.head().id) {
		json open = json::array();
		for(const affresco::ProposalInfo& proposal : repository.proposals()) open.push_back(affresco::toJson(proposal));
		proposals = open.dump();
	}

	/// @brief Tells whether nothing changed.
	/// @param other Another snapshot.
	/// @return true if every part is the same text.
	[[nodiscard]] bool operator==(const Snapshot& other) const = default;
};

/// @brief Commits a revision as the studio, as a Session would submit it: a malformed one is refused.
/// @param repository The repository.
/// @param revision The revision.
/// @return false if it was malformed (refused, as a malformed message is).
bool commit(Repository& repository, json revision) {
	if(!revision.is_object()) return false;
	revision["author"] = "studio";
	revision.erase("undo"); // a forged mark is the revision target's to fuzz; here only real undos are made
	try {
		static_cast<void>(repository.submit(revision));
		return true;
	} catch(const json::exception&) {
		return false;
	}
}

/// @brief Submits an undo's revision, as its author.
/// @param repository The repository.
/// @param revision The undo's revision.
/// @return What happened.
affresco::RevisionResult submit(Repository& repository, const json& revision) {
	try {
		return repository.submit(revision);
	} catch(const json::exception& error) {
		return {RevisionStatus::Rejected, repository.head().number, {}, 0, error.what()};
	}
}

/// @brief Who undoes: the studio, a reviewer where the screen declares permissions; else the person, whose undo that
/// takes out later changes lands where the studio's would wait for review.
/// @param repository The repository.
/// @return The author.
std::string_view undoerOf(const Repository& repository) { return repository.mayTakeOut("studio") ? "studio" : "user"; }

/// @brief The plan of an undo, or nothing when the call itself refuses it (an unknown commit).
/// @param repository The repository.
/// @param target The commit.
/// @param options The choices.
/// @return The plan as the live protocol sends it, or null.
json planOf(const Repository& repository, const json& target, const affresco::UndoOptions& options) {
	try {
		return affresco::toJson(repository.undoPlan(target, undoerOf(repository), options));
	} catch(const affresco::DocumentError&) {
		return nullptr;
	} catch(const json::exception&) {
		return nullptr;
	}
}

/// @brief The undo's revision, or null when it is refused.
/// @param repository The repository.
/// @param target The commit.
/// @param options The choices, with the head they were made on.
/// @return The revision, or null.
json undoOf(const Repository& repository, const json& target, const affresco::UndoOptions& options) {
	try {
		return repository.undoRevision(target, undoerOf(repository), options);
	} catch(const affresco::DocumentError&) {
		return nullptr;
	} catch(const json::exception&) {
		return nullptr;
	}
}

/// @brief A folder of this process's own in the system's temporary folder, emptied before each use.
/// @return It.
const std::filesystem::path& folder() {
	static const std::filesystem::path path =
	    std::filesystem::temp_directory_path() / ("affrescoui-fuzz-undo-" + std::to_string(::getpid()));
	return path;
}

/// @brief Replays the revisions and the undo in a repository that writes its history to a folder, opens the folder
/// again, and checks that the head and its undo mark came back.
/// @param rules The rules.
/// @param document The document.
/// @param revisions The revisions.
/// @param target The commit undone.
/// @param options The choices.
void checkOnDisk(const affresco::format::Rules& rules, const json& document, const json& revisions, const json& target,
                 affresco::UndoOptions options) {
	std::error_code ignored;
	std::filesystem::remove_all(folder(), ignored);
	std::string head;
	json mark;
	{
		Repository writer(rules, document);
		if(!writer.openHistory(folder()).memoryOnly.empty()) return; // nothing would be written
		for(std::size_t i = 0; i < revisions.size() && i < maxRevisions; ++i) commit(writer, revisions[i]);
		const json plan = planOf(writer, target, options);
		if(plan.is_null() || (plan["outcome"] != "clean" && plan["outcome"] != "ready"))
			return; // other ids, other times
		options.head = plan["head"].get<std::string>();
		const json revision = undoOf(writer, target, options);
		if(revision.is_null() || submit(writer, revision).status != RevisionStatus::Applied) return;
		head = writer.head().id;
		mark = writer.head().undo;
	} // releases the folder's lock
	Repository reader(rules, document);
	if(!reader.openHistory(folder()).loaded) affresco::fuzz::broken("a history with an undo is read back");
	if(reader.head().id != head || reader.head().undo != mark)
		affresco::fuzz::broken("a history read back has the same head and the same undo mark");
	std::filesystem::remove_all(folder(), ignored);
}

/// @brief The plan of the undo, checked: making it changes nothing, and it is the same twice.
/// @param repository The repository.
/// @param target The commit.
/// @param options The choices.
/// @return The plan, or null.
json checkedPlan(const Repository& repository, const json& target, const affresco::UndoOptions& options) {
	const Snapshot before(repository);
	const json plan = planOf(repository, target, options);
	if(Snapshot(repository) != before) affresco::fuzz::broken("undoPlan changes nothing");
	if(planOf(repository, target, options) != plan) affresco::fuzz::broken("undoPlan gives the same plan twice");
	return plan;
}

/// @brief Checks an undo that landed on main: one commit, whose mark names the commit undone, and a valid document;
/// then undoes it again, as main is now, with no choice: refused, or nothing changes.
/// @param rules The rules.
/// @param repository The repository, the undo just applied.
/// @param number main's head's number before it.
/// @param undone The id of the commit undone (by its id: "HEAD" now names the undo).
void checkLanded(const affresco::format::Rules& rules, Repository& repository, int number, const std::string& undone) {
	if(repository.head().number != number + 1) affresco::fuzz::broken("an undo adds one commit to main");
	if(repository.head().undo.value("of", std::string()) != undone)
		affresco::fuzz::broken("an undo's mark names the commit it undoes");
	try {
		rules.validate(repository.sourceDocument());
	} catch(const affresco::DocumentError&) {
		affresco::fuzz::broken("an undo leaves a valid document");
	}

	const json after = repository.document();
	const json state = repository.state();
	affresco::UndoOptions again;
	again.head = repository.head().id;
	const json second = undoOf(repository, json(undone), again);
	if(!second.is_null() && submit(repository, second).status == RevisionStatus::Applied &&
	   (repository.document() != after || repository.state() != state))
		affresco::fuzz::broken("a second undo lands nothing new on what the first took back");
}

/// @brief Commits the revisions, plans and undoes the target, and checks what VERSION_CONTROL.md promises of it.
/// @param rules The rules.
/// @param document The document the history starts from.
/// @param input The input.
/// @param options The choices it makes.
void judge(const affresco::format::Rules& rules, const json& document, const json& input,
           affresco::UndoOptions options) {
	Repository repository(rules, document);
	const json& revisions = input["revisions"];
	for(std::size_t i = 0; i < revisions.size() && i < maxRevisions; ++i) commit(repository, revisions[i]);
	const json target = input.value("target", json(1));

	const Snapshot before(repository);
	const json plan = checkedPlan(repository, target, options);
	if(plan.is_null()) return;

	const bool lands = plan["outcome"] == "clean" || plan["outcome"] == "ready";
	options.head = plan["head"].get<std::string>();
	const json revision = undoOf(repository, target, options);
	if(Snapshot(repository) != before) affresco::fuzz::broken("working out an undo changes nothing");
	if(lands && revision.is_null()) affresco::fuzz::broken("a plan that is clean or ready gives its undo");
	if(revision.is_null()) return;

	// Worked out, the undo isn't judged: the submit that lands it is, as the plan was
	const int number = repository.head().number;
	const std::string undone = repository.resolve(target).id;
	const affresco::RevisionResult result = submit(repository, revision);
	if(result.status == RevisionStatus::Rejected) {
		if(Snapshot(repository) != before) affresco::fuzz::broken("an undo that is refused changes nothing");
		if(lands) affresco::fuzz::broken("an undo the plan calls clean or ready lands");
		return;
	}
	if(!lands) affresco::fuzz::broken("an undo the plan refuses doesn't land");
	if(result.status == RevisionStatus::Proposed) return; // the rules ask for review
	checkLanded(rules, repository, number, undone);
	checkOnDisk(rules, document, revisions, target, options);
}

/// A program with a few definitions that use each other, for the revisions on `/program/...`.
constexpr const char* kProgram = R"lisp(
(def greeting "A value." "Hello")
(defn helper "Only a function." [n] (inc n))
(defn uses "Uses the helper." [] (helper 1))
(defcommand go "A command." :title "Go" :category "Run" [] (uses))
(defscreen main :state {:n 1} :permissions {:allow ["studio"] :reviewers ["studio"]}
  (column#list (label#a :text greeting) (label#b :text "B")))
)lisp";

} // namespace

/// @brief Silences the log before the first input.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point: the input is the JSON text described above.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	json input;
	try {
		input = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const std::exception&) {
		return 0; // not JSON: refused before it reaches version control (json.cpp fuzzes that)
	}
	if(!input.is_object() || !input.contains("revisions") || !input["revisions"].is_array()) return 0;
	affresco::UndoOptions options;
	try {
		options = affresco::undoOptionsFrom(input, "undo"); // as a revert message's choices are read
	} catch(const affresco::DocumentError&) {
		return 0; // refused before the plan
	}
	options.head.clear();

	static const affresco::format::Rules rules;
	if(!affresco::fuzz::text(data, size).contains("/program"))
		judge(rules, affresco::fuzz::jsonTemplate(), input, options);
	static const json program = affresco::lisp::sourceDocument(kProgram, "app.lisp");
	judge(rules, program, input, options);
	return 0;
}
