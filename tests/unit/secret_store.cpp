/// @file
/// @brief SecretStore: the in-memory store keeps, reads and forgets secrets by key, and the system's store names
/// itself (without being written to: a test must not touch the person's Keychain).

#include <affresco/core/SecretStore.hpp>
#include <affresco/net/SignIn.hpp>

#include <doctest/doctest.h>

#include <string>
#include <thread>
#include <vector>

TEST_CASE("secret store: memory keeps, reads and forgets secrets by key") {
	affresco::MemorySecretStore store;
	CHECK(store.name() == "memory");
	CHECK_FALSE(store.read("affrescoui/test/a").has_value());
	CHECK(store.write("affrescoui/test/a", "first"));
	CHECK(store.write("affrescoui/test/b", "second"));
	CHECK(store.read("affrescoui/test/a") == "first");
	CHECK(store.write("affrescoui/test/a", "replaced"));
	CHECK(store.read("affrescoui/test/a") == "replaced");
	store.erase("affrescoui/test/a");
	CHECK_FALSE(store.read("affrescoui/test/a").has_value());
	CHECK(store.read("affrescoui/test/b") == "second");
	store.erase("affrescoui/test/missing"); // forgetting what isn't there is no error
}

TEST_CASE("secret store: memory may be used from several threads at once") {
	affresco::MemorySecretStore store;
	std::vector<std::thread> threads;
	for(int t = 0; t < 4; ++t)
		threads.emplace_back([&store, t] {
			const std::string key = "affrescoui/test/" + std::to_string(t);
			for(int i = 0; i < 200; ++i) {
				CHECK(store.write(key, std::to_string(i)));
				CHECK(store.read(key).has_value());
			}
			store.erase(key);
		});
	for(std::thread& thread : threads) thread.join();
	for(int t = 0; t < 4; ++t) CHECK_FALSE(store.read("affrescoui/test/" + std::to_string(t)).has_value());
}

TEST_CASE("secret store: the system's store names itself") {
	const auto store = affresco::net::systemSecretStore("affrescoui-tests");
	REQUIRE(store != nullptr);
	CHECK_FALSE(store->name().empty());
}
