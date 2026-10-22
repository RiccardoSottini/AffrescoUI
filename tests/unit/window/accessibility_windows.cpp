/// @file
/// @brief The accessibility tree a window exposes on Windows, read back through UI Automation as Narrator reads it:
/// control types, names, values, a toggle's state, bounds in physical pixels, and a press. Compiled on Windows only;
/// elsewhere it holds no test.
///
/// Built into affrescoui_window_tests: it needs a window, hidden.

#if defined(_WIN32) && defined(AFFRESCOUI_ACCESSIBILITY)

#include "window/AccessibilityForm.hpp"

#include <affresco/render/Application.hpp>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <windows.h> // first: the others need its types

#include <UIAutomation.h>
#include <oleauto.h>
#include <wrl/client.h>

#include <doctest/doctest.h>

#include <string>

namespace {

using Microsoft::WRL::ComPtr;

/// @brief COM, for the test's thread, as long as it lives.
class ComApartment {
public:
	/// @brief Enters a single-threaded apartment.
	ComApartment() noexcept : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
	/// @brief Leaves it.
	~ComApartment() {
		if(SUCCEEDED(result_)) CoUninitialize();
	}
	ComApartment(const ComApartment&) = delete;
	ComApartment& operator=(const ComApartment&) = delete;

private:
	HRESULT result_; ///< What entering gave.
};

/// @brief An element's name, as UTF-16.
/// @param element The element.
/// @return Its name; empty if none.
std::wstring nameOf(IUIAutomationElement* element) {
	BSTR name = nullptr;
	if(FAILED(element->get_CurrentName(&name)) || !name) return {};
	std::wstring result(name, SysStringLen(name));
	SysFreeString(name);
	return result;
}

/// @brief The descendant of an element with a name.
/// @param automation UI Automation.
/// @param root The element.
/// @param name The name.
/// @return It, or null.
ComPtr<IUIAutomationElement> find(IUIAutomation* automation, IUIAutomationElement* root, const wchar_t* name) {
	ComPtr<IUIAutomationCondition> any;
	ComPtr<IUIAutomationElementArray> all;
	if(FAILED(automation->CreateTrueCondition(&any)) || FAILED(root->FindAll(TreeScope_Descendants, any.Get(), &all)))
		return nullptr;
	int count = 0;
	all->get_Length(&count);
	for(int index = 0; index < count; ++index) {
		ComPtr<IUIAutomationElement> element;
		if(SUCCEEDED(all->GetElement(index, &element)) && nameOf(element.Get()) == name) return element;
	}
	return nullptr;
}

/// @brief An element's control type.
/// @param element The element.
/// @return Its UIA_*ControlTypeId.
CONTROLTYPEID controlTypeOf(IUIAutomationElement* element) {
	CONTROLTYPEID type = 0;
	element->get_CurrentControlType(&type);
	return type;
}

} // namespace

TEST_CASE("accessibility: Windows reads a window's tree through UI Automation") {
	const ComApartment apartment;
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("Accessible", 400, 300);
	affresco::test::Form form;
	affresco::test::build(window, form);
	window.render();

	ComPtr<IUIAutomation> automation;
	REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation))));
	ComPtr<IUIAutomationElement> root;
	REQUIRE(SUCCEEDED(automation->ElementFromHandle(glfwGetWin32Window(window.nativeHandle()), &root)));

	ComPtr<IUIAutomationElement> button = find(automation.Get(), root.Get(), L"Save");
	ComPtr<IUIAutomationElement> toggle = find(automation.Get(), root.Get(), L"Dark mode");
	ComPtr<IUIAutomationElement> slider = find(automation.Get(), root.Get(), L"Volume");
	ComPtr<IUIAutomationElement> field = find(automation.Get(), root.Get(), L"Name");
	REQUIRE((button && toggle && slider && field));
	CHECK(controlTypeOf(button.Get()) == UIA_ButtonControlTypeId);
	CHECK(controlTypeOf(toggle.Get()) == UIA_CheckBoxControlTypeId);
	CHECK(controlTypeOf(slider.Get()) == UIA_SliderControlTypeId);
	CHECK(controlTypeOf(field.Get()) == UIA_EditControlTypeId);

	ComPtr<IUIAutomationRangeValuePattern> range;
	REQUIRE(SUCCEEDED(slider->GetCurrentPatternAs(UIA_RangeValuePatternId, IID_PPV_ARGS(&range))));
	double value = 0.0;
	range->get_CurrentValue(&value);
	CHECK(value == doctest::Approx(4.0));

	// Bounds in physical pixels: the button is 80 by 30 window units
	RECT bounds{};
	button->get_CurrentBoundingRectangle(&bounds);
	CHECK(bounds.right - bounds.left == static_cast<LONG>(80.0F * window.contentScale() + 0.5F));

	// A press is queued and runs on the main thread in the next frame
	ComPtr<IUIAutomationInvokePattern> invoke;
	REQUIRE(SUCCEEDED(button->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke))));
	CHECK(SUCCEEDED(invoke->Invoke()));
	ComPtr<IUIAutomationTogglePattern> toggling;
	REQUIRE(SUCCEEDED(toggle->GetCurrentPatternAs(UIA_TogglePatternId, IID_PPV_ARGS(&toggling))));
	CHECK(SUCCEEDED(toggling->Toggle()));
	window.render();
	CHECK(form.presses == 1);
	CHECK(form.toggle->isChecked());
}

#endif
