/// @file
/// @brief The accessibility tree a window exposes on macOS, read back through NSAccessibility as VoiceOver reads it:
/// roles, names, values, states, bounds in screen points, a press, a slider's increment, and an update after a
/// change. Compiled as Objective-C++ on macOS (tests/unit/CMakeLists.txt); elsewhere it holds no test.
///
/// Built into affrescoui_window_tests: it needs a window, hidden.

#if defined(__APPLE__) && defined(AFFRESCOUI_ACCESSIBILITY)

#include "window/AccessibilityForm.hpp"

#include <affresco/render/Accessibility.hpp>
#include <affresco/render/Application.hpp>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

#include <doctest/doctest.h>

#include <string>

namespace {

using affresco::test::Form;

/// @brief UTF-8 from an NSString.
/// @param string The string, or nil.
/// @return Its text, or "(nil)".
std::string text(NSString* string) { return string ? std::string([string UTF8String]) : std::string("(nil)"); }

/// @brief The child of an element with a label (or a value, for static text).
/// @param parent The element.
/// @param name The label.
/// @return The child, or nil.
id childNamed(id parent, NSString* name) {
	for(id child in [parent accessibilityChildren])
		if([[child accessibilityLabel] isEqualToString:name] || [[child accessibilityTitle] isEqualToString:name] ||
		   ([[child accessibilityValue] isKindOfClass:[NSString class]] &&
		    [[child accessibilityValue] isEqualToString:name]))
			return child;
	return nil;
}

/// @brief Prints an element and its children, for a failing test.
/// @param element The element.
/// @param depth How deep it is.
void dump(id element, int depth) {
	MESSAGE(std::string(static_cast<std::size_t>(depth) * 2, ' ')
	        << text([element accessibilityRole]) << " label=" << text([element accessibilityLabel]) << " title="
	        << text([element accessibilityTitle]) << " value=" << text([[element accessibilityValue] description])
	        << " help=" << text([element accessibilityHelp])
	        << " frame=" << text(NSStringFromRect([element accessibilityFrame])));
	for(id child in [element accessibilityChildren]) dump(child, depth + 1);
}

/// @brief The element of a window's tree in its view: AccessKit puts the window's own element under the view.
/// @param view The view.
/// @return The window's element.
id windowElement(NSView* view) {
	NSArray* children = [view accessibilityChildren]; // activates AccessKit the first time: the tree is read now
	return [children count] == 1u ? children[0] : nil;
}

/// @brief How far an element's top edge is below the window's, in points (screen coordinates grow upwards).
/// @param window The window's element.
/// @param element The element.
/// @return The distance.
double topOffset(id window, id element) {
	const NSRect outer = [window accessibilityFrame];
	const NSRect inner = [element accessibilityFrame];
	return NSMaxY(outer) - NSMaxY(inner);
}

} // namespace

TEST_CASE("accessibility: macOS reads a window's tree through NSAccessibility") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("Accessible", 400, 300);
	Form form;
	affresco::test::build(window, form);
	window.render();

	NSView* view = glfwGetCocoaView(window.nativeHandle());
	id root = windowElement(view);
	if(!root || [[root accessibilityChildren] count] != 5u) dump(view, 0);
	REQUIRE([[root accessibilityChildren] count] == 5u);
	CHECK(NSEqualSizes([root accessibilityFrame].size, NSMakeSize(400, 300)));

	id title = childNamed(root, @"Settings");
	id button = childNamed(root, @"Save");
	id toggle = childNamed(root, @"Dark mode");
	id slider = childNamed(root, @"Volume");
	id field = childNamed(root, @"Name");
	REQUIRE((title && button && toggle && slider && field));
	CHECK(text([title accessibilityRole]) == "AXStaticText");
	CHECK(text([button accessibilityRole]) == "AXButton");
	CHECK(text([button accessibilityHelp]) == "Saves the file");
	CHECK(text([toggle accessibilityRole]) == "AXCheckBox");
	CHECK([[toggle accessibilityValue] intValue] == 0);
	CHECK(text([slider accessibilityRole]) == "AXSlider");
	CHECK([[slider accessibilityValue] doubleValue] == doctest::Approx(4.0));
	CHECK(text([field accessibilityRole]) == "AXTextField");
	CHECK(text([field accessibilityValue]) == "Ada");
	CHECK([field accessibilitySelectedTextRange].location <= 3u); // the caret, in the text

	// Bounds in screen points, whatever the display's scale: the button is 80 by 30, 40 below the window's top
	CHECK(NSEqualSizes([button accessibilityFrame].size, NSMakeSize(80, 30)));
	CHECK(topOffset(root, button) == doctest::Approx(40.0));
	CHECK([button accessibilityFrame].origin.x - [root accessibilityFrame].origin.x == doctest::Approx(10.0));

	// Actions are queued and run on the main thread by the next pass of the loop
	CHECK([button accessibilityPerformPress]);
	CHECK([toggle accessibilityPerformPress]);
	CHECK([slider accessibilityPerformIncrement]);
	window.render();
	CHECK(form.presses == 1);
	CHECK(form.toggle->isChecked());
	CHECK(form.slider->value() == doctest::Approx(5.0));

	// What changed reaches the platform after the frame: the new states, and a label's new text
	form.title->setText("Saved");
	window.render();
	CHECK([[childNamed(root, @"Dark mode") accessibilityValue] intValue] == 1);
	CHECK([[childNamed(root, @"Volume") accessibilityValue] doubleValue] == doctest::Approx(5.0));
	const bool renamed = childNamed(root, @"Saved") != nil && childNamed(root, @"Settings") == nil;
	CHECK(renamed);

	// States: a disabled checkbox
	affresco::AccessibleState disabled;
	disabled.disabled = true;
	form.toggle->setAccessibleState(disabled);
	window.render();
	CHECK_FALSE([childNamed(root, @"Dark mode") isAccessibilityEnabled]);

	// A hidden panel leaves the tree
	form.button->setVisible(false);
	window.render();
	CHECK([[root accessibilityChildren] count] == 4u);
}

#endif
