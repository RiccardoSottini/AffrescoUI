/// @file
/// @brief Random bytes from the system's cryptographic generator (Random.hpp).

#include "Random.hpp"

#include <algorithm>
#include <cerrno>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <cstdlib>
#elif defined(__EMSCRIPTEN__) // NOLINT(readability-use-concise-preprocessor-directives): #elifdef is C++23
#include <unistd.h>
#else
#include <sys/random.h>
#endif

namespace affresco::net {

void fillRandom(std::span<std::byte> into) {
#ifdef _WIN32
	if(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(into.data()), static_cast<ULONG>(into.size()),
	                   BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
		throw std::runtime_error("the system's random generator failed");
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
	arc4random_buf(into.data(), into.size());
#elif defined(__EMSCRIPTEN__) // the browser's crypto.getRandomValues, at most 256 bytes a call
	for(std::size_t done = 0; done < into.size(); done += 256)
		if(getentropy(into.data() + done, std::min<std::size_t>(256, into.size() - done)) != 0)
			throw std::runtime_error("the system's random generator failed");
#else
	for(std::size_t done = 0; done < into.size();) {
		const ssize_t got = getrandom(into.data() + done, into.size() - done, 0);
		if(got < 0 && errno == EINTR) continue; // a signal arrived before any byte: ask again
		if(got < 0) throw std::runtime_error("the system's random generator failed");
		done += static_cast<std::size_t>(got);
	}
#endif
}

} // namespace affresco::net
