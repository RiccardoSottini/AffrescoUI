/// @file
/// @brief The imaging module on the GPU: tile residency within a budget, least recently used first; the canvas's view
/// and mip level; and the canvas compositor against the CPU reference (every blend mode, masks, clipping, groups,
/// adjustments, a coarser mip level and a budget too small for the tiles), in a hidden window's context. Without a
/// display the GPU cases say so and pass.

#include "affresco/imaging/Canvas.hpp"
#include "affresco/imaging/Composite.hpp"
#include "affresco/render/Application.hpp"

#include <glad/glad.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

using namespace affresco::imaging;

namespace {

/// @brief A resident tile of a given size.
/// @param texture Its texture.
/// @param bytes Its size.
[[nodiscard]] Resident resident(unsigned texture, std::size_t bytes) { return Resident{texture, 1, bytes}; }

/// @brief A key of image 7's level-0 tile in a column.
/// @param column The column.
[[nodiscard]] ResidentKey keyAt(int column) { return ResidentKey{7, TileKey{0, column, 0}}; }

/// @brief A frame whose passes are textures and framebuffers of its own, freed with it.
class TestFrame final : public affresco::SurfaceFrame {
public:
	using affresco::SurfaceFrame::SurfaceFrame;
	TestFrame(const TestFrame&) = delete;
	TestFrame& operator=(const TestFrame&) = delete;
	/// @brief Frees the passes.
	~TestFrame() override {
		for(const auto& [name, pass] : passes_) {
			glDeleteFramebuffers(1, &pass.second.framebuffer);
			glDeleteTextures(1, &pass.second.texture);
		}
	}

	/// @brief A pass by name, made (or remade at another size) on first use.
	/// @param name Its name.
	/// @param spec Its size and format.
	[[nodiscard]] affresco::SurfacePass pass(std::string_view name, const affresco::PassSpec& spec) override {
		auto& [made, pass] = passes_[std::string(name)];
		if(pass.texture != 0 && made.width == spec.width && made.height == spec.height && made.format == spec.format)
			return pass;
		if(pass.texture == 0) {
			glGenTextures(1, &pass.texture);
			glGenFramebuffers(1, &pass.framebuffer);
		}
		made = spec;
		pass.width = spec.width, pass.height = spec.height;
		const bool half = spec.format == affresco::PassFormat::rgba16f;
		glBindTexture(GL_TEXTURE_2D, pass.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, half ? GL_RGBA16F : GL_RGBA8, spec.width, spec.height, 0, GL_RGBA,
		             half ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass.texture, 0);
		return pass;
	}

private:
	std::map<std::string, std::pair<affresco::PassSpec, affresco::SurfacePass>> passes_; ///< The passes, by name.
};

/// @brief A hidden window's context, or none without a display.
/// @param application Where the application goes.
/// @return Whether there is a context.
[[nodiscard]] bool openContext(std::optional<affresco::Application>& application) {
	try {
		application.emplace(affresco::ApplicationOptions{nullptr, true});
		(void)application->createWindow("imaging", 64, 64); // its context is current
		return true;
	} catch(const std::exception& error) {
		MESSAGE("no OpenGL context (", error.what(), "): the GPU comparison is skipped");
		return false;
	}
}

/// @brief Draws a model's document on a canvas of a size and reads it back.
/// @param model The model.
/// @param surface The canvas, prepared.
/// @param width The surface's width.
/// @param height Its height.
/// @param params The canvas's parameters.
/// @return width × height × 4 bytes, premultiplied, top row first.
[[nodiscard]] std::vector<std::uint8_t> draw(CanvasSurface& surface, int width, int height,
                                             const affresco::json& params) {
	TestFrame frame(params);
	const affresco::SurfacePass target = frame.pass("output", {width, height, affresco::PassFormat::rgba8});
	frame.width = width, frame.height = height, frame.framebuffer = target.framebuffer;
	surface.render(frame);
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	std::vector<std::uint8_t> read(static_cast<std::size_t>(width * height * 4));
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, read.data());
	std::vector<std::uint8_t> flipped(read.size());
	const auto row = static_cast<std::size_t>(width * 4);
	for(std::size_t y = 0; y < static_cast<std::size_t>(height); ++y)
		std::copy_n(read.begin() + static_cast<std::ptrdiff_t>((static_cast<std::size_t>(height) - 1 - y) * row), row,
		            flipped.begin() + static_cast<std::ptrdiff_t>(y * row));
	CHECK(glGetError() == GL_NO_ERROR);
	return flipped;
}

/// @brief How far the GPU's pixels are from the CPU's.
struct Difference {
	int largest = 0;        ///< The largest difference of a channel, in 8-bit steps.
	std::size_t pixels = 0; ///< Pixels with a channel more than 3 steps off.
};

/// @brief How far the GPU's premultiplied pixels are from the CPU's straight ones, premultiplied.
/// @param gpu The GPU's.
/// @param cpu The CPU's.
[[nodiscard]] Difference differenceOf(const std::vector<std::uint8_t>& gpu, const std::vector<std::uint8_t>& cpu) {
	REQUIRE(gpu.size() == cpu.size());
	Difference difference;
	for(std::size_t index = 0; index < cpu.size(); index += 4) {
		const int alpha = cpu[index + 3];
		int largest = 0;
		for(std::size_t channel = 0; channel < 4; ++channel) {
			const int expected = channel == 3 ? alpha : (cpu[index + channel] * alpha + 127) / 255;
			largest = std::max(largest, std::abs(expected - gpu[index + channel]));
		}
		difference.largest = std::max(difference.largest, largest);
		difference.pixels += largest > 3 ? 1 : 0;
	}
	return difference;
}

/// @brief Compares a canvas of a document at a zoom with the CPU compositor's, at the level it draws from.
/// @param model The model.
/// @param zoom The zoom (1 or a power of 1/2).
/// @return How far the GPU's pixels are from the CPU's.
[[nodiscard]] Difference compare(const std::shared_ptr<CanvasModel>& model, double zoom = 1.0) {
	const Document& document = model->document();
	const int level = canvasLevel(zoom, document.selection().levels());
	const int width = (document.width() + (1 << level) - 1) >> level;
	const int height = (document.height() + (1 << level) - 1) >> level;
	CanvasSurface surface(model);
	surface.prepare({width, height, 1.0f, affresco::surfaceShaderPrelude()});
	const affresco::json params{{"zoom", zoom}, {"x", width * 0.5 / zoom}, {"y", height * 0.5 / zoom},
	                            {"checker", false}};
	const std::vector<std::uint8_t> gpu = draw(surface, width, height, params);
	surface.release();
	return differenceOf(gpu, flatten(document, level, 0, 0, width, height));
}

/// @brief Paints a rectangle of an image with a colour that changes across it.
/// @param image The image.
/// @param left Its left, top, right and bottom.
/// @param from The colour at the left.
/// @param to The colour at the right.
void paint(Image& image, std::array<int, 4> box, const Pixel& from, const Pixel& to) {
	for(int y = box[1]; y < box[3]; ++y)
		for(int x = box[0]; x < box[2]; ++x) {
			const float t = static_cast<float>(x - box[0]) / static_cast<float>(box[2] - box[0]);
			const float u = static_cast<float>(y - box[1]) / static_cast<float>(box[3] - box[1]);
			image.setPixel(x, y,
			               {from.red + (to.red - from.red) * t, from.green + (to.green - from.green) * u,
			                from.blue + (to.blue - from.blue) * t, from.alpha + (to.alpha - from.alpha) * u});
		}
}

/// @brief A document of 300 × 200 (two columns of tiles, the second cut) with a painted bottom layer.
/// @param limits The limits.
[[nodiscard]] std::unique_ptr<Document> painted(const Limits& limits = {}) {
	auto document = std::make_unique<Document>(300, 200, limits);
	paint(*document->image(document->activeLayer()->pixels), {0, 0, 300, 200}, {0.9f, 0.1f, 0.2f, 1.0f},
	      {0.1f, 0.8f, 0.7f, 1.0f});
	return document;
}

/// @brief Adds a pixel layer painted over a box.
/// @param document The document.
/// @param name Its name.
/// @param box Its box.
/// @return Its id.
std::uint64_t addPainted(Document& document, const std::string& name, std::array<int, 4> box) {
	const std::uint64_t id = document.addPixelLayer(name);
	paint(*document.image(document.layer(id)->pixels), box, {0.2f, 0.6f, 1.0f, 0.4f}, {1.0f, 0.9f, 0.1f, 1.0f});
	return id;
}

} // namespace

TEST_CASE("imaging residency: within the budget, least recently used evicted first, stale entries forgotten") {
	TileResidency residency(300);
	std::vector<Resident> evicted;
	CHECK(residency.admit(keyAt(0), resident(1, 100), evicted));
	CHECK(residency.admit(keyAt(1), resident(2, 100), evicted));
	CHECK(residency.admit(keyAt(2), resident(3, 100), evicted));
	CHECK(evicted.empty());
	CHECK(residency.bytes() == 300);
	REQUIRE(residency.find(keyAt(0)) != nullptr); // now the most recent
	CHECK(residency.admit(keyAt(3), resident(4, 100), evicted));
	REQUIRE(evicted.size() == 1);
	CHECK(evicted[0].texture == 2);
	CHECK(residency.find(keyAt(1)) == nullptr);
	CHECK(residency.evictions() == 1);
	CHECK(residency.admit(keyAt(4), resident(0, 0), evicted)); // a transparent tile costs nothing
	CHECK(evicted.empty());
	const std::optional<Resident> forgotten = residency.forget(keyAt(3));
	REQUIRE(forgotten);
	CHECK(forgotten->texture == 4);
	CHECK(residency.bytes() == 200);
	CHECK_FALSE(residency.forget(keyAt(3)));
	CHECK_FALSE(residency.admit(keyAt(5), resident(5, 301), evicted)); // over the budget alone
	CHECK(residency.size() == 3);
	residency.clear(evicted);
	CHECK(evicted.size() == 3);
	CHECK(residency.bytes() == 0);
}

TEST_CASE("imaging canvas: the view maps both ways and the zoom picks the mip level") {
	const CanvasView view{0.25, 1000.0, 500.0, 800, 600};
	const auto [x, y] = view.toDocument(0.0, 0.0);
	CHECK(x == doctest::Approx(-600.0));
	CHECK(y == doctest::Approx(-700.0));
	const auto [back, down] = view.toSurface(x, y);
	CHECK(back == doctest::Approx(0.0));
	CHECK(down == doctest::Approx(0.0));
	CHECK(canvasLevel(4.0, 8) == 0);
	CHECK(canvasLevel(1.0, 8) == 0);
	CHECK(canvasLevel(0.5, 8) == 1);
	CHECK(canvasLevel(0.3, 8) == 2);
	CHECK(canvasLevel(1.0 / 1024.0, 8) == 7);
	CHECK(canvasLevel(0.5, 1) == 0);
}

TEST_CASE("imaging canvas: the GPU matches the CPU compositor") {
	std::optional<affresco::Application> application;
	if(!openContext(application)) return;
	SUBCASE("every blend mode, at an opacity") {
		for(std::size_t mode = 0; mode < blendModeCount; ++mode) {
			auto document = painted();
			const std::uint64_t top = addPainted(*document, "Top", {20, 10, 290, 190});
			document->changeLayer(top, "Blend", [mode](Layer& layer) {
				layer.blend = static_cast<BlendMode>(mode), layer.opacity = 0.8f;
			});
			INFO("blend mode ", mode);
			CHECK(compare(std::make_shared<CanvasModel>(std::move(document))).largest <= 3);
		}
	}
	SUBCASE("masks, clipping, groups and hidden layers") {
		auto document = painted();
		const std::uint64_t below = addPainted(*document, "Below", {40, 30, 280, 170});
		document->addMask(below);
		paint(*document->image(document->layer(below)->mask), {0, 0, 150, 200}, {0.0f, 0.0f, 0.0f, 0.0f},
		      {1.0f, 1.0f, 1.0f, 1.0f});
		const std::uint64_t clipped = addPainted(*document, "Clipped", {0, 0, 200, 120});
		document->changeLayer(clipped, "Clip", [](Layer& layer) { layer.clipped = true; });
		const std::uint64_t hidden = addPainted(*document, "Hidden", {0, 0, 300, 200});
		document->changeLayer(hidden, "Hide", [](Layer& layer) { layer.visible = false; });
		const std::uint64_t group = document->addGroup("Group");
		const std::uint64_t inner = addPainted(*document, "Inner", {100, 50, 300, 200});
		document->moveLayer(inner, group, 0);
		document->changeLayer(inner, "Blend", [](Layer& layer) { layer.blend = BlendMode::multiply; });
		document->changeLayer(group, "Opacity", [](Layer& layer) { layer.opacity = 0.6f; });
		CHECK(compare(std::make_shared<CanvasModel>(std::move(document))).largest <= 3);
	}
	SUBCASE("a coarser mip level, and a budget smaller than the tiles") {
		auto document = painted();
		(void)addPainted(*document, "Top", {20, 10, 290, 190});
		CHECK(compare(std::make_shared<CanvasModel>(std::move(document)), 0.5).largest <= 3);
		Limits small;
		small.gpuBytes = std::size_t{256} * 256 * 4 * 2; // two tiles of four
		auto tight = painted(small);
		(void)addPainted(*tight, "Top", {20, 10, 290, 190});
		const auto model = std::make_shared<CanvasModel>(std::move(tight));
		CHECK(compare(model).largest <= 3);
		CHECK(model->statistics.evictions > 0);
		CHECK(model->statistics.residentBytes <= small.gpuBytes);
	}
}

TEST_CASE("imaging canvas: the GPU matches the CPU compositor on every adjustment") {
	std::optional<affresco::Application> application;
	if(!openContext(application)) return;
	for(std::size_t kind = 0; kind < adjustmentKindCount; ++kind) {
		auto document = painted();
		(void)addPainted(*document, "Top", {20, 10, 290, 190});
		Adjustment adjustment;
		adjustment.kind = static_cast<AdjustmentKind>(kind);
		adjustment.gamma = 1.6f, adjustment.curve = {{0.0f, 0.1f}, {0.5f, 0.7f}, {1.0f, 0.9f}};
		adjustment.hue = 40.0f, adjustment.saturation = 0.3f, adjustment.lightness = 0.1f;
		adjustment.balance = {0.3f, -0.2f, 0.1f}, adjustment.brightness = 0.1f, adjustment.contrast = 0.2f;
		adjustment.gradient.stops = {{0.0f, {0.1f, 0.0f, 0.4f, 1.0f}}, {1.0f, {1.0f, 0.9f, 0.3f, 1.0f}}};
		const std::uint64_t id = document->addAdjustment("Adjustment", adjustment);
		document->changeLayer(id, "Opacity", [](Layer& layer) { layer.opacity = 0.7f; });
		INFO("adjustment ", kind);
		const Difference difference = compare(std::make_shared<CanvasModel>(std::move(document)));
		// A threshold flips where the half-float pass and the CPU's floats round the luminosity either side
		if(adjustment.kind == AdjustmentKind::threshold) CHECK(difference.pixels <= 300 * 200 / 1000);
		else CHECK(difference.largest <= 3);
	}
}

TEST_CASE("imaging canvas: a view far off the document composites nothing") {
	std::optional<affresco::Application> application;
	if(!openContext(application)) return;
	const auto model = std::make_shared<CanvasModel>(painted());
	CanvasSurface surface(model);
	surface.prepare({64, 64, 1.0f, affresco::surfaceShaderPrelude()});
	for(const double far : {1e300, -1e300, 1e12}) {
		CAPTURE(far);
		(void)draw(surface, 64, 64, affresco::json{{"zoom", 1.0}, {"x", far}, {"y", -far}, {"checker", false}});
		CHECK(model->statistics.regionWidth == 0);
		CHECK(model->statistics.regionHeight == 0);
	}
	(void)draw(surface, 64, 64, affresco::json{{"zoom", 1.0}, {"x", 32.0}, {"y", 32.0}, {"checker", false}});
	CHECK(model->statistics.regionWidth > 0);
	surface.release();
}
