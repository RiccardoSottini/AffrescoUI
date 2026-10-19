/// @file
/// @brief Unit tests for the channel module: the live protocol (Session) over a Channel kept in memory.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>

using affresco::json;
using affresco::test::MemoryChannel;

namespace {

/// @brief A screen under version control, and a peer connected to it.
struct Peer {
	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	MemoryChannel channel;                                                          ///< What the peer receives.
};

} // namespace

TEST_CASE("channel: a Session sends hello when created") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	REQUIRE(peer.channel.sent.size() == 1);
	const json& hello = peer.channel.sent[0];
	CHECK(hello.at("type") == "hello");
	CHECK(hello.at("format") == "affrescoui-screen");
	CHECK(hello.at("revision") == 0);
	CHECK(hello.at("head").at("id") == peer.versions.head().id);
	CHECK(hello.at("document") == peer.versions.document());
	CHECK(hello.at("state") == json{{"count", 0}});
	CHECK(hello.at("mode") == "default");
	CHECK(hello.at("you") == "agent");
	CHECK(hello.at("canReview") == false);
}

TEST_CASE("channel: a message of an unknown type gets an error") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive(R"({"type": "launchRockets", "id": 7})");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1] ==
	      json{{"type", "error"}, {"message", "unknown message type 'launchRockets'"}, {"id", 7}});

	peer.channel.receive("not json");
	REQUIRE(peer.channel.sent.size() == 3);
	CHECK(peer.channel.sent[2]["type"] == "error");
	CHECK(peer.versions.head().number == 0);
}

TEST_CASE("channel: an accept from a peer that may not review is rejected") {
	Peer peer;
	const affresco::channel::Session agent(peer.channel, peer.versions, "agent");

	// The agent proposes a change it may not make directly, then tries to approve it itself
	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"#title/text": "Mine"}})");
	const auto proposed =
	    std::ranges::find(peer.channel.sent, json("proposed"), [](const json& m) { return m.value("type", json()); });
	REQUIRE(proposed != peer.channel.sent.end());
	const int proposal = (*proposed)["proposal"];
	const std::string head = (*proposed)["commit"];

	peer.channel.sent.clear();
	peer.channel.receive(json{{"type", "accept"}, {"id", 2}, {"proposal", proposal}, {"head", head}}.dump());
	REQUIRE(peer.channel.sent.size() == 1);
	CHECK(peer.channel.sent[0] == json{{"type", "rejected"},
	                                   {"error", "permission denied: 'agent' may not review changes"},
	                                   {"revision", 0},
	                                   {"id", 2}});
	CHECK(peer.versions.proposals().size() == 1); // still waiting for a reviewer
	CHECK(peer.versions.head().number == 0);
}

namespace {

/// @brief Pictures without a window: keeps each request, and previews the proposal asked for while "drawing", as
/// the real one does.
class FakeImages final : public affresco::ScreenImages {
public:
	/// @brief Draws pictures of a document.
	/// @param versions The document; must outlive this object.
	explicit FakeImages(affresco::VersionControl& versions) : versions_(versions) {}

	[[nodiscard]] std::optional<std::string> render(const affresco::ImageRequest& request,
	                                                affresco::Image& image) override {
		requests.push_back(request);
		if(error) return error;
		rendering_ = true;
		std::optional<std::string> failure;
		if(request.proposal) {
			failure = versions_.preview(*request.proposal);
			versions_.endPreview();
		}
		rendering_ = false;
		image = {std::string("\x89PNG\0", 5), 4 * request.scale, 3 * request.scale};
		return failure;
	}
	[[nodiscard]] bool rendering() const noexcept override { return rendering_; }

	std::vector<affresco::ImageRequest> requests; ///< Every request, oldest first.
	std::optional<std::string> error;             ///< Returned instead of a picture, if set.

private:
	affresco::VersionControl& versions_; ///< The document.
	bool rendering_ = false;             ///< See rendering().
};

} // namespace

TEST_CASE("channel: render answers with the picture as base64 PNG") {
	Peer peer;
	FakeImages images(peer.versions);
	const affresco::channel::Session session(peer.channel, peer.versions, "agent", {}, &images);

	peer.channel.sent.clear();
	peer.channel.receive(R"({"type": "render", "id": 5})");
	REQUIRE(peer.channel.sent.size() == 1);
	CHECK(peer.channel.sent[0] == json{{"type", "image"},
	                                   {"format", "png"},
	                                   {"data", "iVBORwA="},
	                                   {"width", 4},
	                                   {"height", 3},
	                                   {"scale", 1},
	                                   {"proposal", nullptr},
	                                   {"revision", 0},
	                                   {"id", 5}});
	REQUIRE(images.requests.size() == 1);
	CHECK_FALSE(images.requests[0].proposal);
	CHECK(images.requests[0].scale == 1);
}

TEST_CASE("channel: render of a proposal sends no review changes to anyone") {
	Peer peer;
	FakeImages images(peer.versions);
	affresco::test::MemoryChannel studioChannel;
	const affresco::channel::Session agent(peer.channel, peer.versions, "agent", {}, &images);
	const affresco::channel::Session studio(studioChannel, peer.versions, "studio", {}, &images);

	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"#title/text": "Mine"}})");
	REQUIRE(peer.versions.proposals().size() == 1);
	const int proposal = peer.versions.proposals()[0].id;

	peer.channel.sent.clear();
	studioChannel.sent.clear();
	peer.channel.receive(json{{"type", "render"}, {"id", 2}, {"proposal", proposal}, {"scale", 2}}.dump());
	REQUIRE(peer.channel.sent.size() == 1);
	CHECK(peer.channel.sent[0]["type"] == "image");
	CHECK(peer.channel.sent[0]["proposal"] == proposal);
	CHECK(peer.channel.sent[0]["width"] == 8);
	CHECK(studioChannel.sent.empty());
	CHECK_FALSE(peer.versions.previewing());
	REQUIRE(images.requests.size() == 1);
	CHECK(images.requests[0].proposal == proposal);
	CHECK(images.requests[0].scale == 2);
}

TEST_CASE("channel: render refuses what it can't draw") {
	Peer peer;
	FakeImages images(peer.versions);
	const affresco::channel::Session session(peer.channel, peer.versions, "agent", {}, &images);
	const auto answer = [&](const std::string& line) {
		peer.channel.sent.clear();
		peer.channel.receive(line);
		REQUIRE(peer.channel.sent.size() == 1);
		return peer.channel.sent[0];
	};

	for(const char* scale : {"0", "3", "1.5", "\"2\""}) {
		CAPTURE(scale);
		const json rejected = answer(std::string(R"({"type": "render", "id": 1, "scale": )") + scale + "}");
		CHECK(rejected["type"] == "rejected");
		CHECK(rejected["error"].get<std::string>().starts_with("AffrescoUI screen: render.scale: "));
	}
	CHECK(answer(R"({"type": "render", "proposal": "1"})")["error"] ==
	      "AffrescoUI screen: render.proposal: expected an integer");
	CHECK(images.requests.empty());

	const json missing = answer(R"({"type": "render", "id": 3, "proposal": 99})");
	CHECK(missing ==
	      json{{"type", "rejected"}, {"error", "AffrescoUI screen: no proposal 99"}, {"revision", 0}, {"id", 3}});

	images.error = "AffrescoUI screen: render: the screen has no window yet";
	CHECK(answer(R"({"type": "render"})")["error"] == *images.error);

	MemoryChannel other;
	const affresco::channel::Session blind(other, peer.versions, "agent"); // a host that draws no pictures
	other.receive(R"({"type": "render"})");
	REQUIRE(other.sent.size() == 2); // hello, then the answer
	CHECK(other.sent[1]["error"] == "AffrescoUI screen: render: this host draws no pictures");
}
