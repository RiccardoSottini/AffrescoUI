#include "sgl/Stack.hpp"

#include <vector>

namespace sgl {

Stack::Stack() { setColor(colors::transparent); }

void Stack::setGap(float gap) {
	gap_ = gap;
	invalidate();
}

void Stack::setAlignment(Align alignment) {
	alignment_ = alignment;
	invalidate();
}

void Stack::prepareLayout() {
	// Percentages refer to the stack's size from the previous pass (its own size is resolved after this hook)
	const float reference = resolvedSize().y;

	std::vector<Panel*> visible;
	for(const auto& child : children())
		if(child->isVisible()) visible.push_back(child.get());

	float offset = 0.0f;
	if(alignment_ == Align::End) {
		for(auto it = visible.rbegin(); it != visible.rend(); ++it) { // from the bottom up
			(*it)->clearAnchor(Edge::Top);
			(*it)->setAnchor(Edge::Bottom, px(offset));
			offset += (*it)->measuredHeight(reference) + gap_;
		}
	} else {
		for(Panel* child : visible) {
			child->clearAnchor(Edge::Bottom);
			child->setAnchor(Edge::Top, px(offset));
			offset += child->measuredHeight(reference) + gap_;
		}
	}
}

} // namespace sgl
