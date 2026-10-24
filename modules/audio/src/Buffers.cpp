/// @file
/// @brief detail::FrameRing and detail::ByteQueue.

#include "Buffers.hpp"

#include <algorithm>

namespace affresco::audio::detail {

FrameRing::FrameRing(std::size_t capacity_, std::uint32_t channels_)
    : capacity(std::max<std::size_t>(capacity_, 1)), channels(channels_), samples(capacity * channels) {}

std::size_t FrameRing::write(const float* frames, std::size_t count) {
	const std::scoped_lock lock(writers);
	const std::uint64_t head = written.load(std::memory_order_relaxed);
	const auto held = static_cast<std::size_t>(head - consumed.load(std::memory_order_acquire));
	const std::size_t taken = std::min(count, capacity - held);
	const auto start = static_cast<std::size_t>(head % capacity);
	const std::size_t first = std::min(taken, capacity - start);
	std::copy_n(frames, first * channels, samples.begin() + static_cast<std::ptrdiff_t>(start * channels));
	std::copy_n(frames + (first * channels), (taken - first) * channels, samples.begin());
	written.store(head + taken, std::memory_order_release);
	return taken;
}

std::size_t FrameRing::read(float* out, std::size_t count) noexcept {
	const std::uint64_t tail = consumed.load(std::memory_order_relaxed);
	const auto held = static_cast<std::size_t>(written.load(std::memory_order_acquire) - tail);
	const std::size_t taken = std::min(count, held);
	const auto start = static_cast<std::size_t>(tail % capacity);
	const std::size_t first = std::min(taken, capacity - start);
	std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(start * channels), first * channels, out);
	std::copy_n(samples.begin(), (taken - first) * channels, out + (first * channels));
	consumed.store(tail + taken, std::memory_order_release);
	return taken;
}

std::size_t FrameRing::available() const noexcept {
	return static_cast<std::size_t>(written.load(std::memory_order_acquire) -
	                                consumed.load(std::memory_order_acquire));
}

std::size_t FrameRing::space() const noexcept { return capacity - available(); }

bool ByteQueue::push(std::span<const std::uint8_t> data) {
	{
		const std::scoped_lock lock(mutex);
		if(finished || cancelled) return false;
		const bool room = data.size() <= capacity - std::min(bytes.size(), capacity);
		if(!keeping && reading > base && (!room || reading - base >= 65536)) { // drop what was read, now and then
			const auto read = static_cast<std::ptrdiff_t>(reading - base);
			bytes.erase(bytes.begin(), bytes.begin() + read);
			base = reading;
		}
		if(data.size() > capacity - std::min(bytes.size(), capacity)) return false;
		bytes.insert(bytes.end(), data.begin(), data.end());
	}
	changed.notify_all();
	return true;
}

void ByteQueue::finish() noexcept {
	{
		const std::scoped_lock lock(mutex);
		finished = true;
	}
	changed.notify_all();
}

void ByteQueue::cancel() noexcept {
	{
		const std::scoped_lock lock(mutex);
		cancelled = true;
	}
	changed.notify_all();
}

std::size_t ByteQueue::read(std::uint8_t* out, std::size_t count) {
	std::unique_lock lock(mutex);
	changed.wait(lock, [&] {
		return cancelled || finished || base + bytes.size() - reading >= count || bytes.size() >= capacity;
	});
	if(cancelled) return 0;
	const auto start = static_cast<std::size_t>(reading - base);
	const std::size_t taken = std::min(count, bytes.size() - start);
	std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(start), taken, out);
	reading += taken;
	return taken;
}

bool ByteQueue::seek(std::uint64_t offset) {
	std::unique_lock lock(mutex);
	if(offset < base) return false;
	changed.wait(lock, [&] {
		return cancelled || finished || base + bytes.size() >= offset || bytes.size() >= capacity;
	});
	if(cancelled || offset > base + bytes.size()) return false;
	reading = offset;
	return true;
}

std::uint64_t ByteQueue::position() const {
	const std::scoped_lock lock(mutex);
	return reading;
}

std::uint64_t ByteQueue::knownLength() const {
	const std::scoped_lock lock(mutex);
	return finished && base == 0 ? bytes.size() : 0;
}

void ByteQueue::stopKeeping() {
	const std::scoped_lock lock(mutex);
	keeping = false;
}

std::size_t ByteQueue::buffered() const {
	const std::scoped_lock lock(mutex);
	return static_cast<std::size_t>(base + bytes.size() - reading);
}

} // namespace affresco::audio::detail
