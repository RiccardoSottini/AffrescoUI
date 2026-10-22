#pragma once

/// @file
/// @brief The WebSocket protocol (RFC 6455) without the transport: the handshake's keys, encoding frames, and a
/// reader that turns received bytes into messages, bounded and strict.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::net {

/// @brief The opcodes of RFC 6455.
enum class Opcode : std::uint8_t {
	Continuation = 0x0, ///< The next fragment of a message.
	Text = 0x1,         ///< A text message (or its first fragment).
	Binary = 0x2,       ///< A binary message: refused here (1003), as the live protocol is text.
	Close = 0x8,        ///< The end.
	Ping = 0x9,         ///< Answered with a pong.
	Pong = 0xa,         ///< The answer to a ping.
};

/// @brief Encodes bytes in base64 (with padding).
/// @param bytes The bytes.
[[nodiscard]] std::string base64(std::string_view bytes);

/// @brief The SHA-1 digest of bytes, used only for the handshake's Sec-WebSocket-Accept (RFC 6455 prescribes it; it
/// guards nothing).
/// @param bytes The bytes.
/// @return The 20 bytes of the digest.
[[nodiscard]] std::array<std::uint8_t, 20> sha1(std::string_view bytes);

/// @brief The Sec-WebSocket-Accept answer to a Sec-WebSocket-Key.
/// @param key The client's key (base64 of 16 random bytes).
/// @return base64(SHA-1(key + the protocol's GUID)).
[[nodiscard]] std::string acceptKey(std::string_view key);

/// @brief Encodes one frame, whole (FIN set).
/// @param opcode Its opcode.
/// @param payload Its payload (at most 125 bytes for a control frame).
/// @param mask The masking key, for a frame a client sends; nullopt for one a server sends.
/// @return The bytes to send.
[[nodiscard]] std::string encodeFrame(Opcode opcode, std::string_view payload,
                                      std::optional<std::array<std::uint8_t, 4>> mask = std::nullopt);

/// @brief The close frame's payload: a status code, then a reason (cut to fit 125 bytes).
/// @param code E.g. 1000 (normal), 1008 (policy), 1009 (too large).
/// @param reason Why, for people.
[[nodiscard]] std::string closePayload(std::uint16_t code, std::string_view reason);

/// @brief What a FrameReader found.
struct FrameEvent {
	/// @brief Its kind.
	enum class Kind {
		Message, ///< A whole text message, in `data`.
		Ping,    ///< A ping, its payload in `data`: answer with a pong carrying it.
		Pong,    ///< A pong.
		Close,   ///< The peer closes: `code` and the reason in `data`.
		Error,   ///< The peer broke the protocol, or went over a limit: close with `code`, the reason in `data`.
	};
	Kind kind = Kind::Message; ///< See Kind.
	std::string data;          ///< See Kind.
	std::uint16_t code = 0;    ///< For Close and Error.
};

/// @brief Reads frames from the bytes received, one event at a time.
///
/// Checks what RFC 6455 requires: masked frames from a client and unmasked ones from a server, no reserved bits,
/// known opcodes, control frames of at most 125 bytes and not fragmented, continuations only within a message, text
/// that is UTF-8. A message longer than the limit is an error (1009), found from the frame's header, before its
/// payload is received. After an error it reads nothing more.
class FrameReader {
public:
	/// @brief Starts empty.
	/// @param expectMasked true on the server side (frames from a client are masked), false on the client side.
	/// @param messageLimit The longest message, all its fragments together.
	FrameReader(bool expectMasked, std::size_t messageLimit) noexcept
	    : expectMasked_(expectMasked), maxMessageBytes_(messageLimit) {}

	/// @brief Adds received bytes.
	/// @param bytes The bytes.
	void feed(std::string_view bytes) { buffer_.append(bytes); }
	/// @brief The next event, if the bytes received hold one.
	/// @return It, or nullopt until more bytes arrive.
	[[nodiscard]] std::optional<FrameEvent> next();
	/// @brief How many bytes are held, not yet read into an event.
	[[nodiscard]] std::size_t buffered() const noexcept { return buffer_.size(); }

private:
	/// @brief Notes an error and stops reading.
	/// @param code The close code to send.
	/// @param reason Why.
	/// @return The error event.
	FrameEvent fail(std::uint16_t code, std::string reason);
	/// @brief Handles one whole frame's payload.
	/// @param opcode Its opcode.
	/// @param final Whether FIN was set.
	/// @param payload Its payload, unmasked.
	/// @return An event, or nullopt for a fragment that doesn't end its message.
	std::optional<FrameEvent> frame(Opcode opcode, bool final, std::string payload);
	/// @brief Handles a close frame: the peer's code (1005 if none) and its reason, after which nothing is read.
	/// @param payload Its payload, unmasked.
	/// @return The close event, or an error for a payload of one byte or a reason that isn't UTF-8.
	FrameEvent closeFrame(std::string_view payload);

	bool expectMasked_;          ///< See the constructor.
	std::size_t maxMessageBytes_; ///< See the constructor.
	std::string buffer_;         ///< Bytes received and not yet read.
	std::string message_;        ///< The fragments so far of a message.
	bool inMessage_ = false;     ///< Whether a message is being read in fragments.
	bool failed_ = false;        ///< An error was found: nothing more is read.
};

/// @brief Whether bytes are well-formed UTF-8 (no overlong forms, surrogates or code points past U+10FFFF).
/// @param text The bytes.
[[nodiscard]] bool isUtf8(std::string_view text) noexcept;

} // namespace affresco::net
