/// @file
/// @brief affresco::Screen's audio in a build without the audio module (AFFRESCOUI_AUDIO=OFF, or the browser): there
/// is no engine, and the screen's "audio" steps say the build has no audio.

#include "ScreenImpl.hpp"

namespace affresco {

audio::Engine* Screen::audio() { return nullptr; }

void Screen::setAudio(std::shared_ptr<audio::Engine> engine) { static_cast<void>(engine); }

void Screen::setAudioSource(const std::string& name, std::shared_ptr<audio::Source> source) {
	static_cast<void>(name), static_cast<void>(source);
}

void Screen::allowAudioDirectory(const std::filesystem::path& directory) { static_cast<void>(directory); }

} // namespace affresco
