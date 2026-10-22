/// @file
/// @brief Server-sent events and line streams.

#include "http/Streams.hpp"

#include <algorithm>
#include <utility>

namespace affresco::net {

EventStreamParser::EventStreamParser(std::size_t maxEventBytes, EventSink sink)
    : maxEventBytes_(maxEventBytes), sink_(std::move(sink)) {}

bool EventStreamParser::feed(std::string_view chunk) {
	return std::ranges::all_of(chunk, [this](char c) {
		if(c == '\n' && afterCarriageReturn_) {
			afterCarriageReturn_ = false;
			return true;
		}
		afterCarriageReturn_ = c == '\r';
		if(c == '\n' || c == '\r') {
			if(!line(pending_)) return false;
			pending_.clear();
			return true;
		}
		if(pending_.size() >= maxEventBytes_) return false;
		pending_ += c;
		return true;
	});
}

bool EventStreamParser::line(std::string_view text) {
	if(text.empty()) {
		if(hasData_ && sink_) sink_(std::exchange(event_, {}));
		event_ = {};
		hasData_ = false;
		return true;
	}
	if(text.front() == ':') return true;
	const auto colon = text.find(':');
	const auto field = text.substr(0, colon);
	std::string_view value = colon == std::string_view::npos ? std::string_view{} : text.substr(colon + 1);
	if(value.starts_with(' ')) value.remove_prefix(1);
	this->field(field, value);
	return event_.data.size() + event_.type.size() + event_.id.size() <= maxEventBytes_;
}

void EventStreamParser::field(std::string_view name, std::string_view value) {
	if(name == "data") {
		if(hasData_) event_.data += '\n';
		event_.data += value;
		hasData_ = true;
	} else if(name == "event") event_.type = value.empty() ? "message" : std::string(value);
	else if(name == "id" && !value.contains('\0')) event_.id = value;
}

LineStreamParser::LineStreamParser(std::size_t maxLineBytes, EventSink sink)
    : maxLineBytes_(maxLineBytes), sink_(std::move(sink)) {}

bool LineStreamParser::feed(std::string_view chunk) {
	return std::ranges::all_of(chunk, [this](char c) {
		if(c != '\n') {
			if(pending_.size() >= maxLineBytes_) return false;
			pending_ += c;
			return true;
		}
		if(pending_.ends_with('\r')) pending_.pop_back();
		if(!pending_.empty() && sink_) sink_({"line", std::exchange(pending_, {}), {}});
		pending_.clear();
		return true;
	});
}

void LineStreamParser::finish() {
	if(pending_.ends_with('\r')) pending_.pop_back();
	if(!pending_.empty() && sink_) sink_({"line", std::exchange(pending_, {}), {}});
}

} // namespace affresco::net
