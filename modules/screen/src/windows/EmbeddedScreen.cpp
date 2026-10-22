/// @file
/// @brief Screen::embed(): a screen built into the host application's OpenGL context (affresco::Embedding).

#include "affresco/Screen.hpp"

namespace affresco {

void Screen::embed(Embedding& embedding) {
	setExecutor(&embedding); // slow calls and services post to the embedding, as createWindow() does to the Application
	build(embedding.window());
	startChannels(embedding);
}

} // namespace affresco
