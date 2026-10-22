/// @file
/// @brief The accessibility tree a window exposes on Linux, read back through AT-SPI as Orca reads it: roles, names,
/// a slider's value and a press. Compiled on Linux when libatspi is found (tests/unit/CMakeLists.txt); it runs where an
/// accessibility bus is up and enabled, which the environment says with AFFRESCOUI_ATSPI=1 (CI starts one under
/// Xvfb), and otherwise reports that it was skipped.
///
/// Built into affrescoui_window_tests: it needs a window, hidden.

#if defined(__linux__) && defined(AFFRESCOUI_ACCESSIBILITY) && defined(AFFRESCOUI_ATSPI_TEST)

#include "window/AccessibilityForm.hpp"

#include <affresco/render/Application.hpp>

#include <GLFW/glfw3.h>

#include <atspi/atspi.h>
#include <unistd.h>

#include <doctest/doctest.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

/// @brief Owns a GObject reference from AT-SPI.
template <typename Object> class Owned {
public:
	/// @brief Takes a reference.
	/// @param object It, or null.
	explicit Owned(Object* object = nullptr) noexcept : object_(object) {}
	/// @brief Drops it.
	~Owned() {
		if(object_) g_object_unref(object_);
	}
	Owned(const Owned&) = delete;
	Owned& operator=(const Owned&) = delete;
	/// @brief Takes another's reference.
	/// @param other The other.
	Owned(Owned&& other) noexcept : object_(std::exchange(other.object_, nullptr)) {}
	/// @brief Takes another's reference, dropping this one's.
	/// @param other The other.
	/// @return This.
	Owned& operator=(Owned&& other) noexcept {
		std::swap(object_, other.object_);
		return *this;
	}
	/// @brief The object, or null.
	[[nodiscard]] Object* get() const noexcept { return object_; }
	/// @brief Whether there is one.
	explicit operator bool() const noexcept { return object_ != nullptr; }

private:
	Object* object_; ///< The object.
};

/// @brief An accessible's name.
/// @param accessible It.
/// @return Its name; empty if none.
std::string nameOf(AtspiAccessible* accessible) {
	gchar* name = atspi_accessible_get_name(accessible, nullptr);
	std::string result = name ? name : "";
	g_free(name);
	return result;
}

/// @brief The descendant of an accessible with a name, depth first.
/// @param parent The accessible.
/// @param name The name.
/// @param depth How deep the search may still go.
/// @return It, or null.
Owned<AtspiAccessible> find(AtspiAccessible* parent, std::string_view name, int depth) {
	const gint count = atspi_accessible_get_child_count(parent, nullptr);
	for(gint index = 0; index < count; ++index) {
		Owned<AtspiAccessible> child(atspi_accessible_get_child_at_index(parent, index, nullptr));
		if(!child) continue;
		if(nameOf(child.get()) == name) return child;
		if(depth > 0)
			if(Owned<AtspiAccessible> found = find(child.get(), name, depth - 1)) return found;
	}
	return Owned<AtspiAccessible>();
}

/// @brief This process's application on the desktop, once AccessKit registered it.
/// @return It, or null.
Owned<AtspiAccessible> thisApplication() {
	Owned<AtspiAccessible> desktop(atspi_get_desktop(0));
	const gint count = desktop ? atspi_accessible_get_child_count(desktop.get(), nullptr) : 0;
	for(gint index = 0; index < count; ++index) {
		Owned<AtspiAccessible> application(atspi_accessible_get_child_at_index(desktop.get(), index, nullptr));
		if(application && atspi_accessible_get_process_id(application.get(), nullptr) == static_cast<guint>(getpid()))
			return application;
	}
	return Owned<AtspiAccessible>();
}

} // namespace

TEST_CASE("accessibility: Linux reads a window's tree through AT-SPI") {
	const char* enabled = std::getenv("AFFRESCOUI_ATSPI");
	if(!enabled || std::string_view(enabled) != "1") {
		MESSAGE("skipped: no accessibility bus (AFFRESCOUI_ATSPI=1 says there is one)");
		return;
	}
	REQUIRE(atspi_init() >= 0);
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("Accessible", 400, 300);
	affresco::test::Form form;
	affresco::test::build(window, form);

	// AccessKit registers on its own thread, and asks the main thread for the tree: run frames until it shows
	Owned<AtspiAccessible> button;
	for(int attempt = 0; attempt < 100 && !button; ++attempt) {
		glfwPollEvents();
		window.render();
		if(Owned<AtspiAccessible> application = thisApplication()) button = find(application.get(), "Save", 4);
		if(!button) std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	REQUIRE(button);
	Owned<AtspiAccessible> application = thisApplication();
	Owned<AtspiAccessible> toggle = find(application.get(), "Dark mode", 4);
	Owned<AtspiAccessible> slider = find(application.get(), "Volume", 4);
	REQUIRE((toggle && slider));
	CHECK(atspi_accessible_get_role(button.get(), nullptr) == ATSPI_ROLE_PUSH_BUTTON);
	CHECK(atspi_accessible_get_role(toggle.get(), nullptr) == ATSPI_ROLE_CHECK_BOX);
	CHECK(atspi_accessible_get_role(slider.get(), nullptr) == ATSPI_ROLE_SLIDER);
	Owned<AtspiValue> value(atspi_accessible_get_value_iface(slider.get()));
	REQUIRE(value);
	CHECK(atspi_value_get_current_value(value.get(), nullptr) == doctest::Approx(4.0));

	// A press is queued and runs on the main thread in the next frame
	Owned<AtspiAction> action(atspi_accessible_get_action_iface(button.get()));
	REQUIRE(action);
	CHECK(atspi_action_do_action(action.get(), 0, nullptr));
	for(int attempt = 0; attempt < 20 && form.presses == 0; ++attempt) {
		glfwPollEvents();
		window.render();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	CHECK(form.presses == 1);
}

#endif
