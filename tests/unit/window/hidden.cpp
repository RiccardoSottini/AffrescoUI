/// @file
/// @brief AFFRESCOUI_HIDDEN: the environment opens every window of a program hidden, and keeps it off screen even when
/// the program asks to show it.

#include <affresco/affresco.hpp>
#include <affresco/core/Portable.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>

namespace {

/// @brief Sets an environment variable for as long as it lives, then puts the old value back.
class ScopedEnvironment {
public:
	/// @brief Sets it.
	/// @param name The variable.
	/// @param value Its value for now.
	ScopedEnvironment(const char* name, const char* value) : name_(name) {
		old_ = affresco::environmentVariable(name);
		set(value);
	}
	/// @brief Puts the old value back, or unsets it.
	~ScopedEnvironment() { set(old_ ? old_->c_str() : nullptr); }

	ScopedEnvironment(const ScopedEnvironment&) = delete;
	ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
	ScopedEnvironment(ScopedEnvironment&&) = delete;
	ScopedEnvironment& operator=(ScopedEnvironment&&) = delete;

private:
	/// @brief Sets or unsets the variable.
	/// @param value Its value, or nullptr to unset it.
	void set(const char* value) const {
#ifdef _WIN32
		_putenv_s(name_, value ? value : "");
#else
		if(value) setenv(name_, value, 1);
		else unsetenv(name_);
#endif
	}

	const char* name_;               ///< The variable (a literal).
	std::optional<std::string> old_; ///< Its value before, if it had one.
};

/// @brief A small screen, for a window.
constexpr const char* screenText = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"window": { "title": "hidden", "width": 120, "height": 80 },
	"root": { "type": "panel", "id": "main", "color": "#336699" }
})json";

} // namespace

TEST_CASE("window: hiddenWindowsRequested reads AFFRESCOUI_HIDDEN's value") {
	CHECK_FALSE(affresco::hiddenWindowsRequested(nullptr));
	CHECK_FALSE(affresco::hiddenWindowsRequested(""));
	CHECK_FALSE(affresco::hiddenWindowsRequested("0"));
	CHECK(affresco::hiddenWindowsRequested("1"));
	CHECK(affresco::hiddenWindowsRequested("yes"));
	CHECK(affresco::hiddenWindowsRequested("00")); // only "0" turns it off
}

TEST_CASE("window: AFFRESCOUI_HIDDEN=1 keeps every window hidden, and screenshots still draw") {
	const ScopedEnvironment hidden("AFFRESCOUI_HIDDEN", "1");
	affresco::Application app; // shown windows, but the environment wins
	affresco::Screen screen = affresco::Screen::fromString(screenText);
	affresco::Window& window = screen.createWindow(app);
	CHECK_FALSE(window.isVisible());
	window.setVisible(true); // a program showing its window: it still never comes on screen
	CHECK_FALSE(window.isVisible());

	const auto frame = window.renderToImage(screen.get("main"), 1.0f); // drawn offscreen
	REQUIRE(frame);
	REQUIRE(frame->width() == 120);
	const auto rgba = frame->pixels();
	const std::size_t middle = (std::size_t{40} * 120 + 60) * 4;
	CHECK(rgba[middle] == 0x33);
	CHECK(rgba[middle + 1] == 0x66);
	CHECK(rgba[middle + 2] == 0x99);
}

TEST_CASE("window: AFFRESCOUI_HIDDEN=0 leaves windows as the program opens them") {
	const ScopedEnvironment shown("AFFRESCOUI_HIDDEN", "0");
	affresco::Application app({nullptr, true}); // the program's own choice still holds
	affresco::Screen screen = affresco::Screen::fromString(screenText);
	CHECK_FALSE(screen.createWindow(app).isVisible());
}
