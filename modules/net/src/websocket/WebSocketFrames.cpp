/// @file
/// @brief WebSocket frames, the handshake's keys, SHA-1, base64 and the UTF-8 check.

#include "websocket/WebSocketFrames.hpp"

#include <bit>
#include <utility>

namespace affresco::net {

namespace {

/// The GUID RFC 6455 appends to the client's key.
constexpr std::string_view handshakeGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/// @brief A frame's header, once enough bytes are held.
struct FrameHeader {
	bool final = false;                  ///< FIN.
	std::uint8_t reserved = 0;           ///< RSV1-3.
	std::uint8_t opcode = 0;             ///< The opcode.
	bool masked = false;                 ///< MASK.
	std::uint64_t length = 0;            ///< The payload's length.
	std::array<std::uint8_t, 4> mask{};  ///< The masking key.
	std::size_t size = 0;                ///< The header's own length.
};

/// @brief Reads a frame's header.
/// @param bytes The bytes held.
/// @return It, or nullopt until enough bytes are held.
std::optional<FrameHeader> readHeader(std::string_view bytes) noexcept {
	if(bytes.size() < 2) return std::nullopt;
	const auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(bytes[i]); };
	FrameHeader header;
	header.final = (byte(0) & 0x80) != 0;
	header.reserved = static_cast<std::uint8_t>(byte(0) & 0x70);
	header.opcode = static_cast<std::uint8_t>(byte(0) & 0x0f);
	header.masked = (byte(1) & 0x80) != 0;
	header.length = byte(1) & 0x7f;
	std::size_t at = 2;
	const std::size_t extended = header.length == 126 ? 2 : header.length == 127 ? 8 : 0;
	if(bytes.size() < at + extended) return std::nullopt;
	if(extended > 0) header.length = 0;
	for(std::size_t i = 0; i < extended; ++i) header.length = (header.length << 8) | byte(at++);
	if(header.masked) {
		if(bytes.size() < at + 4) return std::nullopt;
		for(auto& key : header.mask) key = byte(at++);
	}
	header.size = at;
	return header;
}

/// @brief Whether an opcode is one RFC 6455 defines.
/// @param opcode The opcode.
bool isKnown(std::uint8_t opcode) noexcept { return opcode <= 0x2 || (opcode >= 0x8 && opcode <= 0xa); }

/// @brief Why a frame is refused from its header alone.
struct Refusal {
	std::uint16_t code; ///< The close code to send.
	const char* reason; ///< Why.
};

/// @brief Checks a frame's header before its payload is read.
/// @param header The header.
/// @param expectMasked Whether frames must be masked (from a client) or must not be (from a server).
/// @param room How many more bytes the message being read may take.
/// @return Why it is refused, or nullopt.
std::optional<Refusal> checkHeader(const FrameHeader& header, bool expectMasked, std::size_t room) noexcept {
	if(header.reserved != 0 || !isKnown(header.opcode)) return Refusal{1002, "a frame the protocol doesn't define"};
	if(header.masked != expectMasked) return Refusal{1002, expectMasked ? "an unmasked frame" : "a masked frame"};
	const bool control = header.opcode >= 0x8;
	if(control && (header.length > 125 || !header.final))
		return Refusal{1002, "a control frame too long or fragmented"};
	// Compared with the room left rather than by adding: a 64-bit length near its maximum would wrap the sum
	if(!control && header.length > room) return Refusal{1009, "a message too large"};
	return std::nullopt;
}

/// @brief Unmasks a payload in place.
/// @param payload The payload.
/// @param mask The masking key.
void unmask(std::string& payload, const std::array<std::uint8_t, 4>& mask) noexcept {
	for(std::size_t i = 0; i < payload.size(); ++i)
		payload[i] = static_cast<char>(static_cast<std::uint8_t>(payload[i]) ^ mask[i % 4]);
}

/// @brief SHA-1's function of a round.
/// @param round The round, 0 to 79.
/// @param b The state's second word.
/// @param c Its third.
/// @param d Its fourth.
constexpr std::uint32_t roundFunction(std::size_t round, std::uint32_t b, std::uint32_t c, std::uint32_t d) noexcept {
	if(round < 20) return (b & c) | (~b & d);
	if(round >= 40 && round < 60) return (b & c) | (b & d) | (c & d);
	return b ^ c ^ d;
}

/// @brief Mixes one 64-byte block into SHA-1's state.
/// @param state The state.
/// @param block The block.
void compress(std::array<std::uint32_t, 5>& state, std::string_view block) noexcept {
	static constexpr std::array<std::uint32_t, 4> constants{0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6};
	std::array<std::uint32_t, 80> words{};
	for(std::size_t i = 0; i < 16; ++i)
		for(std::size_t j = 0; j < 4; ++j) words[i] = (words[i] << 8) | static_cast<std::uint8_t>(block[i * 4 + j]);
	for(std::size_t i = 16; i < 80; ++i)
		words[i] = std::rotl(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
	auto [a, b, c, d, e] = state;
	for(std::size_t i = 0; i < 80; ++i) {
		const std::uint32_t next = std::rotl(a, 5) + roundFunction(i, b, c, d) + e + constants[i / 20] + words[i];
		e = d;
		d = c;
		c = std::rotl(b, 30);
		b = a;
		a = next;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

/// @brief The length of a UTF-8 sequence from its first byte, and the smallest code point it may encode.
/// @param lead The first byte.
/// @param minimum Receives the smallest code point.
/// @return 1 to 4, or 0 for a byte that can't start one.
int sequenceLength(unsigned char lead, char32_t& minimum) noexcept {
	if(lead < 0x80) return minimum = 0, 1;
	if((lead & 0xe0) == 0xc0) return minimum = 0x80, 2;
	if((lead & 0xf0) == 0xe0) return minimum = 0x800, 3;
	if((lead & 0xf8) == 0xf0) return minimum = 0x10000, 4;
	return 0;
}

} // namespace

std::string base64(std::string_view bytes) {
	static constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string encoded;
	encoded.reserve((bytes.size() + 2) / 3 * 4);
	for(std::size_t i = 0; i < bytes.size(); i += 3) {
		std::uint32_t group = static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[i])) << 16;
		if(i + 1 < bytes.size()) group |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[i + 1])) << 8;
		if(i + 2 < bytes.size()) group |= static_cast<std::uint8_t>(bytes[i + 2]);
		encoded += alphabet[(group >> 18) & 0x3f];
		encoded += alphabet[(group >> 12) & 0x3f];
		encoded += i + 1 < bytes.size() ? alphabet[(group >> 6) & 0x3f] : '=';
		encoded += i + 2 < bytes.size() ? alphabet[group & 0x3f] : '=';
	}
	return encoded;
}

std::array<std::uint8_t, 20> sha1(std::string_view bytes) {
	std::array<std::uint32_t, 5> state{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
	std::string message(bytes);
	const std::uint64_t bits = static_cast<std::uint64_t>(bytes.size()) * 8;
	message += static_cast<char>(0x80);
	while(message.size() % 64 != 56) message += '\0';
	for(int shift = 56; shift >= 0; shift -= 8) message += static_cast<char>((bits >> shift) & 0xff);
	for(std::size_t block = 0; block < message.size(); block += 64)
		compress(state, std::string_view(message).substr(block, 64));
	std::array<std::uint8_t, 20> digest{};
	for(std::size_t i = 0; i < 20; ++i) digest[i] = static_cast<std::uint8_t>(state[i / 4] >> (24 - 8 * (i % 4)));
	return digest;
}

std::string acceptKey(std::string_view key) {
	const auto digest = sha1(std::string(key) + std::string(handshakeGuid));
	return base64(std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size()));
}

std::string encodeFrame(Opcode opcode, std::string_view payload, std::optional<std::array<std::uint8_t, 4>> mask) {
	std::string frame;
	frame.reserve(payload.size() + 14);
	frame += static_cast<char>(0x80 | static_cast<std::uint8_t>(opcode));
	const std::uint8_t maskBit = mask ? 0x80 : 0;
	if(payload.size() < 126) frame += static_cast<char>(maskBit | payload.size());
	else if(payload.size() <= 0xffff) {
		frame += static_cast<char>(maskBit | 126);
		frame += static_cast<char>((payload.size() >> 8) & 0xff);
		frame += static_cast<char>(payload.size() & 0xff);
	} else {
		frame += static_cast<char>(maskBit | 127);
		for(int shift = 56; shift >= 0; shift -= 8)
			frame += static_cast<char>((static_cast<std::uint64_t>(payload.size()) >> shift) & 0xff);
	}
	if(!mask) return frame.append(payload);
	for(const auto key : *mask) frame += static_cast<char>(key);
	for(std::size_t i = 0; i < payload.size(); ++i)
		frame += static_cast<char>(static_cast<std::uint8_t>(payload[i]) ^ (*mask)[i % 4]);
	return frame;
}

std::string closePayload(std::uint16_t code, std::string_view reason) {
	std::string payload{static_cast<char>(code >> 8), static_cast<char>(code & 0xff)};
	payload += reason.substr(0, 123);
	while(!isUtf8(payload.substr(2))) payload.pop_back(); // a cut mustn't split a character
	return payload;
}

FrameEvent FrameReader::fail(std::uint16_t code, std::string reason) {
	failed_ = true;
	buffer_.clear();
	message_.clear();
	return {FrameEvent::Kind::Error, std::move(reason), code};
}

std::optional<FrameEvent> FrameReader::next() {
	while(!failed_) {
		const auto header = readHeader(buffer_);
		if(!header) return std::nullopt;
		// message_ is within the limit, so the room left doesn't wrap
		if(const auto refusal = checkHeader(*header, expectMasked_, maxMessageBytes_ - message_.size()))
			return fail(refusal->code, refusal->reason);
		const auto length = static_cast<std::size_t>(header->length);
		if(buffer_.size() < header->size + length) return std::nullopt;
		std::string payload = buffer_.substr(header->size, length);
		buffer_.erase(0, header->size + length);
		if(header->masked) unmask(payload, header->mask);
		if(auto event = frame(static_cast<Opcode>(header->opcode), header->final, std::move(payload))) return event;
	}
	return std::nullopt;
}

std::optional<FrameEvent> FrameReader::frame(Opcode opcode, bool final, std::string payload) {
	switch(opcode) {
	case Opcode::Ping: return FrameEvent{FrameEvent::Kind::Ping, std::move(payload), 0};
	case Opcode::Pong: return FrameEvent{FrameEvent::Kind::Pong, {}, 0};
	case Opcode::Close:
		return closeFrame(payload);
	case Opcode::Binary: return fail(1003, "binary messages aren't accepted");
	case Opcode::Text:
		if(inMessage_) return fail(1002, "a new message inside a fragmented one");
		message_ = std::move(payload);
		inMessage_ = !final;
		break;
	case Opcode::Continuation:
		if(!inMessage_) return fail(1002, "a continuation outside a message");
		message_ += payload;
		inMessage_ = !final;
		break;
	}
	if(inMessage_) return std::nullopt;
	if(!isUtf8(message_)) return fail(1007, "a text message that isn't UTF-8");
	return FrameEvent{FrameEvent::Kind::Message, std::exchange(message_, {}), 0};
}

FrameEvent FrameReader::closeFrame(std::string_view payload) {
	if(payload.size() == 1) return fail(1002, "a close frame of one byte");
	const std::uint16_t code = payload.size() >= 2
	                               ? static_cast<std::uint16_t>((static_cast<std::uint8_t>(payload[0]) << 8) |
	                                                            static_cast<std::uint8_t>(payload[1]))
	                               : noCloseCode;
	std::string reason(payload.size() > 2 ? payload.substr(2) : std::string_view{});
	if(!isUtf8(reason)) return fail(1007, "a close reason that isn't UTF-8");
	failed_ = true; // nothing is read after a close
	return FrameEvent{FrameEvent::Kind::Close, std::move(reason), code};
}

bool isUtf8(std::string_view text) noexcept {
	for(std::size_t i = 0; i < text.size();) {
		char32_t minimum = 0;
		const auto lead = static_cast<unsigned char>(text[i]);
		const int length = sequenceLength(lead, minimum);
		if(length == 0 || i + static_cast<std::size_t>(length) > text.size()) return false;
		char32_t code = length == 1 ? lead : lead & (0x7fu >> length);
		for(int j = 1; j < length; ++j) {
			const auto next = static_cast<unsigned char>(text[i + static_cast<std::size_t>(j)]);
			if((next & 0xc0) != 0x80) return false;
			code = (code << 6) | (next & 0x3fu);
		}
		if(code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
		i += static_cast<std::size_t>(length);
	}
	return true;
}

} // namespace affresco::net
