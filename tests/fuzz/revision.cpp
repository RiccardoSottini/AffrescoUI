/// @file
/// @brief Fuzzes a revision submitted to a vcs::Repository loaded with the Kanban template, as a peer's "revision"
/// message is: compact edits or JSON Patch, judged by the template's permission rules.
///
/// Besides not crashing, the rules VERSION_CONTROL.md promises must hold:
/// - a revision that isn't applied changes nothing (revisions are atomic);
/// - one that is applied, directly or once a reviewer accepts it, is undone exactly by reverting it.

#include "Fuzz.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

using affresco::json;
using affresco::RevisionStatus;

namespace {

/// @brief A document and its state, to compare with later.
struct Snapshot {
	std::string document; ///< The document, as JSON text (so the order of keys counts).
	std::string state;    ///< The live state, likewise.
	int head;             ///< main's revision number.

	/// @brief Takes one.
	/// @param repository The repository.
	explicit Snapshot(const affresco::vcs::Repository& repository)
	    : document(repository.document().dump()), state(repository.state().dump()), head(repository.head().number) {}

	/// @brief Tells whether two snapshots hold the same values, whatever the order of their keys.
	/// @param other Another snapshot.
	/// @return true if the documents and the states are equal.
	[[nodiscard]] bool sameValues(const Snapshot& other) const {
		return nlohmann::json::parse(document) == nlohmann::json::parse(other.document) &&
		       nlohmann::json::parse(state) == nlohmann::json::parse(other.state);
	}
};

/// @brief Submits a revision as a Session does: a malformed one is refused.
/// @param repository The repository.
/// @param revision The revision.
/// @return What happened.
affresco::RevisionResult submit(affresco::vcs::Repository& repository, const json& revision) {
	try {
		return repository.submit(revision);
	} catch(const json::exception& error) {
		return {RevisionStatus::Rejected, repository.head().number, {}, 0, error.what()};
	}
}

/// @brief Reverts main's head as the reviewer, and checks that this brings back what was there before it.
/// @param repository The repository, its head just applied.
/// @param before The repository before that.
void checkRevert(affresco::vcs::Repository& repository, const Snapshot& before) {
	json revert;
	try {
		revert = repository.revertRevision("HEAD", "studio");
	} catch(const affresco::DocumentError&) {
		return; // e.g. the commit changed nothing
	}
	if(submit(repository, revert).status != RevisionStatus::Applied) return; // e.g. a deny rule: refused both ways
	if(!Snapshot(repository).sameValues(before)) affresco::fuzz::broken("reverting a commit restores what it changed");
}

} // namespace

/// @brief Silences the log before the first input.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int*, char***) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point: the input is the revision's JSON text.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	json revision;
	try {
		revision = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const std::exception&) {
		return 0; // not JSON: refused before it reaches version control (json.cpp fuzzes that)
	}

	static const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, affresco::fuzz::kanbanTemplate()); // a new one: inputs don't add up
	const Snapshot before(repository);
	const affresco::RevisionResult result = submit(repository, revision);

	if(result.status != RevisionStatus::Applied) {
		const Snapshot after(repository);
		if(after.document != before.document || after.state != before.state || after.head != before.head)
			affresco::fuzz::broken("a revision that isn't applied changes nothing");
	}
	if(result.status == RevisionStatus::Proposed) {
		static_cast<void>(repository.proposalDiff(result.proposal));
		if(repository.accept(result.proposal, {"studio", result.commit, false})) return 0; // e.g. its own author
	}
	if(result.status != RevisionStatus::Rejected) {
		static_cast<void>(repository.diff(0, "HEAD"));
		checkRevert(repository, before);
	}
	return 0;
}
