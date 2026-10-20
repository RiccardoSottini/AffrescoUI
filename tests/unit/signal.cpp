/// @file
/// @brief core's Signal: slots run in order, and may connect or disconnect slots, themselves too, while it emits.

#include <affresco/core/Signal.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

TEST_CASE("core: a signal calls its slots in the order they were connected") {
	affresco::Signal<int> signal;
	std::vector<std::string> calls;
	CHECK(signal.empty());
	signal.connect([&calls](int value) { calls.push_back("a" + std::to_string(value)); });
	const affresco::Connection second =
	    signal.connect([&calls](int value) { calls.push_back("b" + std::to_string(value)); });
	CHECK_FALSE(signal.empty());
	signal.emit(1);
	CHECK(signal.disconnect(second));
	CHECK_FALSE(signal.disconnect(second));
	signal(2);
	CHECK(calls == std::vector<std::string>{"a1", "b1", "a2"});
}

// The slots are kept in a vector: one connected while another runs must not move the one running
TEST_CASE("core: slots connected during an emission run from the next one, and the running slot never moves") {
	affresco::Signal<> signal;
	int added = 0;
	int ran = 0;
	signal.connect([&] {
		const std::string kept = "a capture that would dangle if this slot moved";
		for(int i = 0; i < 64; ++i) signal.connect([&ran] { ++ran; });
		added += 64;
		CHECK(kept.size() == 46);
	});
	signal.emit();
	CHECK(added == 64);
	CHECK(ran == 0);
	signal.emit();
	CHECK(added == 128);
	CHECK(ran == 64);
}

TEST_CASE("core: a slot can disconnect itself, another slot, or one connected during the same emission") {
	affresco::Signal<> signal;
	std::vector<std::string> calls;
	affresco::Connection self = 0;
	affresco::Connection later = 0;
	affresco::Connection waiting = 0;
	self = signal.connect([&] {
		calls.emplace_back("self");
		CHECK(signal.disconnect(self));
		waiting = signal.connect([&calls] { calls.emplace_back("waiting"); });
		CHECK(signal.disconnect(waiting)); // never ran
		CHECK(signal.disconnect(later));
	});
	later = signal.connect([&calls] { calls.emplace_back("later"); });
	signal.emit();
	signal.emit();
	CHECK(calls == std::vector<std::string>{"self"});
	CHECK(signal.empty());
}

TEST_CASE("core: a signal emitted from one of its own slots finishes both emissions") {
	affresco::Signal<int> signal;
	std::vector<int> seen;
	signal.connect([&](int depth) {
		seen.push_back(depth);
		if(depth == 0) {
			signal.connect([&seen](int value) { seen.push_back(100 + value); });
			signal.emit(1); // the slot just connected waits for the outermost emission to end
		}
	});
	signal.emit(0);
	CHECK(seen == std::vector<int>{0, 1});
	signal.emit(2);
	CHECK(seen == std::vector<int>{0, 1, 2, 102});
}
