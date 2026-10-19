/// @file
/// @brief Counts every operator new and the bytes still allocated, so each workload reports its allocations and the
/// memory it keeps. Replacing the global operator new is a process-wide hook by nature: these two counters are the
/// benchmark's only global mutable state, and they are atomic because driver threads allocate too.
///
/// Each block carries its size in a header, so delete knows what it frees. The aligned forms aren't replaced: their
/// default versions allocate and free on their own, outside these counters.

#include "Bench.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace {

std::atomic<std::uint64_t> allocations{0}; ///< Calls to operator new.
std::atomic<std::int64_t> bytesInUse{0};   ///< Bytes allocated and not deleted.

/// @brief Room for the size in front of each block, keeping the block aligned as malloc's are.
constexpr std::size_t header = alignof(std::max_align_t);

/// @brief Allocates a counted block.
/// @param size The bytes asked for.
/// @return The block, or nullptr if there is no memory.
void* allocate(std::size_t size) noexcept {
	if(size > std::numeric_limits<std::size_t>::max() - header) return nullptr; // the header would wrap the size
	auto* block = static_cast<unsigned char*>(std::malloc(size + header));
	if(!block) return nullptr;
	*reinterpret_cast<std::size_t*>(block) = size;
	allocations.fetch_add(1, std::memory_order_relaxed);
	bytesInUse.fetch_add(static_cast<std::int64_t>(size), std::memory_order_relaxed);
	return block + header;
}

/// @brief Frees a counted block.
/// @param pointer The block, or nullptr.
void release(void* pointer) noexcept {
	if(!pointer) return;
	unsigned char* block = static_cast<unsigned char*>(pointer) - header;
	bytesInUse.fetch_sub(static_cast<std::int64_t>(*reinterpret_cast<std::size_t*>(block)), std::memory_order_relaxed);
	std::free(block);
}

/// @brief Allocates a counted block, or throws as operator new must.
/// @param size The bytes asked for.
/// @return The block.
/// @throws std::bad_alloc If there is no memory.
void* allocateOrThrow(std::size_t size) {
	if(void* block = allocate(size)) return block;
	throw std::bad_alloc();
}

} // namespace

std::uint64_t affresco::bench::allocationCount() noexcept { return allocations.load(std::memory_order_relaxed); }

std::int64_t affresco::bench::liveBytes() noexcept { return bytesInUse.load(std::memory_order_relaxed); }

/// @brief Counted operator new.
/// @param size The bytes asked for.
/// @return The block.
void* operator new(std::size_t size) { return allocateOrThrow(size); }
/// @brief Counted operator new[].
/// @param size The bytes asked for.
/// @return The block.
void* operator new[](std::size_t size) { return allocateOrThrow(size); }
/// @brief Counted operator new that returns nullptr instead of throwing.
/// @param size The bytes asked for.
/// @return The block, or nullptr.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
/// @brief Counted operator new[] that returns nullptr instead of throwing.
/// @param size The bytes asked for.
/// @return The block, or nullptr.
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }

/// @brief Counted operator delete.
/// @param pointer The block.
void operator delete(void* pointer) noexcept { release(pointer); }
/// @brief Counted operator delete[].
/// @param pointer The block.
void operator delete[](void* pointer) noexcept { release(pointer); }
/// @brief Counted sized operator delete.
/// @param pointer The block.
void operator delete(void* pointer, std::size_t) noexcept { release(pointer); }
/// @brief Counted sized operator delete[].
/// @param pointer The block.
void operator delete[](void* pointer, std::size_t) noexcept { release(pointer); }
/// @brief Counted operator delete, for a failed nothrow new.
/// @param pointer The block.
void operator delete(void* pointer, const std::nothrow_t&) noexcept { release(pointer); }
/// @brief Counted operator delete[], for a failed nothrow new[].
/// @param pointer The block.
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { release(pointer); }
