/// @file
/// @brief gl::StencilClips: pushing and popping clips by counting up and down in the stencil buffer.

#include "gl/Stencil.hpp"

namespace affresco::gl {

void StencilClips::start() noexcept {
	level_ = 0;
	glStencilMask(0xFF);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
}

// Only pixels inside every clip so far hold the level, so a pushed quad counts up just where both overlap
// NOLINTNEXTLINE(readability-make-member-function-const): it sets the stencil state these clips own
void StencilClips::beginStep(ClipStep step) noexcept {
	glEnable(GL_STENCIL_TEST);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glStencilFunc(GL_EQUAL, level_, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, step == ClipStep::push ? GL_INCR : GL_DECR);
}

void StencilClips::endStep(ClipStep step) noexcept {
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	level_ += step == ClipStep::push ? 1 : -1;
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, level_, 0xFF);
	if(level_ <= 0) glDisable(GL_STENCIL_TEST);
}

void StencilClips::finish() noexcept {
	level_ = 0;
	glDisable(GL_STENCIL_TEST);
}

} // namespace affresco::gl
