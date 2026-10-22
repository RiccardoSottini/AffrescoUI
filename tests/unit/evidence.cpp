/// @file
/// @brief Unit tests for a proposal's evidence (VERSION_CONTROL.md, "Evidence"): the region where two pictures differ,
/// the "getEvidence" answer and its contract, and an author never supplying any of it.

#include "Helpers.hpp"
#include "pictures/ProposalEvidence.hpp" // modules/screen/src

#include <affresco/channel/Session.hpp>
#include <affresco/core/ScreenImages.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <array>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::changedRegion;
using affresco::detail::changedTexts;
using affresco::detail::RgbImage;
using affresco::test::MemoryChannel;
using affresco::test::permissionsScreen;

namespace {

/// @brief A picture of one colour.
/// @param width In pixels.
/// @param height In pixels.
/// @return It, all black.
RgbImage blank(int width, int height) {
	return {width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width * height * 3), 0)};
}

/// @brief Paints one pixel.
/// @param image The picture.
/// @param x Its column.
/// @param y Its row.
void paint(RgbImage& image, int x, int y) {
	const auto pixel =
	    static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x);
	image.rgb[pixel * 3 + 1] = 255;
}

/// @brief Evidence without a window: the repository's own behaviour lines, a fixed picture and one scenario, as a
/// host would gather them.
class FakeEvidence final : public affresco::ScreenImages {
public:
	/// @brief Gathers evidence on a document.
	/// @param versions The document; must outlive this object.
	explicit FakeEvidence(affresco::VersionControl& versions) : versions_(versions) {}

	[[nodiscard]] std::optional<std::string> render(const affresco::ImageRequest& /*request*/,
	                                                affresco::Image& /*image*/) override {
		return "no pictures here";
	}
	[[nodiscard]] bool rendering() const noexcept override { return false; }
	[[nodiscard]] std::optional<std::string> evidence(int proposal, int scale, affresco::Evidence& evidence) override {
		for(const affresco::ProposalInfo& info : versions_.proposals())
			if(info.id == proposal) {
				evidence.before = {std::string("\x89PNG\0", 5), 4 * scale, 3 * scale};
				evidence.after = evidence.before;
				evidence.region = {1, 1, 2, 1};
				evidence.behaviour = info.behaviour;
				evidence.scenarios = json::array({{{"name", "opens"}, {"passed", true}, {"message", ""}}});
				evidence.texts = json::array({{{"node", "#title"}, {"original", "A"}, {"modified", "B"}}});
				return std::nullopt;
			}
		return "no proposal " + std::to_string(proposal);
	}

private:
	affresco::VersionControl& versions_; ///< The document.
};

} // namespace

TEST_CASE("evidence: the changed region holds every pixel that differs, and nothing else") {
	const RgbImage before = blank(8, 6);
	CHECK(changedRegion(before, before) == std::array{0, 0, 0, 0});

	RgbImage after = before;
	paint(after, 2, 1);
	CHECK(changedRegion(before, after) == std::array{2, 1, 1, 1});
	paint(after, 6, 4);
	CHECK(changedRegion(before, after) == std::array{2, 1, 5, 4});

	// Pictures of other sizes differ everywhere
	CHECK(changedRegion(before, blank(4, 4)) == std::array{0, 0, 4, 4});
}

TEST_CASE("evidence: the texts a proposal changes, for the diff view") {
	const json main = json::parse(R"({"root": {"children": [{"id": "a", "text": "One"}, {"id": "b", "text": "Two"},
	                                   {"id": "c", "text": "Gone"}, {"text": "no id"}]}})");
	const json proposal = json::parse(R"({"root": {"children": [{"id": "a", "text": "One"}, {"id": "b", "text": "2",
	                                       "language": "markdown"}, {"id": "d", "text": "New"}]}})");
	const json texts = changedTexts(main, proposal);
	REQUIRE(texts.size() == 3);
	CHECK(texts[0] == json{{"node", "#b"}, {"original", "Two"}, {"modified", "2"}, {"language", "markdown"}});
	CHECK(texts[1] == json{{"node", "#d"}, {"original", ""}, {"modified", "New"}, {"language", "text"}});
	CHECK(texts[2] == json{{"node", "#c"}, {"original", "Gone"}, {"modified", ""}, {"language", "text"}});
	CHECK(changedTexts(main, main).empty());
}

TEST_CASE("evidence: an author can't supply any of it with a revision") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	for(const char* key : {"evidence", "behaviour"}) {
		const affresco::RevisionResult result = repository.submit(
		    {{"author", "agent"}, {"set", {{"#title/text", "Today"}}}, {key, json::array({"looks fine"})}});
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.find("the host gathers it itself") != std::string::npos);
	}
	CHECK(repository.proposals().empty());

	// A host without evidence says so
	struct NoEvidence final : affresco::ScreenImages {
		[[nodiscard]] std::optional<std::string> render(const affresco::ImageRequest& /*request*/,
		                                                affresco::Image& /*image*/) override {
			return std::nullopt;
		}
		[[nodiscard]] bool rendering() const noexcept override { return false; }
	} images;
	affresco::Evidence evidence;
	CHECK(images.evidence(1, 1, evidence));
}

TEST_CASE("evidence: getEvidence answers with the contract, behaviour lines from the host") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	MemoryChannel channel;
	FakeEvidence images(repository);
	const affresco::channel::Session session(channel, repository, "studio", {}, &images);

	const affresco::RevisionResult proposed = repository.submit(
	    {{"author", "agent"}, {"set", {{"#title/onClick", json::array({{{"set", "state.count"}, {"to", 1}}})}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);

	channel.sent.clear();
	channel.receive(json{{"type", "getEvidence"}, {"proposal", proposed.proposal}, {"id", 4}}.dump());
	REQUIRE(channel.sent.size() == 1);
	const json& answer = channel.sent[0];
	CHECK(answer["type"] == "evidence");
	CHECK(answer["id"] == 4);
	CHECK(answer["proposal"] == proposed.proposal);
	const json& evidence = answer["evidence"];
	CHECK(evidence["before"] == json{{"format", "png"}, {"data", "iVBORwA="}, {"width", 4}, {"height", 3}});
	CHECK(evidence["after"]["format"] == "png");
	CHECK(evidence["region"] == json::array({1, 1, 2, 1}));
	CHECK(evidence["behaviour"] == json::array({"adds onClick of #title"}));
	CHECK(evidence["scenarios"][0] == json{{"name", "opens"}, {"passed", true}, {"message", ""}});
	CHECK(evidence["texts"][0]["modified"] == "B");

	// No such proposal, a bad scale: errors, not evidence
	channel.sent.clear();
	channel.receive(R"({"type": "getEvidence", "proposal": 99, "id": 5})");
	channel.receive(json{{"type", "getEvidence"}, {"proposal", proposed.proposal}, {"scale", 9}, {"id", 6}}.dump());
	REQUIRE(channel.sent.size() == 2);
	CHECK(channel.sent[0]["type"] == "rejected");
	CHECK(channel.sent[1]["type"] == "rejected");
}
