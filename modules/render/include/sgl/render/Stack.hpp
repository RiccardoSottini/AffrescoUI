#pragma once

#include "sgl/render/Panel.hpp"

namespace sgl {

// Places its children one below the other, `gap` apart, in order. Children keep their own width and horizontal
// anchors; their vertical position is managed by the stack. With Align::End the block of children sits at the
// bottom (like a chat history: the last child is lowest, older ones scroll off the top and get clipped).
class Stack : public Panel {
public:
	Stack();

	void setGap(float gap);
	[[nodiscard]] float gap() const noexcept { return gap_; }

	void setAlignment(Align alignment); // Start (top) or End (bottom)
	[[nodiscard]] Align alignment() const noexcept { return alignment_; }

protected:
	void prepareLayout() override;

private:
	float gap_ = 0.0f;
	Align alignment_ = Align::Start;
};

} // namespace sgl
