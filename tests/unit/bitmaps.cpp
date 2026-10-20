/// @file
/// @brief Unit tests for images in the render module: decoding PNG and JPEG within limits (Bitmap), the cache that
/// decodes files on a worker thread and delivers them on the main one (ImageCache), only from allowed folders, the
/// host's named images, fitting an image to a panel (ImageView), and shelf packing.
///
/// The images are the rendering check's (tests/render/screens/images, made by its make.py).

#include <affresco/render/Bitmap.hpp>
#include <affresco/render/ImageCache.hpp>
#include <affresco/render/ImageView.hpp>

#include "detail/ShelfPacker.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

using affresco::Bitmap;
using affresco::ImageCache;
using affresco::ImageFit;
using affresco::ImageView;
using affresco::Rect;

namespace {

/// @brief The rendering check's images.
std::filesystem::path images() {
	return std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR).parent_path() / "screens/images";
}

/// @brief A file's bytes.
/// @param path The file.
std::vector<std::uint8_t> bytesOf(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// @brief An Executor that keeps tasks posted from any thread until the test runs them.
class LockedExecutor final : public affresco::Executor {
public:
	/// @brief Keeps a task; any thread.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
	}
	/// @brief Runs the tasks posted so far.
	/// @return How many ran.
	std::size_t runPosted() {
		std::vector<std::function<void()>> tasks;
		{
			const std::scoped_lock lock(mutex_);
			tasks.swap(tasks_);
		}
		for(const auto& task : tasks) task();
		return tasks.size();
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

/// @brief A 1 by 1 RGBA PNG whose compressed pixels inflate to far more than its one pixel: a literal, then
/// back-references of 258 bytes in fixed Huffman codes, as a decompression bomb is made.
/// @param inflated About how many bytes the pixels inflate to.
/// @return The file's bytes (stb_image checks no CRC or Adler-32, so they are left zero).
std::vector<std::uint8_t> inflatingPng(std::size_t inflated) {
	std::vector<std::uint8_t> zlib{0x78, 0x01};
	std::uint32_t bits = 0;
	int used = 0;
	const auto put = [&](std::uint32_t value, int count) { // least significant bit first
		bits |= value << used;
		for(used += count; used >= 8; used -= 8, bits >>= 8) zlib.push_back(static_cast<std::uint8_t>(bits));
	};
	const auto code = [&](std::uint32_t value, int count) { // Huffman codes go most significant bit first
		for(int bit = count - 1; bit >= 0; --bit) put((value >> bit) & 1u, 1);
	};
	put(1, 1); // the final block,
	put(1, 2); // in fixed codes:
	std::size_t made = 0;
	for(; made < std::min<std::size_t>(inflated, 5); ++made) code(0x30, 8); // literal zeros: a filter byte, a pixel
	for(; made < inflated; made += 258) {
		code(0xC5, 8); // length 258 (code 285)
		code(0, 5);    // at distance 1
	}
	code(0, 7); // the end of the block
	put(0, 7);  // to a whole byte
	zlib.insert(zlib.end(), 4, 0);

	std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
	const auto chunk = [&png](const char* type, const std::vector<std::uint8_t>& data) {
		const auto size = static_cast<std::uint32_t>(data.size());
		for(int shift = 24; shift >= 0; shift -= 8) png.push_back(static_cast<std::uint8_t>(size >> shift));
		png.insert(png.end(), type, type + 4);
		png.insert(png.end(), data.begin(), data.end());
		png.insert(png.end(), 4, 0);
	};
	chunk("IHDR", {0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0}); // 1 by 1, 8-bit RGBA
	chunk("IDAT", zlib);
	chunk("IEND", {});
	return png;
}

} // namespace

TEST_CASE("bitmaps: PNG and JPEG decode to premultiplied RGBA, and transparent pixels lose their colour") {
	const auto png = Bitmap::load(images() / "landscape.png", {});
	CHECK(png->width() == 160);
	CHECK(png->height() == 100);
	CHECK(png->pixels().size() == 160u * 100u * 4u);
	CHECK(png->pixels()[3] == 255); // the frame is opaque

	const auto jpeg = Bitmap::load(images() / "landscape.jpg", {});
	CHECK(jpeg->width() == 160);
	CHECK(jpeg->height() == 100);
	CHECK(jpeg->id() != png->id());

	const auto badge = Bitmap::load(images() / "badge.png", {});
	CHECK(badge->pixels()[3] == 0); // the corner is transparent ...
	CHECK(badge->pixels()[0] == 0); // ... so its colour is premultiplied away

	const std::array<std::uint8_t, 8> two{200, 100, 50, 128, 10, 20, 30, 255};
	const auto made = Bitmap::fromPixels(2, 1, two, {});
	CHECK(made->pixels()[0] == 100); // 200 * 128 / 255, rounded
	CHECK(made->pixels()[3] == 128);
	CHECK(made->pixels()[4] == 10); // opaque: unchanged
}

TEST_CASE("bitmaps: what isn't an image within the limits is refused before it is decoded") {
	const std::vector<std::uint8_t> png = bytesOf(images() / "landscape.png");
	CHECK_THROWS_AS((void)Bitmap::decode(png, {.fileBytes = 64, .side = 8192, .pixels = 1 << 24}), std::runtime_error);
	CHECK_THROWS_AS((void)Bitmap::decode(png, {.fileBytes = 1 << 20, .side = 100, .pixels = 1 << 24}),
	                std::runtime_error); // 160 wide
	CHECK_THROWS_AS((void)Bitmap::decode(png, {.fileBytes = 1 << 20, .side = 8192, .pixels = 15'999}),
	                std::runtime_error); // 16,000 pixels
	CHECK_NOTHROW((void)Bitmap::decode(png, {.fileBytes = 1 << 20, .side = 160, .pixels = 16'000}));

	const std::vector<std::uint8_t> truncated(png.begin(), png.begin() + 100);
	CHECK_THROWS_AS((void)Bitmap::decode(truncated, {}), std::runtime_error);
	const std::array<std::uint8_t, 4> text{'t', 'e', 'x', 't'};
	CHECK_THROWS_AS((void)Bitmap::decode(text, {}), std::runtime_error);
	CHECK_THROWS_AS((void)Bitmap::load(images(), {}), std::runtime_error); // a folder
	CHECK_THROWS_AS((void)Bitmap::fromPixels(0, 1, {}, {}), std::invalid_argument);
	CHECK_THROWS_AS((void)Bitmap::fromPixels(2, 2, text, {}), std::invalid_argument);
}

TEST_CASE("bitmaps: compressed pixels that inflate far beyond the image's size are refused, not allocated") {
	const std::vector<std::uint8_t> bomb = inflatingPng(std::size_t{64} << 20); // 64 MiB from about 400 KiB
	CHECK(bomb.size() < (std::size_t{1} << 19));
	CHECK_THROWS_AS((void)Bitmap::decode(bomb, {}), std::runtime_error);
	const std::vector<std::uint8_t> exact = inflatingPng(5); // a filter byte and one pixel: an image
	CHECK(Bitmap::decode(exact, {})->width() == 1);
}

TEST_CASE("image cache: without an executor a file is decoded at once, once, and kept") {
	ImageCache cache;
	cache.allowDirectory(images());
	std::shared_ptr<const Bitmap> first, second;
	{
		const ImageCache::Request request =
		    cache.load(images() / "badge.png", [&](const auto& bitmap) { first = bitmap; });
		REQUIRE(first);
	}
	const ImageCache::Request again =
	    cache.load(images() / "../images/badge.png", [&](const auto& bitmap) { second = bitmap; });
	CHECK(second == first); // the same file, written another way: decoded once
	CHECK(cache.bytes() == first->memoryBytes());
}

TEST_CASE("image cache: a file outside the allowed folders is never read") {
	ImageCache cache;
	CHECK_FALSE(cache.allows(images() / "badge.png")); // nothing is allowed until the host says so
	cache.allowDirectory(images() / "missing-folder");
	CHECK_FALSE(cache.allows(images() / "badge.png"));
	cache.allowDirectory(images());
	CHECK(cache.allows(images() / "badge.png"));
	CHECK(cache.allows(images() / "sub" / "x.png"));
	CHECK_FALSE(cache.allows(images() / ".." / "fonts" / "LICENSE-NotoSansArabic.txt"));
	CHECK_FALSE(cache.allows(images().parent_path()));

	ImageCache closed;
	bool delivered = false;
	std::shared_ptr<const Bitmap> got = Bitmap::fromPixels(1, 1, std::array<std::uint8_t, 4>{}, {});
	(void)closed.load(images() / "badge.png", [&](const auto& bitmap) {
		delivered = true;
		got = bitmap;
	});
	CHECK(delivered);
	CHECK_FALSE(got);
}

TEST_CASE("image cache: with an executor, files decode on the worker and arrive on the main thread; a cancelled "
          "request never runs") {
	LockedExecutor executor;
	auto cache = std::make_unique<ImageCache>();
	cache->setExecutor(&executor);
	cache->allowDirectory(images());

	std::shared_ptr<const Bitmap> landscape, banner;
	int landscapeDeliveries = 0;
	const ImageCache::Request a = cache->load(images() / "landscape.png", [&](const auto& bitmap) {
		landscape = bitmap;
		++landscapeDeliveries;
	});
	const ImageCache::Request b = cache->load(images() / "landscape.png", [&](const auto&) { ++landscapeDeliveries; });
	ImageCache::Request c = cache->load(images() / "banner.png", [&](const auto& bitmap) { banner = bitmap; });
	CHECK_FALSE(landscape); // not before the main thread runs what the worker posted
	c.cancel();

	cache->finishPending();
	CHECK(cache->pending() == 0);
	CHECK(landscapeDeliveries == 2); // one file, decoded once, delivered to both
	REQUIRE(landscape);
	CHECK(landscape->width() == 160);
	CHECK_FALSE(banner);             // cancelled
	CHECK(executor.runPosted() > 0); // what the worker posted does nothing more

	// A broken file is delivered as nothing; the cache goes before what it posted
	bool broken = false;
	const ImageCache::Request d = cache->load(images() / "make.py", [&](const auto& bitmap) { broken = !bitmap; });
	cache->finishPending();
	CHECK(broken);
	(void)cache->load(images() / "badge.png", [](const auto&) { FAIL("delivered after the cache was destroyed"); });
	cache.reset();
	executor.runPosted();
}

TEST_CASE("image cache: named images are delivered to who watches them, now and each time they change") {
	ImageCache cache;
	std::vector<const Bitmap*> seen;
	ImageCache::Request watching = cache.watch("avatar", [&](const auto& bitmap) { seen.push_back(bitmap.get()); });
	REQUIRE(seen.size() == 1);
	CHECK(seen[0] == nullptr);
	const auto avatar = Bitmap::fromPixels(1, 1, std::array<std::uint8_t, 4>{255, 0, 0, 255}, {});
	cache.setNamed("avatar", avatar);
	cache.setNamed("other", avatar);
	REQUIRE(seen.size() == 2);
	CHECK(seen[1] == avatar.get());
	CHECK(cache.named("avatar") == avatar);
	watching.cancel();
	cache.setNamed("avatar", nullptr);
	CHECK(seen.size() == 2);
	CHECK_FALSE(cache.named("avatar"));
}

TEST_CASE("image cache: decoded files beyond the bound are dropped, least recently used first, never one in use") {
	ImageCache cache({.bitmap = {}, .bytes = 70'000, .queued = 256});
	cache.allowDirectory(images());
	std::shared_ptr<const Bitmap> held;
	(void)cache.load(images() / "landscape.png", [&](const auto& bitmap) { held = bitmap; }); // 64,000 bytes, held
	(void)cache.load(images() / "badge.png", [](const auto&) {});                             // 9,216 bytes
	CHECK(cache.bytes() == held->memoryBytes()); // the badge went over the bound; the held landscape stays
	(void)cache.load(images() / "banner.png", [](const auto&) {});
	CHECK(cache.bytes() == held->memoryBytes());
}

TEST_CASE("image view: contain, cover and fill place the image like CSS object-fit") {
	const Rect frame{10.0f, 20.0f, 210.0f, 120.0f}; // 200 by 100
	CHECK(ImageView::fitted(ImageFit::fill, frame, 50, 50) == frame);
	CHECK(ImageView::fitted(ImageFit::contain, frame, 50, 50) == Rect{60.0f, 20.0f, 160.0f, 120.0f});
	CHECK(ImageView::fitted(ImageFit::cover, frame, 50, 50) == Rect{10.0f, -30.0f, 210.0f, 170.0f});
	CHECK(ImageView::fitted(ImageFit::contain, frame, 400, 100) == Rect{10.0f, 45.0f, 210.0f, 95.0f});
	CHECK(ImageView::fitted(ImageFit::cover, frame, 0, 0) == frame); // nothing to fit
}

TEST_CASE("image view: a file shown arrives later, and the view can go before it does") {
	LockedExecutor executor;
	ImageCache cache;
	cache.setExecutor(&executor);
	cache.allowDirectory(images());
	auto view = std::make_unique<ImageView>();
	view->showFile(cache, images() / "badge.png");
	cache.finishPending();
	REQUIRE(view->bitmap());
	CHECK(view->bitmap()->width() == 48);

	view->showFile(cache, images() / "banner.png");
	view.reset(); // its request goes with it
	cache.finishPending();

	ImageView named;
	named.showNamed(cache, "chart");
	CHECK_FALSE(named.bitmap());
	cache.setNamed("chart", Bitmap::fromPixels(2, 2, std::array<std::uint8_t, 16>{}, {}));
	REQUIRE(named.bitmap());
	named.setBitmap(nullptr); // stops following the name
	cache.setNamed("chart", Bitmap::fromPixels(1, 1, std::array<std::uint8_t, 4>{}, {}));
	CHECK_FALSE(named.bitmap());
	executor.runPosted();
}

TEST_CASE("shelf packer: boxes go on the shortest shelf they fit, and a full page refuses more") {
	affresco::detail::ShelfPacker packer(100, 2);
	CHECK(packer.place(40, 10, 0) == std::array<int, 2>{0, 2});
	CHECK(packer.place(40, 20, 0) == std::array<int, 2>{0, 12});
	CHECK(packer.place(50, 8, 0) == std::array<int, 2>{40, 2}); // the shorter shelf
	CHECK(packer.place(101, 1, 0) == std::nullopt);
	CHECK(packer.place(100, 69, 0) == std::nullopt); // 32 + 69 rows: one more than the page has
	CHECK(packer.place(100, 68, 0) == std::array<int, 2>{0, 32});
	CHECK(packer.place(10, 1, 0) == std::array<int, 2>{90, 2}); // the shortest shelf with room
	CHECK(packer.place(100, 2, 0) == std::nullopt);
}
