/// @file
/// @brief format::AudioPlayers in a build without the audio module (AFFRESCOUI_AUDIO=OFF, or the browser): the
/// section is still checked, and every "audio" step says that the build has no audio, in the sound's state too.

#include "AudioPlayers.hpp"

#include "Engine.hpp"
#include "NodeKeys.hpp"

namespace affresco::format {

/// @brief What stands in for the players: the runtime, to write the sounds' state.
struct AudioPlayers::Impl {
	Engine& runtime; ///< The runtime.
};

AudioPlayers::AudioPlayers(Engine& engine) : impl_(std::make_unique<Impl>(Impl{engine})) {}

AudioPlayers::~AudioPlayers() = default;

void AudioPlayers::setEngine(std::shared_ptr<audio::Engine> engine) { static_cast<void>(engine); }

std::shared_ptr<audio::Engine> AudioPlayers::engine() const { return nullptr; }

void AudioPlayers::setSource(const std::string& name, std::shared_ptr<audio::Source> source) {
	static_cast<void>(name), static_cast<void>(source);
}

void AudioPlayers::allowDirectory(const std::filesystem::path& directory) { static_cast<void>(directory); }

void AudioPlayers::documentChanged() {}

void AudioPlayers::stopAll() {}

std::optional<std::string> AudioPlayers::run(const std::string& name, const std::string& operation, const json& step,
                                             const std::string& where) {
	static_cast<void>(operation), static_cast<void>(step), static_cast<void>(where);
	static const std::string problem = "this build of AffrescoUI has no audio";
	const json& document = impl_->runtime.versions.document();
	const auto sounds = document.find("audio");
	if(sounds != document.end() && sounds->contains(name) && (*sounds)[name].contains("into")) {
		const std::string at = "audio." + name + ".into";
		const json::json_pointer pointer = statePointer((*sounds)[name]["into"].get<std::string>(),
		                                                impl_->runtime.document.context(Scope{}, emptyObject()), at);
		statePlace(impl_->runtime.versions.state(), pointer, at) = {{"status", "error"}, {"playing", false},
		                                                             {"error", problem}};
		impl_->runtime.document.markStateChanged(pointer);
		impl_->runtime.reconciler.requestStateUpdate();
	}
	return problem;
}

std::optional<double> probeSound(const std::string& bytes, std::string_view type) {
	static_cast<void>(bytes), static_cast<void>(type);
	return std::nullopt; // nothing decodes sound in this build: the first bytes were checked already
}

} // namespace affresco::format
