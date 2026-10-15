#pragma once

/// @file
/// @brief Stack: a container that places its children one below the other.

#include "sgl/render/Panel.hpp"

namespace sgl {

/// @brief Places its visible children one below the other, `gap` apart, in order.
///
/// Children keep their own width and horizontal anchors; the stack manages their vertical anchors. With Align::End
/// the block sits at the bottom, like a chat history: older children scroll off the top and get clipped.
class Stack : public Panel {
public:
	/// @brief An empty stack with a transparent background.
	Stack();

	/// @brief Sets the space between children.
	/// @param gap In pixels.
	void setGap(float gap);
	/// @brief The space between children, in pixels.
	[[nodiscard]] float gap() const noexcept { return gap_; }

	/// @brief Sets where the block of children sits.
	/// @param alignment Align::Start (top) or Align::End (bottom).
	void setAlignment(Align alignment);
	/// @brief Where the block of children sits.
	[[nodiscard]] Align alignment() const noexcept { return alignment_; }

protected:
	/// @brief Anchors each visible child after the previous one. Percentages use the stack's previous size.
	void prepareLayout() override;

private:
	float gap_ = 0.0f;               ///< See gap().
	Align alignment_ = Align::Start; ///< See alignment().
};

} // namespace sgl
