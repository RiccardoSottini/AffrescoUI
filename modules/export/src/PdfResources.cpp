/// @file
/// @brief detail::PdfResources: images, transparency states and the shared resource dictionary.

#include "PdfResources.hpp"

#include "PdfShapes.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace affresco::detail {

PdfResources::PdfResources(PdfObjects& objects, const PdfLimits& limits)
    : objects_(objects), fonts_(objects, limits.fontBytes), id_(objects.reserve()), pixelLimit_(limits.imagePixels) {}

std::string PdfResources::alpha(float alpha) {
	const int level = static_cast<int>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
	if(level >= 255) return {};
	auto [found, added] = alphas_.try_emplace(level, 0);
	if(added) {
		found->second = objects_.reserve();
		std::string body = "<< /Type /ExtGState /ca ";
		appendNumber(body, level / 255.0);
		body += "/CA ";
		appendNumber(body, level / 255.0);
		body += ">>";
		objects_.define(found->second, body);
	}
	return "Ga" + std::to_string(level);
}

std::string PdfResources::image(std::pair<const void*, std::uint64_t> key, int width, int height,
                                std::span<const std::uint8_t> rgba) {
	if(const auto found = names_.find(key); found != names_.end()) return found->second;
	const auto count = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height));
	if(count == 0 || rgba.size() < count * 4) throw std::runtime_error("PDF image without pixels");
	if(count > pixelLimit_ - std::min(pixelLimit_, pixels_))
		throw std::runtime_error("PDF images larger than their limit (" + std::to_string(pixelLimit_) + " pixels)");
	pixels_ += count;
	std::vector<std::uint8_t> colors(count * 3);
	std::vector<std::uint8_t> alphas(count);
	bool opaque = true;
	for(std::size_t i = 0; i < count; ++i) {
		std::copy_n(rgba.begin() + static_cast<std::ptrdiff_t>(i * 4), 3,
		            colors.begin() + static_cast<std::ptrdiff_t>(i * 3));
		alphas[i] = rgba[i * 4 + 3];
		opaque = opaque && alphas[i] == 255;
	}
	const std::string size =
	    " /Width " + std::to_string(width) + " /Height " + std::to_string(height) + " /BitsPerComponent 8";
	const std::uint32_t id = objects_.reserve();
	std::string dictionary = "/Type /XObject /Subtype /Image /ColorSpace /DeviceRGB" + size;
	if(!opaque) {
		const std::uint32_t mask = objects_.reserve();
		objects_.defineStream(mask, "/Type /XObject /Subtype /Image /ColorSpace /DeviceGray" + size, alphas);
		dictionary += " /SMask " + std::to_string(mask) + " 0 R";
	}
	objects_.defineStream(id, dictionary, colors);
	const std::string name = "Im" + std::to_string(images_.size() + 1);
	images_.emplace(name, id);
	names_.emplace(key, name);
	return name;
}

void PdfResources::finish() {
	std::string body = "<< /ProcSet [ /PDF /Text /ImageB /ImageC ] /Font << ";
	fonts_.finish(body);
	body += ">> /XObject << ";
	for(const auto& [name, id] : images_) body += "/" + name + " " + std::to_string(id) + " 0 R ";
	body += ">> /ExtGState << ";
	for(const auto& [level, id] : alphas_) body += "/Ga" + std::to_string(level) + " " + std::to_string(id) + " 0 R ";
	objects_.define(id_, body + ">> >>");
}

} // namespace affresco::detail
