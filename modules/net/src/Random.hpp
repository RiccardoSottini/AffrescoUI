#pragma once

/// @file
/// @brief Random bytes from the system's cryptographic generator: what PKCE verifiers, sign-in states and WebSocket
/// masking keys need (RFC 7636 section 7.1, RFC 6455 section 10.3), where a seeded engine would be predictable.

#include <cstddef>
#include <span>

namespace affresco::net {

/// @brief Fills bytes from the system's cryptographic generator.
/// @param into The bytes.
/// @throws std::runtime_error If the system gives none (never on a working system).
void fillRandom(std::span<std::byte> into);

} // namespace affresco::net
