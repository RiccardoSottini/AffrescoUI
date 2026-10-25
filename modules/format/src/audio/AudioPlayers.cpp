/// @file
/// @brief format::AudioPlayers on the audio module: each declared sound's voice, its source (a file in an allowed
/// folder, a URL through the host's network, a source the host named, a tone), its state and its actions.

#include "AudioPlayers.hpp"

#include "Engine.hpp"
#include "NodeKeys.hpp"

#include <affresco/audio/Engine.hpp>
#include <affresco/audio/Sound.hpp>
#include <affresco/audio/Spectrum.hpp>

#include <fstream>

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace affresco::format {

static_assert(AudioPlayers::maxBands == audio::maxSpectrumBands); // "bands" is checked against it

namespace {

/// What a URL's request reads at a time, at most (a chunk of its stream).
constexpr std::size_t streamChunkBytes = std::size_t{64} << 10;

/// @brief Whether a path is a folder or inside it, both resolved.
/// @param path The path.
/// @param folder The folder.
/// @return Whether it is.
bool isInside(const std::filesystem::path& path, const std::filesystem::path& folder) {
	const std::filesystem::path relative = path.lexically_relative(folder);
	return !relative.empty() && *relative.begin() != "..";
}

/// @brief A path resolved: absolute, links followed, so that a link out of an allowed folder is seen.
/// @param path The path.
/// @return It, resolved.
std::filesystem::path resolved(const std::filesystem::path& path) {
	std::error_code error;
	std::filesystem::path out = std::filesystem::weakly_canonical(path, error);
	return error ? std::filesystem::absolute(path, error).lexically_normal() : out;
}

/// @brief A number of seconds as the state shows it: to the millisecond.
/// @param seconds The seconds.
/// @return Them, rounded.
double milliseconds(double seconds) { return std::round(seconds * 1000) / 1000; }

/// @brief A value read as a number in a range.
/// @param value The value.
/// @param at Its place.
/// @param low The least.
/// @param high The most.
/// @return It.
/// @throws DocumentError If it isn't a number in the range.
double numberIn(const json& value, const std::string& at, double low, double high) {
	if(!value.is_number() || !(value.get<double>() >= low && value.get<double>() <= high))
		fail(at, "expected a number from " + json(low).dump() + " to " + json(high).dump());
	return value.get<double>();
}

} // namespace

/// @brief The players, the sources the host named, and the folders files may come from.
struct AudioPlayers::Impl {
	/// @brief One declared sound.
	struct Player {
		json spec;                                    ///< Its declaration.
		std::shared_ptr<audio::Voice> voice;          ///< Once a step needed it.
		std::shared_ptr<audio::EncodedStream> stream; ///< A URL's bytes.
		std::unique_ptr<network::Transfer> transfer;  ///< A URL's request, while it runs.
		std::string status = "stopped";               ///< stopped, playing, paused or error.
		std::optional<std::string> error;             ///< Why it couldn't play.
		double position = 0;                          ///< Seconds from the start.
		std::optional<double> duration;               ///< Seconds in all, if known.
		float volume = 1;                             ///< Its gain.
		float pan = 0;                                ///< Its balance.
		float rate = 1;                               ///< Its playback rate.
		bool loop = false;                            ///< Whether it starts again at its end.
		float level = 0;                              ///< Its peak over the last block.
		std::vector<float> spectrum;                  ///< Its "bands", if it has them (see audio::spectrum).
		std::size_t underruns = 0;                    ///< How often its stream ran dry.
		std::uint64_t generation = 0;                 ///< Its voice's: events of an older one are ignored.
	};

	/// @brief The players of an engine.
	/// @param self The public object.
	/// @param engine The runtime.
	Impl(AudioPlayers& self, Engine& engine) : owner(self), runtime(engine) {}

	AudioPlayers& owner;                                                       ///< For the network.
	Engine& runtime;                                                           ///< The runtime.
	std::shared_ptr<audio::Engine> audio;                                      ///< See setEngine().
	std::map<std::string, std::shared_ptr<audio::Source>, std::less<>> sources; ///< See setSource().
	std::vector<std::filesystem::path> folders;                                ///< See allowDirectory().
	std::map<std::string, Player, std::less<>> players;                       ///< The sounds used so far.
	std::vector<std::unique_ptr<network::Transfer>> retired;                   ///< See retire().
	std::uint64_t generation = 0;                                              ///< The last voice made.
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);                ///< Expires with this.

	/// @brief The declaration of a sound in the document, or null.
	/// @param name The sound.
	/// @return It.
	[[nodiscard]] const json* declaration(std::string_view name) const {
		const json& document = runtime.versions.document();
		const auto sounds = document.find("audio");
		if(sounds == document.end() || !sounds->is_object()) return nullptr;
		const auto found = sounds->find(name);
		return found == sounds->end() ? nullptr : &*found;
	}

	/// @brief The player of a declared sound, made with its declaration's settings at first.
	/// @param name The sound.
	/// @param where The step's place.
	/// @return It.
	Player& player(const std::string& name, const std::string& where) {
		if(const auto found = players.find(name); found != players.end()) return found->second;
		const json* spec = declaration(name);
		if(!spec) fail(where, "no sound \"" + name + "\" in the \"audio\" section");
		Player made;
		made.spec = *spec;
		made.volume = spec->value("volume", 1.0F);
		made.pan = spec->value("pan", 0.0F);
		made.rate = spec->value("rate", 1.0F);
		made.loop = spec->value("loop", false);
		return players.emplace(name, std::move(made)).first->second;
	}

	/// @brief Keeps a transfer until the current task returns (never destroyed inside its own handler).
	/// @param transfer It.
	void retire(std::unique_ptr<network::Transfer> transfer) {
		if(!transfer) return;
		retired.push_back(std::move(transfer));
		if(retired.size() > 1 || !runtime.window) return; // a clear is scheduled, or nothing runs handlers
		runtime.scheduler.defer([this, kept = std::weak_ptr<bool>(alive)] {
			if(kept.lock()) retired.clear();
		});
	}

	/// @brief Forgets a player's voice and stream: the next play makes them again.
	/// @param player The player.
	void drop(Player& player) {
		if(player.voice) player.voice->stop();
		player.voice.reset();
		if(player.stream) player.stream->finish();
		player.stream.reset();
		retire(std::move(player.transfer));
	}

	/// @brief A player's spectrum for its state: its "bands", all 0 unless it plays.
	/// @param player The player, with "bands".
	/// @return The bands, each 0 to 1 to three decimals.
	static json spectrumOf(const Player& player) {
		json bands = json::array();
		const bool playing = player.status == "playing";
		for(std::size_t i = 0; i < player.spec["bands"].get<std::size_t>(); ++i)
			bands.push_back(playing && i < player.spectrum.size() ? std::round(player.spectrum[i] * 1000) / 1000 : 0.0);
		return bands;
	}

	/// @brief Writes a player's state to its "into" path.
	/// @param name The sound.
	/// @param player The player.
	void publish(const std::string& name, const Player& player) {
		if(!player.spec.contains("into")) return;
		const std::string at = "audio." + name + ".into";
		const json state = {
		    {"status", player.status},
		    {"playing", player.status == "playing"},
		    {"position", milliseconds(player.position)},
		    {"duration", player.duration ? json(milliseconds(*player.duration)) : json(nullptr)},
		    {"progress", player.duration && *player.duration > 0
		                     ? json(std::clamp(player.position / *player.duration, 0.0, 1.0))
		                     : json(nullptr)},
		    {"volume", player.volume},
		    {"pan", player.pan},
		    {"rate", player.rate},
		    {"loop", player.loop},
		    {"level", std::round(player.level * 1000) / 1000},
		    {"underruns", player.underruns},
		    {"error", player.error ? json(*player.error) : json(nullptr)}};
		try {
			const json::json_pointer pointer = statePointer(player.spec["into"].get<std::string>(),
			                                                runtime.document.context(Scope{}, emptyObject()), at);
			json& placed = statePlace(runtime.versions.state(), pointer, at) = state;
			if(player.spec.contains("bands")) placed["spectrum"] = spectrumOf(player);
			runtime.document.markStateChanged(pointer);
			runtime.reconciler.requestStateUpdate();
		} catch(const std::exception& error) {
			runtime.report(error.what());
		}
	}

	/// @brief Runs the action "<sound>:<what>" with an event, if the screen has it.
	/// @param name The sound.
	/// @param what started, ended, underrun or error.
	/// @param event The event.
	void runAction(const std::string& name, const std::string& what, const json& event) {
		const std::string action = name + ":" + what;
		if(!runtime.window || !runtime.owner.hasAction(action)) return;
		runtime.runner.handle(json(action), runtime.window->root(), event, Scope{}, "audio." + name);
		if(runtime.document.stateChanged()) runtime.reconciler.requestStateUpdate();
	}

	/// @brief Takes an event of a player's voice, on the main thread.
	/// @param name The sound.
	/// @param voiceGeneration The voice's generation.
	/// @param event The event.
	void voiceEvent(const std::string& name, std::uint64_t voiceGeneration, const audio::Event& event) {
		const auto found = players.find(name);
		if(found == players.end() || found->second.generation != voiceGeneration) return;
		Player& player = found->second;
		player.position = event.position;
		if(event.duration) player.duration = event.duration;
		if(player.voice) {
			const audio::Levels levels = player.voice->levels();
			player.level = std::max(levels.peak[0], levels.peak[1]);
			if(player.spec.contains("bands"))
				player.spectrum = player.voice->spectrum(player.spec["bands"].get<std::size_t>());
		}
		const json data = {{"position", milliseconds(event.position)}, {"message", event.message}};
		switch(event.type) {
		case audio::EventType::Position: publish(name, player); return;
		case audio::EventType::Started:
			player.status = "playing";
			publish(name, player);
			runAction(name, "started", data);
			return;
		case audio::EventType::Ended:
			player.status = "stopped";
			player.level = 0;
			player.spectrum.clear();
			publish(name, player);
			runAction(name, "ended", data);
			return;
		case audio::EventType::Underrun:
			++player.underruns;
			publish(name, player);
			runAction(name, "underrun", data);
			return;
		case audio::EventType::Error:
			player.status = "error";
			player.error = event.message;
			player.level = 0;
			player.spectrum.clear();
			publish(name, player);
			runAction(name, "error", data);
			return;
		}
	}

	/// @brief The source of a declared file, if it is inside an allowed folder.
	/// @param file The path as the declaration writes it.
	/// @param error Receives why not.
	/// @return The source, or null.
	std::shared_ptr<audio::Source> fileSource(const std::string& file, std::string& error) const {
		if(file.starts_with(assetScheme)) { // an asset of the version shown, its bytes kept with the history
			const std::string asset = file.substr(assetScheme.size());
			const std::shared_ptr<const std::string> bytes = runtime.versions.assetBytes(asset);
			if(!bytes) {
				error = "no asset '" + asset.substr(0, 100) + "'";
				return nullptr;
			}
			if(assetTypeOf(asset) == soundType) return audio::soundSource(*bytes, error); // rendered as it plays
			const auto* data = reinterpret_cast<const std::uint8_t*>(bytes->data());
			return audio::Source::memory(std::vector<std::uint8_t>(data, data + bytes->size()),
			                             audio::encodingOf(asset));
		}
		const std::filesystem::path& base = runtime.document.baseDirectory();
		std::filesystem::path path(file);
		if(path.is_relative()) path = base / path;
		path = resolved(path);
		std::vector<std::filesystem::path> allowed = folders;
		if(!base.empty()) allowed.push_back(resolved(base));
		if(std::ranges::none_of(allowed, [&](const std::filesystem::path& folder) { return isInside(path, folder); })) {
			error = "'" + std::filesystem::path(file).filename().string() +
			        "' is outside the folders the host allows (the screen's own, Screen::allowAudioDirectory)";
			return nullptr;
		}
		if(assetTypeOf(file) == soundType) { // a sound description of the screen's folder
			std::ifstream in(path, std::ios::binary);
			std::string text((std::istreambuf_iterator<char>(in)), {});
			if(!in.eof() && !in) error = "'" + path.filename().string() + "' can't be read";
			if(text.size() > audio::SoundLimits{}.bytes) error = "'" + path.filename().string() + "' is too long";
			return error.empty() ? audio::soundSource(text, error) : nullptr;
		}
		return audio::Source::file(path, audio::encodingOf(file));
	}

	/// @brief Starts a URL's request, its bytes pushed into a stream as they arrive.
	/// @param name The sound.
	/// @param player The player.
	/// @param error Receives why it can't.
	/// @return The stream, or null.
	std::shared_ptr<audio::Source> urlSource(const std::string& name, Player& player, std::string& error) {
		network::Network* network = owner.network_;
		const std::string url = player.spec["url"].get<std::string>();
		if(!network) {
			error = "the host gives this screen no network";
			return nullptr;
		}
		if(const auto refused = network->check(url)) {
			error = *refused;
			return nullptr;
		}
		const auto capacity = static_cast<std::size_t>(player.spec.value("maxBytes", defaultStreamBytes));
		player.stream = audio::EncodedStream::create(std::min(capacity, maxStreamBytes), audio::encodingOf(url));
		network::Request request;
		request.url = url;
		request.stream = network::Stream::Bytes;
		request.maxBodyBytes = streamChunkBytes;
		const std::uint64_t voiceGeneration = generation + 1; // the voice this stream is made for
		const auto stillOurs = [this, name, voiceGeneration]() -> Player* {
			const auto found = players.find(name);
			return found != players.end() && found->second.generation == voiceGeneration ? &found->second : nullptr;
		};
		const auto ended = [this, name, stillOurs](const std::optional<std::string>& why) {
			Player* current = stillOurs();
			if(!current) return;
			if(current->stream) current->stream->finish();
			retire(std::move(current->transfer));
			if(!why) return;
			current->error = *why;
			publish(name, *current);
			runAction(name, "error", {{"message", *why}});
		};
		network::Handlers handlers;
		handlers.event = [stillOurs, ended](network::StreamEvent event) {
			Player* current = stillOurs();
			if(!current || !current->stream) return;
			if(!current->stream->push(event.data))
				ended("the stream's buffer is full (maxBytes): it plays what it holds, then ends");
		};
		handlers.done = [ended](const network::Response&) { ended(std::nullopt); };
		handlers.failed = [ended](const network::Failure& failure) { ended(failure.message); };
		player.transfer = network->start(std::move(request), std::move(handlers));
		return player.stream;
	}

	/// @brief The source a declaration names.
	/// @param name The sound.
	/// @param player The player.
	/// @param error Receives why it has none.
	/// @return The source, or null.
	std::shared_ptr<audio::Source> sourceOf(const std::string& name, Player& player, std::string& error) {
		const json& spec = player.spec;
		if(spec.contains("file")) return fileSource(spec["file"].get<std::string>(), error);
		if(spec.contains("url")) return urlSource(name, player, error);
		if(spec.contains("tone")) {
			const json& tone = spec["tone"];
			return audio::Source::tone(tone.value("frequency", 440.0), tone.value("seconds", 1.0),
			                           tone.value("amplitude", 0.5F));
		}
		const std::string named = spec.value("source", "");
		const auto found = sources.find(named);
		if(found == sources.end() || !found->second) {
			error = "the host gives no source named '" + named + "'";
			return nullptr;
		}
		return found->second;
	}

	/// @brief Makes a player's voice, unless it has one.
	/// @param name The sound.
	/// @param player The player.
	/// @return Why it can't, or nothing.
	std::optional<std::string> ensureVoice(const std::string& name, Player& player) {
		if(player.voice) return std::nullopt;
		if(!audio) return "this host plays no audio";
		std::string error;
		const std::shared_ptr<audio::Source> source = sourceOf(name, player, error);
		if(!source) return error;
		audio::VoiceOptions options;
		options.volume = player.volume;
		options.pan = player.pan;
		options.rate = player.rate;
		options.loop = player.loop;
		options.positionInterval = std::chrono::milliseconds{player.spec.value("updates", 250)};
		const std::uint64_t voiceGeneration = ++generation;
		options.onEvent = [this, kept = std::weak_ptr<bool>(alive), name, voiceGeneration](const audio::Event& event) {
			if(kept.lock()) voiceEvent(name, voiceGeneration, event);
		};
		player.voice = audio->createVoice(source, std::move(options), error);
		if(!player.voice) {
			drop(player);
			return error;
		}
		player.generation = voiceGeneration;
		player.duration = player.voice->duration();
		return std::nullopt;
	}

	/// @brief Plays a player, from a time if given; a stream that ended starts again from its source.
	/// @param name The sound.
	/// @param player The player.
	/// @param from Seconds, or null.
	/// @return Why it can't, or nothing.
	std::optional<std::string> play(const std::string& name, Player& player, const json& from) {
		if(const auto problem = ensureVoice(name, player)) return problem;
		if(!from.is_null() && !player.voice->seek(from.get<double>())) return "this sound can't seek";
		if(!player.voice->play()) { // a stream at its end, or a voice that failed: make it again
			drop(player);
			if(const auto problem = ensureVoice(name, player)) return problem;
			if(!player.voice->play()) return player.voice->error().value_or("this sound can't play");
		}
		player.status = "playing";
		player.position = player.voice->position();
		return std::nullopt;
	}

	/// @brief Runs one operation on a player.
	/// @param name The sound.
	/// @param operation The operation.
	/// @param step The step's values.
	/// @param where Its place.
	/// @return Why it can't play, or nothing.
	std::optional<std::string> operate(const std::string& name, const std::string& operation, const json& step,
	                                   const std::string& where) {
		Player& player = this->player(name, where);
		const json to = step.value("to", json());
		const double ms = step.contains("ms") ? numberIn(step["ms"], where + ".ms", 0, 3600000) : 0;
		std::optional<std::string> problem;
		if(operation == "play" || (operation == "toggle" && player.status != "playing")) {
			problem = play(name, player, step.contains("from")
			                                  ? json(numberIn(step["from"], where + ".from", 0, 1e9))
			                                  : json());
		} else if(operation == "pause" || operation == "toggle") {
			if(player.voice) player.voice->pause();
			if(player.status == "playing") player.status = "paused";
		} else if(operation == "stop") {
			if(player.voice) {
				if(ms > 0) player.voice->fade(0, ms / 1000, true);
				else player.voice->stop();
				player.position = ms > 0 ? player.position : player.voice->position();
			}
			player.status = "stopped";
		} else if(operation == "seek") {
			const double seconds = numberIn(to, where + ".to", 0, 1e9);
			problem = ensureVoice(name, player);
			if(!problem && !player.voice->seek(seconds)) problem = "this sound can't seek";
			if(!problem) player.position = player.voice->position();
		} else if(operation == "volume") {
			player.volume = static_cast<float>(numberIn(to, where + ".to", 0, 4));
			if(player.voice) player.voice->fade(player.volume, ms / 1000);
		} else if(operation == "pan") {
			player.pan = static_cast<float>(numberIn(to, where + ".to", -1, 1));
			if(player.voice) player.voice->setPan(player.pan);
		} else if(operation == "rate") {
			player.rate = static_cast<float>(numberIn(to, where + ".to", 0.25, 4));
			if(player.voice) player.voice->setRate(player.rate);
		} else if(operation == "loop") {
			player.loop = expect::boolean(to, where + ".to");
			if(player.voice) player.voice->setLoop(player.loop);
		}
		player.error = problem;
		if(problem && operation != "seek") player.status = "error";
		if(!problem && operation == "play") player.error.reset();
		publish(name, player);
		return problem;
	}
};

AudioPlayers::AudioPlayers(Engine& engine) : impl_(std::make_unique<Impl>(*this, engine)) {}

AudioPlayers::~AudioPlayers() {
	*impl_->alive = false;
	for(auto& [name, player] : impl_->players)
		if(player.voice) player.voice->stop();
}

void AudioPlayers::setEngine(std::shared_ptr<audio::Engine> engine) {
	if(engine == impl_->audio) return;
	stopAll();
	impl_->audio = std::move(engine);
}

std::shared_ptr<audio::Engine> AudioPlayers::engine() const { return impl_->audio; }

void AudioPlayers::setSource(const std::string& name, std::shared_ptr<audio::Source> source) {
	if(source) impl_->sources.insert_or_assign(name, std::move(source));
	else impl_->sources.erase(name);
}

void AudioPlayers::allowDirectory(const std::filesystem::path& directory) {
	std::error_code error;
	if(!std::filesystem::is_directory(directory, error)) return;
	const std::filesystem::path folder = resolved(directory);
	if(std::ranges::find(impl_->folders, folder) == impl_->folders.end()) impl_->folders.push_back(folder);
}

void AudioPlayers::documentChanged() {
	for(auto it = impl_->players.begin(); it != impl_->players.end();) {
		const json* spec = impl_->declaration(it->first);
		if(spec && *spec == it->second.spec) {
			++it;
			continue;
		}
		impl_->drop(it->second);
		it = impl_->players.erase(it);
	}
}

void AudioPlayers::stopAll() {
	for(auto& [name, player] : impl_->players) impl_->drop(player);
	impl_->players.clear();
}

std::optional<std::string> AudioPlayers::run(const std::string& name, const std::string& operation, const json& step,
                                             const std::string& where) {
	if(operation == "master") {
		const double volume = numberIn(step.value("to", json()), where + ".to", 0, 4);
		if(!impl_->audio) return "this host plays no audio";
		impl_->audio->setMasterVolume(static_cast<float>(volume));
		return std::nullopt;
	}
	return impl_->operate(name, operation, step, where);
}

std::optional<double> probeSound(const std::string& bytes, std::string_view type) {
	if(type == soundType) {
		std::string error;
		const auto score = audio::readSound(bytes, error);
		if(!score) throw DocumentError("the sound description is wrong: " + error);
		return score->seconds;
	}
	const audio::Encoding hint = type == "audio/wav"    ? audio::Encoding::Wav
	                             : type == "audio/flac" ? audio::Encoding::Flac
	                                                    : audio::Encoding::Mp3;
	std::string error;
	const auto seconds = audio::probeSeconds(
	    std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()), hint, error);
	if(!seconds) throw DocumentError("the sound doesn't play (" + error + ")");
	return seconds;
}

} // namespace affresco::format
