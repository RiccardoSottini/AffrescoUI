/// @file
/// @brief Assets, run steps and attachments (VERSION_CONTROL.md, "Assets"): assets stored, addressed and checked in
/// the history, reverted and restored; "asset:" read by image nodes, sounds and the image step; "run" only when a
/// proposal is merged; attachments from the file dialog and drops; the review rules (only the person accepts, whatever
/// the rules, mode or grants say); and the bounds on names, types and sizes. Every file here is made by the test: an
/// SVG written below and a synthesised tone.

#include "AudioWav.hpp"
#include "Headless.hpp"
#include "Helpers.hpp"
#include "HistoryFiles.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/audio/Engine.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/render/ImageView.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
namespace audio = affresco::audio;

namespace {

/// A picture an agent could write: a moon over the sea, at night.
constexpr const char* moonSea =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="30" viewBox="0 0 40 30">)"
    R"(<rect width="40" height="30" fill="#0b1640"/><circle cx="28" cy="8" r="5" fill="#f4f1c9"/>)"
    R"(<rect y="20" width="40" height="10" fill="#123a6b"/></svg>)";

/// @brief A tenth of a second of a 440 Hz tone, as a WAV file's bytes.
/// @return The bytes.
std::string toneWav() {
	const std::vector<std::uint8_t> bytes =
	    affresco::test::wavFile(affresco::test::toneSamples(4800, 48000, 440), 48000, 1);
	return {bytes.begin(), bytes.end()};
}

/// @brief Base64 of bytes (RFC 4648, padded).
/// @param bytes The bytes.
/// @return The text.
std::string base64(const std::string& bytes) {
	static constexpr std::string_view digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string text;
	for(std::size_t i = 0; i < bytes.size(); i += 3) {
		std::uint32_t chunk = static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[i])) << 16U;
		if(i + 1 < bytes.size()) chunk |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[i + 1])) << 8;
		if(i + 2 < bytes.size()) chunk |= static_cast<std::uint8_t>(bytes[i + 2]);
		text += digits[(chunk >> 18) & 63];
		text += digits[(chunk >> 12) & 63];
		text += i + 1 < bytes.size() ? digits[(chunk >> 6) & 63] : '=';
		text += i + 2 < bytes.size() ? digits[chunk & 63] : '=';
	}
	return text;
}

/// @brief A screen with an image node, a sound and an image to edit, under permission rules.
/// @return The document.
json assetScreen() {
	json screen = affresco::test::permissionsScreen();
	screen["state"]["n"] = 0;
	screen["root"]["children"].push_back({{"type", "image"}, {"id", "cover"}, {"width", 40}, {"height", 30}});
	return screen;
}

/// @brief A revision bringing the SVG.
/// @param author Its author.
/// @return It.
json svgRevision(const std::string& author) {
	return {{"author", author},
	        {"message", "A night sea"},
	        {"assets", json::array({{{"name", "moon-sea.svg"}, {"type", "image/svg+xml"}, {"text", moonSea}}})}};
}

/// @brief The error a revision's assets give, or "".
/// @param assets The revision's assets.
/// @return The error.
std::string assetError(const json& assets) {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, assetScreen());
	const std::optional<std::string> error = repository.commit({{"author", "app"}, {"assets", assets}});
	return error ? *error : std::string();
}

} // namespace

TEST_CASE("assets: a revision's assets are kept by their SHA-256, named in the manifest, reverted and restored") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, assetScreen());
	REQUIRE_FALSE(repository.commit(svgRevision("app")));
	const std::vector<affresco::AssetInfo> assets = repository.assets();
	REQUIRE(assets.size() == 1);
	CHECK(assets[0].name == "moon-sea.svg");
	CHECK(assets[0].type == "image/svg+xml");
	CHECK(assets[0].size == std::string(moonSea).size());
	CHECK(assets[0].sha256 == affresco::sha256::hex(moonSea));
	CHECK(repository.document()["assets"]["moon-sea.svg"]["sha256"] == assets[0].sha256); // the commit covers it
	REQUIRE(repository.assetBytes("moon-sea.svg"));
	CHECK(*repository.assetBytes("moon-sea.svg") == moonSea);
	CHECK(repository.assetBytes("other.svg") == nullptr);

	// The same bytes in base64, under another name, are the same blob
	REQUIRE_FALSE(repository.commit(
	    {{"author", "app"},
		 {"assets", json::array({{{"name", "copy.svg"}, {"type", "image/svg+xml"}, {"data", base64(moonSea)}}})}}));
	CHECK(repository.assets()[0].sha256 == repository.assets()[1].sha256);

	REQUIRE_FALSE(repository.commit(repository.revertRevision("HEAD", "app"))); // copy.svg goes
	CHECK(repository.assets().size() == 1);
	REQUIRE_FALSE(repository.commit(repository.restoreRevision(0, "app"))); // the manifest goes
	CHECK(repository.assets().empty());
	CHECK_FALSE(repository.document().contains("assets"));
	REQUIRE_FALSE(repository.commit(repository.restoreRevision(1, "app"))); // the history still has the bytes
	REQUIRE(repository.assetBytes("moon-sea.svg"));
	CHECK(*repository.assetBytes("moon-sea.svg") == moonSea);
}

TEST_CASE("assets: names, types, sizes and bytes are bounded and checked; a manifest can't name bytes it lacks") {
	const json svg = {{"name", "moon.svg"}, {"type", "image/svg+xml"}, {"text", moonSea}};
	CHECK(assetError(json::array({svg})).empty());
	const auto with = [&](const char* key, const json& value) {
		json asset = svg;
		asset[key] = value;
		return assetError(json::array({asset}));
	};
	CHECK(with("name", "night/moon.svg").contains("no folders"));
	CHECK(with("name", "Moon.svg").contains("lowercase"));
	CHECK(with("name", ".moon.svg").contains("starting with"));
	CHECK(with("name", "moon.png").contains("must end with .svg"));
	CHECK(with("type", "image/gif").contains("isn't a type an asset may have"));
	CHECK(with("type", "image/png").contains("must end with .png"));
	CHECK(assetError(json::array({{{"name", "x.png"}, {"type", "image/png"}, {"text", "not a picture"}}}))
	          .contains("aren't image/png"));
	CHECK(assetError(json::array({{{"name", "x.svg"}, {"type", "image/svg+xml"}, {"text", "<svg"}}}))
	          .contains("doesn't decode"));
	CHECK(assetError(json::array({{{"name", "x.wav"}, {"type", "audio/wav"}, {"data", "%%%"}}})).contains("base64"));
	CHECK(assetError(json::array({{{"name", "x.txt"}, {"type", "text/plain"}, {"text", "a"}, {"data", "YQ=="}}}))
	          .contains("exactly one"));
	CHECK(assetError(json::array({svg, svg})).contains("twice"));
	CHECK(assetError(json::array({{{"name", "big.txt"},
	                               {"type", "text/plain"},
	                               {"text", std::string(affresco::maxAssetBytes + 1, 'a')}}}))
	          .contains("32 MiB"));
	const std::string half(affresco::maxAssetBytes - 1, 'a');
	CHECK(assetError(json::array({{{"name", "a.txt"}, {"type", "text/plain"}, {"text", half}},
	                              {{"name", "b.txt"}, {"type", "text/plain"}, {"text", half}},
	                              {{"name", "c.txt"}, {"type", "text/plain"}, {"text", "abc"}}}))
	          .contains("64 MiB"));

	// A patch may not forge an entry: the bytes must be in the history, of that size and type
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, assetScreen());
	const std::string id = affresco::sha256::hex("hello");
	const auto forged = repository.commit(
	    {{"author", "app"},
		 {"patch", json::array({{{"op", "add"},
		                         {"path", "/assets"},
		                         {"value", {{"x.txt", {{"type", "text/plain"}, {"size", 5}, {"sha256", id}}}}}}})}});
	REQUIRE(forged);
	CHECK(forged->contains("has no such bytes"));
	CHECK(repository.assets().empty());
}

TEST_CASE("assets: the history folder keeps the bytes, checks them on load, and refuses a history missing some") {
	const affresco::test::TemporaryFolder folder;
	const affresco::format::Rules rules;
	std::string id;
	{
		affresco::vcs::Repository repository(rules, assetScreen());
		static_cast<void>(repository.openHistory(folder.path() / "a.history"));
		REQUIRE_FALSE(repository.commit(svgRevision("app")));
		id = repository.assets().at(0).sha256;
		CHECK(std::filesystem::is_regular_file(folder.path() / "a.history" / "blobs" / id.substr(0, 2) / id.substr(2)));
	}
	{
		affresco::vcs::Repository repository(rules, assetScreen());
		CHECK(repository.openHistory(folder.path() / "a.history").loaded);
		REQUIRE(repository.assetBytes("moon-sea.svg"));
		CHECK(*repository.assetBytes("moon-sea.svg") == moonSea);
	}
	const std::filesystem::path blob = folder.path() / "a.history" / "blobs" / id.substr(0, 2) / id.substr(2);
	{
		std::ofstream(blob, std::ios::binary | std::ios::trunc) << std::string(std::string(moonSea).size(), 'x');
		affresco::vcs::Repository repository(rules, assetScreen());
		static_cast<void>(repository.openHistory(folder.path() / "a.history"));
		std::string logged;
		affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged += message; });
		CHECK(repository.assetBytes("moon-sea.svg") == nullptr); // damaged: never handed out
		affresco::setLogSink(nullptr);
		CHECK(logged.contains("doesn't match its SHA-256"));
	}
	std::filesystem::remove(blob);
	affresco::vcs::Repository repository(rules, assetScreen());
	CHECK_THROWS_WITH_AS(static_cast<void>(repository.openHistory(folder.path() / "a.history")),
	                     doctest::Contains("has no bytes of its size"), affresco::DocumentError);
}

TEST_CASE("assets: only the person accepts assets and run steps: no rule, mode, grant or other reviewer lets them") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, assetScreen());
	repository.setMode(affresco::Mode::AcceptEdits);

	// studio is allowed everything and reviews; agent may change #board: neither applies assets or run
	for(const char* author : {"studio", "agent"}) {
		CAPTURE(author);
		const affresco::RevisionResult result = repository.submit(svgRevision(author));
		REQUIRE(result.status == RevisionStatus::Proposed);
		CHECK(repository.proposals().back().reason.contains("only the person accepts"));
		CHECK(repository.proposals().back().remember.empty()); // "don't ask again" can't remember it
		CHECK(repository.proposals().back().person.at(0).contains("brings the asset 'moon-sea.svg'"));
		CHECK(repository.proposals().back().assets.at(0)["name"] == "moon-sea.svg");
	}
	const affresco::ProposalInfo agents = repository.proposals().back();
	auto refused = repository.accept(agents.id, {"studio", agents.head, false});
	REQUIRE(refused);
	CHECK(refused->contains("only the person may accept"));
	refused = repository.accept(agents.id, {"user", agents.head, true});
	REQUIRE(refused);
	CHECK(refused->contains("can't be remembered"));
	CHECK(repository.assets().empty()); // nothing landed
	CHECK_FALSE(repository.accept(agents.id, {"user", agents.head, false}));
	CHECK(repository.assets().size() == 1);
	CHECK(repository.grants().empty());

	// The person's own proposal isn't anyone else's to accept, and the person can't approve their own channel
	const affresco::ProposalInfo studios = repository.proposals().front();
	CHECK(repository.accept(studios.id, {"studio/ann", studios.head, false}).has_value());

	// run steps alone: the same
	const affresco::RevisionResult run =
	    repository.submit({{"author", "agent"}, {"run", json::array({{{"set", "state.n"}, {"to", 1}}})}});
	REQUIRE(run.status == RevisionStatus::Proposed);
	CHECK(repository.proposals().back().person.back().contains("runs 1 step once"));
	CHECK(repository.proposals().back().run.size() == 1);
	CHECK(repository.accept(run.proposal, {"studio", repository.proposals().back().head, false}).has_value());

	// Other capabilities stay protected for everyone, the person included
	for(const char* author : {"agent", "user"}) {
		const affresco::RevisionResult channels =
		    repository.submit({{"author", author}, {"set", {{"/channels", {{"x", {{"socket", "x.sock"}}}}}}}});
		CHECK(channels.status == RevisionStatus::Rejected);
		CHECK(channels.error.contains("can't change \"channels\""));
	}
}

TEST_CASE("assets: run steps are checked, and may not control changes or reach outside") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, assetScreen());
	const auto runs = [&](const json& steps) {
		const affresco::RevisionResult result = repository.submit({{"author", "agent"}, {"run", steps}});
		return result.status == RevisionStatus::Rejected ? result.error : std::string();
	};
	CHECK(runs(json::array({{{"set", "state.n"}, {"to", 2}}})).empty());
	CHECK(runs(json::array({{{"set", "state.n"}, {"to", "{{state.nope}}"}}})).contains("nope"));
	CHECK(runs(json::array({{{"call", "anything"}}})).contains("permission denied"));
	CHECK(runs(json::array({{{"accept", 1}, {"head", "abcdefg"}}})).contains("permission denied"));
	CHECK(runs(json::array({{{"attach", json::object()}}})).contains("permission denied"));
	CHECK(runs(json::array()).contains("expected a list of steps"));
	CHECK(runs("{\"set\": 1}").contains("expected a list of steps"));
}

TEST_CASE("assets: the audio section changes only with files or assets, and only the person accepts it") {
	const affresco::format::Rules rules;
	json screen = assetScreen();
	screen["audio"] = {{"song", {{"tone", {{"frequency", 440}, {"seconds", 1}}}, {"into", "state.song"}}}};
	affresco::vcs::Repository repository(rules, screen);

	const affresco::RevisionResult url =
	    repository.submit({{"author", "agent"}, {"set", {{"/audio/song", {{"url", "https://example.com/a.mp3"}}}}}});
	CHECK(url.status == RevisionStatus::Rejected);
	CHECK(url.error.contains("never a \"url\""));
	const affresco::RevisionResult missing =
	    repository.submit({{"author", "agent"}, {"set", {{"/audio/song", {{"file", "asset:beat.wav"}}}}}});
	CHECK(missing.status == RevisionStatus::Rejected);
	CHECK(missing.error.contains("no asset 'beat.wav'"));

	const affresco::RevisionResult sound = repository.submit(
	    {{"author", "studio"},
		 {"assets", json::array({{{"name", "beat.wav"}, {"type", "audio/wav"}, {"data", base64(toneWav())}}})},
		 {"set", {{"/audio/song", {{"file", "asset:beat.wav"}, {"into", "state.song"}}}}}});
	REQUIRE(sound.status == RevisionStatus::Proposed);
	const std::vector<std::string> person = repository.proposals().back().person;
	CHECK(
	    std::ranges::any_of(person, [](const std::string& line) { return line.contains("changes the sound 'song'"); }));
	// A change of the sounds alone is the person's too
	const affresco::RevisionResult tone =
	    repository.submit({{"author", "studio"}, {"set", {{"/audio/song/tone/frequency", 880}}}});
	REQUIRE(tone.status == RevisionStatus::Proposed);
	CHECK(repository.proposals().back().person.at(0).contains("changes the sound 'song'"));
}

TEST_CASE("assets: asset: shows in an image node, plays as a sound, and opens in the image step") {
	affresco::test::ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	json document = assetScreen();
	document["audio"] = {{"beat", {{"tone", {{"frequency", 440}, {"seconds", 1}}}, {"into", "state.beat"}}}};
	document["images"] = {{"canvas", {{"width", 8}, {"height", 8}, {"into", "state.canvas"}}}};
	document["actions"] = {{"play", json::array({{{"audio", "play"}, {"of", "beat"}}})}};
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	audio::EngineOptions options;
	options.output = audio::Output::Manual;
	options.executor = &executor;
	screen.setAudio(std::make_shared<audio::Engine>(options));
	screen.setExecutor(&executor);
	screen.build(*window);

	const affresco::RevisionResult result = screen.submitRevision(
	    json{{"author", "agent"},
		     {"assets", json::array({{{"name", "moon-sea.svg"}, {"type", "image/svg+xml"}, {"text", moonSea}},
		                             {{"name", "beat.wav"}, {"type", "audio/wav"}, {"data", base64(toneWav())}}})},
		     {"set",
		      {{"#cover/file", "asset:moon-sea.svg"},
		       {"/audio/beat", {{"file", "asset:beat.wav"}, {"into", "state.beat"}}}}},
		     {"run", json::array({{{"image", "open"}, {"of", "canvas"}, {"with", {{"asset", "moon-sea.svg"}}}}})}}
	        .dump());
	REQUIRE(result.status == RevisionStatus::Proposed);
	REQUIRE(screen.acceptProposal(result.proposal, result.commit));
	window->runDeferredTasks();
	window->render();

	CHECK(screen.assets().size() == 2);
	const auto* cover = dynamic_cast<affresco::ImageView*>(screen.find("cover"));
	REQUIRE(cover != nullptr);
	REQUIRE(cover->bitmap());
	CHECK(cover->bitmap()->width() >= 40);
	const json state = json::parse(screen.stateJson());
	CHECK(state["canvas"]["width"] >= 40); // the run step opened the SVG in the image
	screen.runAction("play", window->root());
	const json played = json::parse(screen.stateJson());
	CHECK(played["beat"]["status"] == "playing");
	CHECK(played["beat"]["duration"] == doctest::Approx(0.1));

	// The checker finds an asset: the document doesn't have
	const affresco::format::Rules rules;
	CHECK(rules.findings(json::parse(screen.documentJson()), state).empty());
	json typo = json::parse(screen.documentJson());
	typo["root"]["children"][3]["file"] = "asset:moon.svg";
	const std::vector<std::string> findings = rules.findings(typo, state);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].contains("unknown asset 'asset:moon.svg'"));
}

TEST_CASE("assets: run steps run once when the proposal is merged, never in its preview or on a restore") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(assetScreen().dump());
	screen.build(*window);
	const auto n = [&] { return json::parse(screen.stateJson())["n"]; };
	const affresco::RevisionResult result = screen.submitRevision(json{
	    {"author", "agent"},
	    {"set", {{"#title/text", "Done"}}},
	    {"run", json::array({{{"increment", "state.n"}}})}}.dump());
	REQUIRE(result.status == RevisionStatus::Proposed);
	REQUIRE(screen.versions().preview(result.proposal) == std::nullopt);
	window->runDeferredTasks();
	CHECK(n() == 0);
	screen.versions().endPreview();
	REQUIRE(screen.acceptProposal(result.proposal, result.commit));
	window->runDeferredTasks();
	CHECK(n() == 1);
	CHECK(screen.versions().head().run.size() == 1); // recorded in the merge commit
	REQUIRE(screen.applyRevision(screen.versions().restoreRevision(0, "user").dump()));
	window->runDeferredTasks();
	CHECK(n() == 1);
	CHECK(screen.versions().head().run.is_null());
}

TEST_CASE("assets: the person attaches files through the dialog or a drop; agents see metadata and name them") {
	const affresco::test::TemporaryFolder folder;
	{
		const std::string wav = toneWav();
		std::ofstream(folder.path() / "My Beat.WAV", std::ios::binary)
		    .write(wav.data(), static_cast<std::streamsize>(wav.size()));
		std::ofstream(folder.path() / "notes.exe", std::ios::binary) << "MZ";
	}
	affresco::test::MemoryChannel agent;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	json document = assetScreen();
	document["state"]["attached"] = nullptr;
	document["state"]["failed"] = nullptr;
	document["actions"] = {
	    {"pick", json::array({{{"attach", {{"accept", json::array({"audio/*"})}, {"multiple", true}}},
		                       {"into", "state.attached"},
		                       {"onError", json::array({{{"set", "state.failed"}, {"to", "{{error.message}}"}}})}}})},
	    {"drop", json::array({{{"attach", {{"from", "{{event.files}}"}}}, {"into", "state.attached"}}})},
	    {"send", json::array({{{"ask", "Make it the intro"}, {"attachments", "{{state.attached}}"}}})}};
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	screen.connect(agent, "agent");
	std::vector<affresco::FileDialog> asked;
	std::vector<std::string> chosen = {(folder.path() / "My Beat.WAV").string()};
	window->setFileDialogHandler([&](const affresco::FileDialog& dialog) {
		asked.push_back(dialog);
		return chosen;
	});

	screen.runAction("pick", window->root());
	window->runDeferredTasks();
	REQUIRE(asked.size() == 1);
	CHECK(asked[0].multiple);
	CHECK(std::ranges::contains(asked[0].filters.at(0).extensions, "wav"));
	const json attached = json::parse(screen.stateJson())["attached"];
	REQUIRE(attached.size() == 1);
	CHECK(attached[0]["name"] == "my-beat.wav");
	CHECK(attached[0]["type"] == "audio/wav");
	CHECK(attached[0]["duration"] == doctest::Approx(0.1));
	CHECK_FALSE(attached[0].contains("data"));

	// A file of another kind is refused, and says so
	chosen = {(folder.path() / "notes.exe").string()};
	screen.runAction("pick", window->root());
	window->runDeferredTasks();
	CHECK(json::parse(screen.stateJson())["failed"].get<std::string>().contains("isn't a kind of file"));

	// The request carries the metadata, never the bytes
	screen.runAction("send", window->root());
	window->runDeferredTasks();
	const auto request =
	    std::ranges::find_if(agent.sent, [](const json& m) { return m.value("type", "") == "request"; });
	REQUIRE(request != agent.sent.end());
	REQUIRE((*request)["attachments"].size() == 1);
	CHECK((*request)["attachments"][0]["name"] == "my-beat.wav");
	CHECK((*request)["attachments"][0]["size"] == toneWav().size());

	// The agent names the attachment in a revision: a proposal with the asset, for the person
	agent.receive(json{{"type", "revision"},
	                   {"assets", json::array({{{"name", "intro.wav"}, {"attachment", attached[0]["id"]}}})}}
	                  .dump());
	const auto proposed =
	    std::ranges::find_if(agent.sent, [](const json& m) { return m.value("type", "") == "proposed"; });
	REQUIRE(proposed != agent.sent.end());
	const affresco::ProposalInfo proposal = screen.proposals().back();
	CHECK(proposal.assets.at(0)["name"] == "intro.wav");
	CHECK(proposal.assets.at(0)["type"] == "audio/wav");
	REQUIRE(screen.acceptProposal(proposal.id, proposal.head));
	REQUIRE(screen.assetBytes("intro.wav"));
	CHECK(*screen.assetBytes("intro.wav") == toneWav());

	// Only files the person dropped may be attached from "from"
	std::string logged;
	affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged += message; });
	screen.runAction("drop", window->root(), json{{"files", {(folder.path() / "My Beat.WAV").string()}}}.dump());
	affresco::setLogSink(nullptr);
	CHECK(logged.contains("only files the person dropped"));

	// An agent can neither add an attach step nor attach by name
	const affresco::RevisionResult added = screen.submitRevision(
	    json{{"author", "agent"}, {"set", {{"/actions/grab", json::array({{{"attach", json::object()}}})}}}}.dump());
	CHECK(added.status == RevisionStatus::Rejected);
	CHECK(added.error.contains("permission denied"));
	const affresco::RevisionResult unknown = screen.submitRevision(
	    json{{"author", "agent"}, {"assets", json::array({{{"name", "x.wav"}, {"attachment", "att-99-000000000000"}}})}}
	        .dump());
	CHECK(unknown.status == RevisionStatus::Rejected);
	CHECK(unknown.error.contains("no attachment"));
}

TEST_CASE("assets: attachments are bounded, the oldest going first") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, assetScreen());
	std::string first;
	for(std::size_t i = 0; i <= affresco::maxAttachments; ++i) {
		const affresco::Attached attached =
		    repository.attach("note " + std::to_string(i) + ".txt", "text/plain", "n" + std::to_string(i), {});
		REQUIRE(attached.error.empty());
		CHECK(attached.attachment.name == "note-" + std::to_string(i) + ".txt");
		if(i == 0) first = attached.attachment.id;
	}
	CHECK_FALSE(repository.attachment(first));
	CHECK(repository.attach("x.png", "image/png", "not a png", {}).error.contains("aren't image/png"));
	CHECK(repository.attach("x.gif", "image/gif", "GIF89a", {}).error.contains("isn't a type"));
}

TEST_CASE("assets: the checker knows what an attach step gives its handlers") {
	json document = assetScreen();
	document["state"]["files"] = nullptr;
	document["state"]["problem"] = nullptr;
	document["actions"] = {
	    {"pick", json::array({{{"attach", json::object()},
	                           {"onDone", json::array({{{"set", "state.files"}, {"to", "{{result}}"}},
	                                                   {{"set", "state.problem"}, {"to", "{{error.message}}"}}})},
	                           {"onError", json::array({{{"set", "state.problem"}, {"to", "{{error.code}}"}}})}}})}};
	const affresco::format::Rules rules;
	CHECK(rules.findings(document, document["state"]).empty());
}
