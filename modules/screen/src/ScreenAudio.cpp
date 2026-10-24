/// @file
/// @brief affresco::Screen's audio: the engine its "audio" section plays on, made at the first call of audio().

#include "ScreenImpl.hpp"

#include "affresco/audio/Engine.hpp"

#include <cstdlib>
#include <memory>
#include <string>

namespace affresco {

namespace {

/// @brief Where the engine plays: AFFRESCOUI_AUDIO_OUTPUT=null plays to no hardware (a host under test, CI), anything
/// else to the system's output.
/// @return The output.
audio::Output outputFromEnvironment() {
	std::string value;
#ifdef _WIN32
	char* text = nullptr;
	std::size_t length = 0;
	if(_dupenv_s(&text, &length, "AFFRESCOUI_AUDIO_OUTPUT") == 0 && text) {
		value = text;
		std::free(text); // NOLINT(cppcoreguidelines-no-malloc): _dupenv_s allocates with malloc
	}
#else
	if(const char* text = std::getenv("AFFRESCOUI_AUDIO_OUTPUT")) value = text; // NOLINT(concurrency-mt-unsafe)
#endif
	return value == "null" ? audio::Output::Null : audio::Output::System;
}

} // namespace

audio::Engine* Screen::audio() {
	Impl& impl = *impl_;
	if(!impl.audio) {
		audio::EngineOptions options;
		options.output = outputFromEnvironment();
		options.executor = &impl.relay; // events reach the Screen's executor, set before or after this call
		impl.audio = std::make_shared<audio::Engine>(options);
		impl.runtime.setAudio(impl.audio);
	}
	return impl.audio.get();
}

void Screen::setAudio(std::shared_ptr<audio::Engine> engine) {
	impl_->runtime.setAudio(engine);
	impl_->audio = std::move(engine);
}

void Screen::setAudioSource(const std::string& name, std::shared_ptr<audio::Source> source) {
	impl_->runtime.setAudioSource(name, std::move(source));
}

void Screen::allowAudioDirectory(const std::filesystem::path& directory) {
	impl_->runtime.allowAudioDirectory(directory);
}

} // namespace affresco
