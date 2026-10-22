/// @file
/// @brief No bridge to the platform's accessibility API: the build was configured with AFFRESCOUI_ACCESSIBILITY=OFF.

#include "Accessibility.hpp"

namespace affresco::platform {

std::unique_ptr<AccessibilityAdapter> createAccessibilityAdapter(Window& window) {
	static_cast<void>(window);
	return nullptr;
}

} // namespace affresco::platform
