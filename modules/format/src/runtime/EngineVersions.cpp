/// @file
/// @brief format::Engine as the version control's observer: what a new version of the document or of the proposals
/// brings.

#include "Engine.hpp"

#include <array>
#include <string_view>
#include <utility>

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

} // namespace

void Engine::documentChanged() {
	data.documentChanged(); // the records follow its models first: records that don't fit refuse the version
	programs.load();        // and its program: one that can't be loaded refuses it too
	builder.compiler().forgetVersion(); // what was compiled belongs to the version before
	document.readDerived();
	++revisionCounters().derivedReads;
	document.bumpStateVersion();    // a revision may change the state too
	serviceCalls.load();            // a restore may change the services or the connections
	audio.documentChanged();        // and the sounds, through an include or a feature
	otherWindows.documentChanged(); // and the other windows
	reconciler.reconcile("revision");
}

void Engine::documentChangedWith(const DocumentChange& change) {
	if(change.everything || !change.before) {
		documentChanged();
		return;
	}
	if(change.document.empty()) { // the same document: a state update, with nothing compiled or reconciled
		for(const std::string& place : change.state)
			if(place.empty()) document.markStateChanged(); // the whole state
			else document.markStateChanged(json::json_pointer(place));
		document.reviewChanged(); // the version's number and the history: what reads them is evaluated too
		reconciler.updateNow();
		return;
	}
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
	document.reviewChanged();           // the version's number and the history are new globals
	document.requestUpdate("revision"); // what reads outside the panels (the icon, the title, the menus) runs too
	reconciler.reconcile("revision");
}

void Engine::proposalsChanged() {
	document.globalsChanged();
	reconciler.requestReconcile();
}

} // namespace affresco::format
