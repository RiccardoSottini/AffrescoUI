/// @file
/// @brief The form the window/accessibility_*.cpp tests read through each platform's API: a label, a button, a
/// checkbox, a slider and a text field, each with a role and a name.

#pragma once

#include <affresco/render/Controls.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Panel.hpp>
#include <affresco/render/TextArea.hpp>
#include <affresco/render/Window.hpp>

namespace affresco::test {

/// @brief A window's panels, with a role each.
struct Form {
	affresco::Label* title = nullptr;    ///< A label.
	affresco::Panel* button = nullptr;   ///< A button, counting presses.
	affresco::Toggle* toggle = nullptr;  ///< A checkbox.
	affresco::Slider* slider = nullptr;  ///< A slider from 0 to 10.
	affresco::TextArea* field = nullptr; ///< A text field.
	int presses = 0;                     ///< How often the button was pressed.
};

/// @brief Places a panel.
/// @param panel The panel.
/// @param x Its left edge, in window units.
/// @param y Its top edge.
/// @param width Its width.
/// @param height Its height.
inline void place(affresco::Panel& panel, float x, float y, float width, float height) {
	panel.setPosition(affresco::px(x), affresco::px(y));
	panel.setSize(affresco::px(width), affresco::px(height));
}

/// @brief Fills a window with a form.
/// @param window The window.
/// @param form Where to keep the panels.
inline void build(affresco::Window& window, Form& form) {
	affresco::Panel& root = window.root();
	form.title = &root.add<affresco::Label>("Settings");
	place(*form.title, 10, 10, 200, 24);
	form.button = &root.add<affresco::Panel>();
	form.button->setAccessibleRole("button");
	form.button->setAccessibleLabel("Save");
	form.button->setAccessibleDescription("Saves the file");
	form.button->onMouseButton.connect([&form](const affresco::MouseButtonEvent& event) {
		if(event.action == affresco::Action::Press) ++form.presses;
	});
	place(*form.button, 10, 40, 80, 30);
	form.toggle = &root.add<affresco::Toggle>(affresco::Toggle::Look::Checkbox);
	form.toggle->setAccessibleLabel("Dark mode");
	place(*form.toggle, 10, 80, 40, 20);
	form.slider = &root.add<affresco::Slider>();
	form.slider->setAccessibleLabel("Volume");
	form.slider->setRange(0.0, 10.0, 1.0);
	form.slider->setValue(4.0);
	place(*form.slider, 10, 110, 200, 20);
	form.field = &root.add<affresco::TextArea>();
	form.field->setAccessibleLabel("Name");
	form.field->setText("Ada");
	place(*form.field, 10, 140, 200, 24);
}

} // namespace affresco::test
