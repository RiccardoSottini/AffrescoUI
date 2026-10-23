/// @file
/// @brief Fuzzes the images a screen edits: the "images" section's entries and the "image" step's operations with
/// any "with", as a document or a revision may write them (format/src/imaging). A malformed entry or a step that
/// can't be done must be refused with a reason, never crash, hang or leave an image outside its limits.
///
/// The input is a JSON object `{ "images": {...}, "steps": [{ "image": operation, "of": name, "with": {...} }] }`.
/// Each entry's limits are lowered to small ones (an entry's own lower ones stay), so that inputs reach them and each
/// run stays quick. Every step runs as the "image" step runs it, without a window (opening a file fails), and the
/// image's snapshot, which the screen's state receives, must then be one that can be written out.

#include "Fuzz.hpp"

#include "ImageDocuments.hpp"
#include "ImageOperations.hpp"

#include <affresco/core/Json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

using affresco::json;

namespace {

constexpr std::size_t maxImages = 4; ///< The entries a run makes, at most.
constexpr std::size_t maxSteps = 48; ///< The steps a run plays, at most.

/// @brief The limits each entry gets at most, by their keys in an entry's "limits".
constexpr std::array<std::pair<std::string_view, std::int64_t>, 9> fuzzLimits{{{"side", 256},
                                                                               {"pixels", 65536},
                                                                               {"layers", 12},
                                                                               {"groupDepth", 4},
                                                                               {"undoSteps", 12},
                                                                               {"undoBytes", 4 << 20},
                                                                               {"filterRadius", 12},
                                                                               {"brushSize", 64},
                                                                               {"cacheBytes", 8 << 20}}};

/// @brief An entry with its limits lowered to the fuzzer's; one that isn't an object, or whose "limits" isn't one,
/// stays as it is, for the reader to refuse.
/// @param entry The entry.
/// @return It, lowered.
json lowered(json entry) {
	if(!entry.is_object()) return entry;
	json& limits = entry["limits"];
	if(limits.is_null()) limits = json::object();
	if(!limits.is_object()) return entry;
	for(const auto& [key, most] : fuzzLimits) {
		const std::string name(key);
		if(!limits.contains(name) ||
		   (limits[name].is_number() && limits[name].get<double>() > static_cast<double>(most)))
			limits[name] = most;
	}
	return entry;
}

/// @brief Checks what the screen would receive of an image: its snapshot written out, its size within its limits.
/// @param image The image.
void checkSnapshot(const affresco::format::editing::ImageDocument& image) {
	const json shown = affresco::format::editing::snapshot(image);
	try {
		(void)shown.dump();
	} catch(const json::exception&) {
		affresco::fuzz::broken("an image's snapshot can be written out");
	}
	const auto side = static_cast<std::int64_t>(image.limits.side);
	if(shown.value("width", 0) > side || shown.value("height", 0) > side)
		affresco::fuzz::broken("an image stays within its side limit");
}

/// @brief Plays one step as the "image" step does: a refused one leaves its reason in the image's message.
/// @param images The images made.
/// @param step The step.
/// @param only The image a step without "of" edits (the step's rule with a single image).
void play(affresco::format::editing::ImageDocuments& images, const json& step, const std::string& only) {
	using namespace affresco::format::editing;
	if(!step.is_object() || !step.contains("image") || !step["image"].is_string()) return;
	const ImageOperation* operation = findImageOperation(step["image"].get<std::string>());
	const std::string name = step.contains("of") && step["of"].is_string() ? step["of"].get<std::string>() : only;
	ImageDocument* image = images.find(name);
	if(operation == nullptr || image == nullptr) return;
	const json with = step.contains("with") ? step["with"] : json::object();
	if(!with.is_object()) return;
	try {
		operation->run(*image, with, nullptr);
	} catch(const std::exception& error) { // what the step catches: its message, then onError or a failure
		image->message = error.what();
	}
	checkSnapshot(*image);
}

} // namespace

/// @brief Quiets the log, which a refused entry or step writes to.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const json input = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(!input.is_object() || !input.contains("images") || !input["images"].is_object()) return 0;
	affresco::format::editing::ImageDocuments images;
	std::size_t made = 0;
	std::string first;
	for(const auto& [name, entry] : input["images"].items()) {
		if(made++ == maxImages) break;
		if(first.empty()) first = name;
		try {
			checkSnapshot(images.image(name, lowered(entry), "images." + name));
		} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): refusing is the expected outcome
		}
	}
	if(input["images"].size() != 1) first.clear(); // with several, a step names its image
	const json steps = input.value("steps", json::array());
	if(!steps.is_array()) return 0;
	for(std::size_t index = 0; index < std::min(steps.size(), maxSteps); ++index) play(images, steps[index], first);
	return 0;
}
