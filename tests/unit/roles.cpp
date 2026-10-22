/// @file
/// @brief Unit tests for several agents at once (VERSION_CONTROL.md, "Roles" and "Leases"): two agents on the Kanban
/// template, one leasing a column and the other refused until the lease ends; and that an agent can never grant itself
/// a role, a lease beyond its rules, or end another's lease.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::channel::Session;
using affresco::test::MemoryChannel;

namespace {

/// @brief The Kanban template's document, its state in "state", as a host gives it to its repository.
/// @return The document.
json kanban() {
	const std::filesystem::path directory = std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "flow";
	const std::ifstream file(directory / "kanban.screen.json");
	REQUIRE(file);
	std::stringstream text;
	text << file.rdbuf();
	affresco::Screen screen = affresco::Screen::fromString(text.str(), directory);
	screen.setPlaceholderTypes(true);
	json document = json::parse(screen.documentJson());
	document["state"] = json::parse(screen.stateJson());
	return document;
}

/// @brief One peer: a channel and the session on it.
struct Peer {
	MemoryChannel channel;            ///< What the peer was sent.
	std::unique_ptr<Session> session; ///< Its session.

	/// @brief Connects a peer.
	/// @param versions The document's version control.
	/// @param author Its channel, e.g. "agent".
	Peer(affresco::VersionControl& versions, const std::string& author)
	    : session(std::make_unique<Session>(channel, versions, author, std::function<json()>{})) {}

	/// @brief Sends a message and returns the answer to it.
	/// @param message The message; given the id 1.
	/// @return The last message sent back with that id.
	json ask(json message) {
		message["id"] = 1;
		channel.receive(message.dump());
		for(auto each = channel.sent.rbegin(); each != channel.sent.rend(); ++each)
			if(each->value("id", json()) == 1) return *each;
		return {};
	}
};

/// @brief A revision that renames a Kanban column.
/// @param name The agent's own name.
/// @param column Which column.
/// @return The message.
json renameColumn(const std::string& name, int column) {
	return {{"type", "revision"},
	        {"author", name},
	        {"patch",
	         {{{"op", "replace"}, {"path", "/state/columns/" + std::to_string(column) + "/name"}, {"value", "Next"}}}}};
}

} // namespace

TEST_CASE("roles: two agents on one template, one leases a column, the other is refused until the lease ends") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, kanban());
	Peer studio(repository, "studio");
	Peer ada(repository, "agent");
	auto bob = std::make_unique<Peer>(repository, "agent");
	CHECK(repository.coordination()["peers"].size() == 3);

	const json leased = ada.ask({{"type", "lease"}, {"author", "ada"}, {"target", "/state/columns/0"}});
	REQUIRE(leased["type"] == "leased");
	CHECK(repository.coordination()["leases"][0]["holder"] == "agent/ada");
	CHECK(bob->ask({{"type", "lease"}, {"author", "bob"}, {"target", "/state/columns/0/cards"}})["type"] == "rejected");

	// Bob's change is refused, naming the holder; Ada's own goes on (proposed: agents ask on Kanban)
	const json refused = bob->ask(renameColumn("bob", 0));
	CHECK(refused["type"] == "rejected");
	CHECK(refused["error"].get<std::string>().find("'agent/ada' holds a lease on /state/columns/0") !=
	      std::string::npos);
	CHECK(bob->ask(renameColumn("bob", 1))["type"] != "rejected");
	CHECK(ada.ask(renameColumn("ada", 0))["type"] != "rejected");
	// Bob can't take Ada's name, on another connection, to pass
	CHECK(bob->ask({{"type", "lease"}, {"author", "ada"}, {"target", "/state/columns/2"}})["type"] == "rejected");

	// Released, then the lease ends with the connection
	CHECK(ada.ask({{"type", "release"}, {"target", "/state/columns/0"}})["type"] == "released");
	REQUIRE(bob->ask({{"type", "lease"}, {"author", "bob"}, {"target", "/state/columns/0"}})["type"] == "leased");
	CHECK(ada.ask(renameColumn("ada", 0))["type"] == "rejected");
	bob.reset();
	CHECK(repository.coordination()["leases"].empty());
	CHECK(repository.coordination()["peers"].size() == 2);
	CHECK(ada.ask(renameColumn("ada", 0))["type"] != "rejected");
	// Every peer heard who is connected
	CHECK(studio.channel.sent.back()["type"] == "review");
	CHECK(studio.channel.sent.back().contains("coordination"));
}

TEST_CASE("roles: an agent can't give itself a role, a lease beyond its rules, or end another's lease") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, kanban());
	Peer studio(repository, "studio");
	Peer ada(repository, "agent");
	Peer bob(repository, "agent");

	// Roles and ending leases are a reviewer's
	CHECK(ada.ask({{"type", "setRole"}, {"author", "agent/ada"}, {"role", "design"}})["type"] == "rejected");
	CHECK(ada.ask({{"type", "setRole"}, {"author", "agent/bob"}, {"role", "design"}})["type"] == "rejected");
	CHECK(repository.roles().of("agent/ada").empty());
	REQUIRE(bob.ask({{"type", "lease"}, {"author", "bob"}, {"target", "/state/columns/1"}})["type"] == "leased");
	CHECK(ada.ask({{"type", "endLease"}, {"target", "/state/columns/1"}})["type"] == "rejected");
	CHECK(repository.coordination()["leases"].size() == 1);
	// A lease on a part its rules deny it is refused: the status bar is denied to everyone
	const json beyond = ada.ask({{"type", "lease"}, {"author", "ada"}, {"target", "#statusBar"}});
	CHECK(beyond["type"] == "rejected");
	CHECK(beyond["error"].get<std::string>().find("beyond") != std::string::npos);
	CHECK(ada.ask(
	          {{"type", "lease"}, {"author", "ada"}, {"target", "/state/columns/2"}, {"seconds", 99999}})["seconds"] ==
	      3600);

	// A reviewer sets an agent's role and ends a lease; nobody sets their own role
	CHECK(studio.ask({{"type", "setRole"}, {"author", "agent/ada"}, {"role", "design"}})["type"] == "done");
	CHECK(repository.roles().of("agent/ada") == "design");
	CHECK(repository.setRole("studio/ann", "admin", "studio/ann"));
	CHECK(repository.setRole("agent/ada", "admin", "agent/bob"));
	CHECK(studio.ask({{"type", "endLease"}, {"target", "/state/columns/1"}})["type"] == "done");
	CHECK(repository.coordination()["leases"].size() == 1); // Ada's, on column 2
	// A revision can't carry a lease: a "lease" key is not one
	CHECK(bob.ask({{"type", "revision"}, {"author", "bob"}, {"lease", "#board"}, {"set", {{"/state/x", 1}}}})["type"] !=
	      "leased");
}
