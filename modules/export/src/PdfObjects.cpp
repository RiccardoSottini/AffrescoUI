/// @file
/// @brief detail::PdfObjects: objects written in order, offsets recorded, the cross-reference table at the end.

#include "PdfObjects.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace affresco::detail {

PdfObjects::PdfObjects(std::size_t limit) : limit_(limit) {
	// The binary comment tells tools the file holds 8-bit data (ISO 32000-1, 7.5.2)
	append("%PDF-1.7\n%\xE2\xE3\xCF\xD3\n");
}

std::uint32_t PdfObjects::reserve() {
	offsets_.push_back(0);
	return static_cast<std::uint32_t>(offsets_.size());
}

void PdfObjects::begin(std::uint32_t id) {
	if(id == 0 || id > offsets_.size() || offsets_[id - 1] != 0) throw std::runtime_error("PDF object defined twice");
	offsets_[id - 1] = out_.size();
	append(std::to_string(id) + " 0 obj\n");
}

void PdfObjects::define(std::uint32_t id, std::string_view body) {
	begin(id);
	append(body);
	append("\nendobj\n");
}

void PdfObjects::defineStream(std::uint32_t id, std::string_view dictionary, std::span<const std::uint8_t> data) {
	defineStream(id, dictionary, std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

void PdfObjects::defineStream(std::uint32_t id, std::string_view dictionary, std::string_view data) {
	begin(id);
	append("<<");
	append(dictionary);
	append(" /Length " + std::to_string(data.size()) + " >>\nstream\n");
	append(data);
	append("\nendstream\nendobj\n");
}

void PdfObjects::append(std::string_view text) {
	if(text.size() > limit_ - std::min(limit_, out_.size()))
		throw std::runtime_error("PDF larger than its limit (" + std::to_string(limit_) + " bytes)");
	out_.insert(out_.end(), text.begin(), text.end());
}

std::vector<std::uint8_t> PdfObjects::finish(std::uint32_t root, std::uint32_t info) {
	const std::size_t table = out_.size();
	append("xref\n0 " + std::to_string(offsets_.size() + 1) + "\n0000000000 65535 f\r\n");
	for(const std::size_t offset : offsets_) {
		if(offset == 0) throw std::runtime_error("PDF object reserved but not defined");
		char entry[21];
		std::snprintf(entry, sizeof entry, "%010zu 00000 n\r\n", offset); // 20 bytes each, end of line included
		append(entry);
	}
	append("trailer\n<< /Size " + std::to_string(offsets_.size() + 1) + " /Root " + std::to_string(root) +
	       " 0 R /Info " + std::to_string(info) + " 0 R >>\nstartxref\n" + std::to_string(table) + "\n%%EOF\n");
	return std::move(out_);
}

} // namespace affresco::detail
