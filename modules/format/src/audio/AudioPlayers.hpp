#pragma once

/// @file
/// @brief format::AudioPlayers: the sounds a screen's "audio" section declares, played by "audio" steps on the
/// host's engine (Screen::audio()), with their state written to their "into" paths and their events run as the
/// actions "<name>:started", "<name>:ended", "<name>:underrun" and "<name>:error".

#include "affresco/core/Json.hpp"
#include "affresco/core/Network.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::audio {
class Engine;
class Source;
} // namespace affresco::audio

namespace affresco::format {

struct Engine;

/// @brief The players of a screen's sounds.
///
/// A player is made at its first step, on the engine the host gave (setEngine()): without one, every step reports
/// "this host plays no audio" in the player's state and the action goes on. A source is only ever what the section
/// declares (a capability: revisions can't change it): a file inside the screen's folder or one the host allows, a URL
/// the host's network allows, a source the host named, or a tone. Main thread only; the engine's events reach it
/// through the engine's Executor. Built without the audio module (AFFRESCOUI_AUDIO=OFF), AudioPlayersNone.cpp
/// stands in: every step reports that the build has no audio.
class AudioPlayers {
public:
	/// The most sounds a section declares.
	static constexpr std::size_t maxSounds = 32;
	/// The most bytes a URL's stream buffers, and its default.
	static constexpr std::size_t maxStreamBytes = std::size_t{64} << 20;
	/// The default of "maxBytes".
	static constexpr std::size_t defaultStreamBytes = std::size_t{8} << 20;

	/// @brief The players of an engine. The screen's folder (DocumentContext::baseDirectory()) is allowed: its
	/// files may play.
	/// @param engine The runtime; outlives this object.
	explicit AudioPlayers(Engine& engine);
	AudioPlayers(const AudioPlayers&) = delete;
	AudioPlayers& operator=(const AudioPlayers&) = delete;
	AudioPlayers(AudioPlayers&&) = delete;
	AudioPlayers& operator=(AudioPlayers&&) = delete;
	/// @brief Stops every sound.
	~AudioPlayers();

	/// @brief Sets the engine the sounds play on; the players playing on another stop.
	/// @param engine The engine, or null for none.
	void setEngine(std::shared_ptr<audio::Engine> engine);
	/// @brief The engine the sounds play on, or null.
	[[nodiscard]] std::shared_ptr<audio::Engine> engine() const;
	/// @brief Names a source the host makes (a live stream, decoded bytes) for sounds that say `"source": name`.
	/// @param name The name.
	/// @param source The source, or null to remove it.
	void setSource(const std::string& name, std::shared_ptr<audio::Source> source);
	/// @brief Lets sounds' files come from a folder too.
	/// @param directory The folder.
	void allowDirectory(const std::filesystem::path& directory);
	/// @brief Sets the network URLs stream through, whose allowlist they pass first.
	/// @param network The network, or null (no URL plays).
	void setNetwork(network::Network* network) noexcept { network_ = network; }
	/// @brief Stops the players whose declaration changed or went (a restore, an include).
	void documentChanged();
	/// @brief Stops every sound and forgets the players.
	void stopAll();

	/// @brief Runs one operation of an "audio" step.
	/// @param name The sound (empty for "master").
	/// @param operation play, pause, toggle, stop, seek, volume, pan, rate, loop or master.
	/// @param step The step's evaluated values: "to", "from", "ms".
	/// @param where The step's place, for document errors.
	/// @return Why it couldn't play (no engine, a file that doesn't open), which the player's state shows too; or
	/// nothing.
	/// @throws DocumentError If the step is wrong (an operation or a value it can't take).
	[[nodiscard]] std::optional<std::string> run(const std::string& name, const std::string& operation, const json& step,
	                                             const std::string& where);

private:
	struct Impl;                 ///< The players (AudioPlayers.cpp), or nothing (AudioPlayersNone.cpp).
	std::unique_ptr<Impl> impl_;          ///< See Impl.
	network::Network* network_ = nullptr; ///< See setNetwork().
};

/// @brief The operations of the "audio" step, for the checker and the reference.
/// @return Their names.
[[nodiscard]] const std::vector<std::string>& audioOperations();

} // namespace affresco::format
