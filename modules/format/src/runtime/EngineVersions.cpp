/// @file
/// @brief format::Engine as the version control's observer: what a new version of the document or of the proposals
/// brings.

#include "Engine.hpp"

#include <affresco/format/NodeIds.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// @brief The parts of the engine that read sections of the document as a version lands, beside the update itself,
/// as bits.
namespace reader {
constexpr unsigned dataModels = 1U; ///< The data layer: the models it follows, the queries its `data` variable runs.
constexpr unsigned program = 2U;    ///< The program, loaded to run.
constexpr unsigned derived = 4U;    ///< Colours, design tokens, fonts, languages and strings (readDerived()).
constexpr unsigned services = 8U;   ///< Services and connections.
constexpr unsigned sounds = 16U;    ///< The sounds.
constexpr unsigned windows = 32U;   ///< The other windows.
constexpr unsigned every = 63U;     ///< All of them.
} // namespace reader

/// @brief What each reader reads: a version runs a reader again only when it touches one of its sections. Every
/// other section is read by the update, which follows every version, or by the steps as they run. A section read by
/// all is one that may change what any of them sees (an asset a font or a sound names, a feature, the state).
constexpr std::array<std::pair<std::string_view, unsigned>, 18> sectionReaders{{
    {"models", reader::dataModels},
    {"queries", reader::dataModels},
    {"migrations", reader::dataModels},
    {"program", reader::program},
    {"colors", reader::derived},
    {"tokens", reader::derived},
    {"fonts", reader::derived},
    {"languages", reader::derived},
    {"strings", reader::derived},
    {"services", reader::services},
    {"connections", reader::services},
    {"audio", reader::sounds},
    {"windows", reader::windows},
    {"assets", reader::every},
    {"features", reader::every},
    {"imports", reader::every},
    {"include", reader::every},
    {"state", reader::every},
}};

/// @brief The readers a change's sections need.
/// @param sections The sections touched.
/// @return Them, as reader bits.
unsigned readersOf(const std::vector<std::string>& sections) {
	unsigned readers = 0U;
	for(const std::string& section : sections)
		for(const auto& [name, bits] : sectionReaders)
			if(name == section) readers |= bits;
	return readers;
}

/// @brief How the panels follow a revision, per section it touches: only the lists of children it changed
/// (CompiledCache::carryOver()) and the readers of the places it marks, or a full pass, for the reason given (what
/// UpdateStatistics::fullReason then says). A section not listed takes the full pass: one the panels may read
/// without a place saying so. Among sections, the first that takes a full pass gives its reason.
constexpr std::array<std::pair<std::string_view, std::string_view>, 21> revisionPasses{{
    // The nodes: what each kept compiled node's panels show reads its place (childrenPlace()); a root, a screen or an
    // overlay that changes in its own keys, or the screens and overlays named, take the full pass (listsOnly()).
    {"root", {}},
    {"screens", {}},
    {"overlays", {}},
    // The state: its places are marked, and their readers run, as an action's change runs them.
    {"state", {}},
    // Read by name by any binding or step: no place says which panels read them.
    {"computed", "revision: computed"},
    {"actions", "revision: actions"},
    {"watch", "revision: watch"},
    // What every node is compiled with: everything compiled is dropped (carryOver()).
    {"styles", "revision: styles"},
    {"components", "revision: components"},
    {"templates", "revision: templates"},
    // Read again for every panel's values (readDerived()).
    {"colors", "revision: colors"},
    {"tokens", "revision: tokens"},
    {"fonts", "revision: fonts"},
    {"strings", "revision: strings"},
    // Read outside the panels (the menus, the shortcuts, the window), on a full pass.
    {"commands", "revision: commands"},
    {"menus", "revision: menus"},
    {"settings", "revision: settings"},
    {"window", "revision: window"},
    // Read by the data layer and the program, whose results any binding may read.
    {"models", "revision: models"},
    {"queries", "revision: queries"},
    {"program", "revision: program"},
}};

/// @brief The section a revision's place is in. A place written as a node's id ("#logo/text") is in the section of
/// the node with that id, in the version after, else (a node removed) in the version before.
/// @param place The place: a JSON Pointer, or "#id" and the rest.
/// @param before The document before.
/// @param after The document after.
/// @return The section; empty if no node has that id.
std::string sectionOf(const std::string& place, const json& before, const json& after) {
	std::optional<std::string> pointer = place;
	if(place.starts_with('#')) {
		const std::string_view id = std::string_view(place).substr(1, place.find('/') - 1U);
		pointer = findNodeWithId(after, id);
		if(!pointer) pointer = findNodeWithId(before, id);
	}
	if(!pointer || !pointer->starts_with('/')) return {};
	return json::json_pointer(pointer->substr(0, pointer->find('/', 1))).back();
}

/// @brief Whether a revision's places take a full pass of the panels (revisionPasses).
/// @param change The revision.
/// @param after The document after.
/// @return The reason, or empty if only the lists and the places it changed need to be run.
std::string_view fullPassFor(const DocumentChange& change, const json& after) {
	for(const std::string& place : change.document) {
		const std::string section = sectionOf(place, *change.before, after);
		const std::string_view* reason = nullptr;
		for(const auto& [name, why] : revisionPasses)
			if(name == section) reason = &why;
		if(reason == nullptr) return "revision: another section";
		if(!reason->empty()) return *reason;
	}
	return {};
}

/// @brief Marks the places of the state a change says changed, for their readers.
/// @param document The engine's document.
/// @param change The change.
void markStatePlaces(DocumentContext& document, const DocumentChange& change) {
	for(const std::string& place : change.state)
		if(place.empty()) document.markStateChanged(); // the whole state
		else document.markStateChanged(json::json_pointer(place));
}

} // namespace

void Engine::documentChanged() {
	data.documentChanged(); // the records follow its models first: records that don't fit refuse the version
	programs.load();        // and its program: one that can't be loaded refuses it too
	builder.compiler().forgetVersion(); // what was compiled belongs to the version before
	document.readDerived();
	++revisionCounters().derivedReads;
	document.bumpStateVersion();    // a revision may change the state too
	computed.documentChanged();     // and the computed values' definitions
	serviceCalls.load();            // a restore may change the services or the connections
	audio.documentChanged();        // and the sounds, through an include or a feature
	otherWindows.documentChanged(); // and the other windows
	reconciler.reconcile("revision");
}

void Engine::readChangedSections(const DocumentChange& change) {
	const unsigned readers = readersOf(change.sections);
	if((readers & reader::dataModels) != 0U) data.documentChanged(); // first: records that don't fit refuse the version
	if((readers & reader::program) != 0U) programs.load();
	builder.compiler().carryOver(*change.before, versions.document(), change); // what didn't change stays compiled
	if((readers & reader::derived) != 0U) {
		document.readDerived();
		++revisionCounters().derivedReads;
	}
	document.bumpStateVersion(); // a revision may change the state too
	if((readers & reader::services) != 0U) serviceCalls.load();
	if((readers & reader::sounds) != 0U) audio.documentChanged();
	if((readers & reader::windows) != 0U) otherWindows.documentChanged();
}

void Engine::documentChangedWith(const DocumentChange& change) {
	if(change.everything || !change.before) {
		documentChanged();
		return;
	}
	if(change.document.empty()) { // the same document: a state update, with nothing compiled or reconciled
		markStatePlaces(document, change);
		document.reviewChanged(); // the version's number and the history: what reads them is evaluated too
		reconciler.updateNow();
		return;
	}
	readChangedSections(change);
	document.reviewChanged(); // the version's number and the history are new globals
	markStatePlaces(document, change);
	std::string_view full = fullPassFor(change, versions.document());
	const CompiledCache& compiler = builder.compiler();
	if(full.empty() && !compiler.listsOnly()) full = "revision: a root, screen or overlay";
	if(!full.empty()) {
		document.requestUpdate(full); // what reads outside the panels (the icon, the title, the menus) runs too
		reconciler.reconcile(full);
		return;
	}
	for(const std::uint64_t serial : compiler.changedLists()) document.markPlaceChanged(childrenPlace(serial));
	reconciler.updateNow(); // the lists it changed and the places it marked: one update
}

void Engine::proposalsChanged() {
	document.globalsChanged();
	reconciler.requestReconcile();
}

} // namespace affresco::format
