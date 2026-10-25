/// @file
/// @brief The QR code expression function, qrCode.

#include "ExpressionFunctions.hpp"
#include "QrCode.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace affresco::format::detail {
namespace {

/// The most commands and bytes of path data a path node draws (render's VectorPath::Limits, which the format module
/// doesn't see): a larger code fails here, clearly, rather than not drawing.
constexpr std::size_t mostCommands = 16384;
/// The longest path data a path node draws.
constexpr std::size_t mostBytes = std::size_t{64} << 10;
/// The light modules the standard asks around a code, which its viewBox frames.
constexpr int quietZone = 4;

/// @brief `qrCode(text, level?)`: a QR code of a text, as the path data of its dark modules.
/// @param call [text, level "L", "M" (the default), "Q" or "H"].
/// @return {size: modules on a side, version, d: the path data, viewBox: the code and its quiet zone}.
Value qrCode(const Call& call) {
	QrLevel level = QrLevel::Medium;
	if(call.size() > 1) {
		static constexpr std::array<std::string_view, 4> names{"L", "M", "Q", "H"};
		const std::string_view name = call[1].isText() ? call[1].text() : std::string_view{};
		const auto* const found = std::ranges::find(names, name);
		if(found == names.end()) call.fail(R"(expected the level "L", "M", "Q" or "H")");
		level = static_cast<QrLevel>(found - names.begin());
	}
	const std::string text = call[0].toText();
	const std::optional<QrCode> code = encodeQr(text, level);
	if(!code) call.fail(std::format("the text is too long for a QR code ({} bytes)", text.size()));
	std::size_t commands = 0;
	std::string d = qrPathData(*code, commands);
	if(commands > mostCommands || d.size() > mostBytes)
		call.fail(std::format("the QR code is too large to draw (version {}): shorten the text", code->version));
	const int side = code->size + 2 * quietZone;
	std::vector<Value::Field> fields;
	fields.push_back({"size", Value::fromInteger(code->size)});
	fields.push_back({"version", Value::fromInteger(code->version)});
	fields.push_back({"d", Value::fromText(std::move(d))});
	fields.push_back({"viewBox", Value::fromText(std::format("{} {} {} {}", -quietZone, -quietZone, side, side))});
	return Value::fromObject(std::move(fields));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "qrCode",
	 .arguments = "text, level?",
	 .about = R"(A QR code of the text, level "L", "M" (default), "Q" or "H": {size, version, d, viewBox} for a path)",
	 .fewest = 1,
	 .most = 2,
	 .apply = qrCode},
});

} // namespace

std::span<const Function> qrFunctions() { return table; }

} // namespace affresco::format::detail
