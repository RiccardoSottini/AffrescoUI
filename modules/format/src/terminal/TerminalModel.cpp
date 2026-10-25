/// @file
/// @brief TerminalModel on libvterm: the callbacks that keep the scrollback, the title and the cursor's visibility,
/// and the keys it encodes.

#include "TerminalModel.hpp"

#include <vterm.h>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace affresco::format {

namespace {

/// @brief The most bytes of a title it keeps.
constexpr std::size_t mostTitleBytes = 256;
/// @brief The value libvterm gives the column a wide character covers.
constexpr std::uint32_t wideContinuation = 0xFFFFFFFFu;

/// @brief Clamps a number of rows to what a terminal may have.
/// @param cells The number.
/// @return It, within 1..mostTerminalCells.
[[nodiscard]] int clampCells(int cells) noexcept { return std::clamp(cells, 1, mostTerminalCells); }

/// @brief Clamps a number of columns to what a terminal may have: two at least, which a wide character covers (in one,
/// libvterm writes its second column outside the row).
/// @param cells The number.
/// @return It, within 2..mostTerminalCells.
[[nodiscard]] int clampColumns(int cells) noexcept { return std::clamp(cells, 2, mostTerminalCells); }

/// @brief A colour as 0xRRGGBB.
/// @param color An RGB colour.
/// @return Its value.
[[nodiscard]] std::uint32_t rgbOf(const VTermColor& color) noexcept {
	return (std::uint32_t{color.rgb.red} << 16) | (std::uint32_t{color.rgb.green} << 8) | color.rgb.blue;
}

/// @brief Makes a libvterm colour.
/// @param rgb 0xRRGGBB.
/// @param defaultFlag VTERM_COLOR_DEFAULT_FG or _BG if it is the default one, else 0.
/// @return The colour.
[[nodiscard]] VTermColor vtermColor(std::uint32_t rgb, int defaultFlag) noexcept {
	VTermColor color{};
	vterm_color_rgb(&color, static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
	                static_cast<std::uint8_t>(rgb));
	color.type = static_cast<std::uint8_t>(color.type | defaultFlag);
	return color;
}

/// @brief Turns a libvterm cell into a TerminalCell.
/// @param screen The screen, which knows the palette.
/// @param source The cell.
/// @return The cell.
[[nodiscard]] TerminalCell cellFrom(const VTermScreen* screen, const VTermScreenCell& source) {
	TerminalCell cell;
	if(source.chars[0] == wideContinuation) {
		cell.width = 0;
		return cell;
	}
	cell.character = static_cast<char32_t>(source.chars[0]);
	// A C1 control written in UTF-8 takes a column (cmake/AffrescoUITerminal.cmake), shown as the replacement character
	if(cell.character >= 0x80 && cell.character < 0xa0) cell.character = 0xfffd;
	cell.width = source.width == 2 ? 2 : 1;
	cell.bold = source.attrs.bold != 0;
	cell.underline = source.attrs.underline != 0;
	cell.italic = source.attrs.italic != 0;
	cell.reverse = source.attrs.reverse != 0;
	cell.defaultForeground = VTERM_COLOR_IS_DEFAULT_FG(&source.fg);
	cell.defaultBackground = VTERM_COLOR_IS_DEFAULT_BG(&source.bg);
	VTermColor foreground = source.fg; // converting drops the default flags, read above
	VTermColor background = source.bg;
	vterm_screen_convert_color_to_rgb(screen, &foreground);
	vterm_screen_convert_color_to_rgb(screen, &background);
	cell.foreground = rgbOf(foreground);
	cell.background = rgbOf(background);
	return cell;
}

/// @brief Turns a TerminalCell back into a libvterm cell, for a line that comes back from the scrollback.
/// @param cell The cell.
/// @return The libvterm cell (a wide character's second column is a blank of width 1, which libvterm skips).
[[nodiscard]] VTermScreenCell cellTo(const TerminalCell& cell) noexcept {
	VTermScreenCell out{};
	out.chars[0] = cell.character;
	out.width = cell.width == 2 ? 2 : 1;
	out.attrs.bold = cell.bold ? 1u : 0u;
	out.attrs.underline = cell.underline ? 1u : 0u;
	out.attrs.italic = cell.italic ? 1u : 0u;
	out.attrs.reverse = cell.reverse ? 1u : 0u;
	out.fg = vtermColor(cell.foreground, cell.defaultForeground ? VTERM_COLOR_DEFAULT_FG : 0);
	out.bg = vtermColor(cell.background, cell.defaultBackground ? VTERM_COLOR_DEFAULT_BG : 0);
	return out;
}

/// @brief The libvterm modifiers for the ones held.
/// @param mods The modifiers.
/// @return Them as VTermModifier.
[[nodiscard]] VTermModifier modifiersOf(Mods mods) noexcept {
	int bits = VTERM_MOD_NONE;
	if(mods.shift()) bits |= VTERM_MOD_SHIFT;
	if(mods.alt()) bits |= VTERM_MOD_ALT;
	if(mods.control()) bits |= VTERM_MOD_CTRL;
	return static_cast<VTermModifier>(bits);
}

/// @brief The libvterm key for a key that isn't a character.
/// @param key The key.
/// @return It, or std::nullopt for any other key.
[[nodiscard]] std::optional<VTermKey> vtermKey(Key key) noexcept {
	static constexpr std::array<std::pair<Key, VTermKey>, 14> keys{{
	    {Key::Enter, VTERM_KEY_ENTER},
	    {Key::KeypadEnter, VTERM_KEY_ENTER},
	    {Key::Tab, VTERM_KEY_TAB},
	    {Key::Backspace, VTERM_KEY_BACKSPACE},
	    {Key::Escape, VTERM_KEY_ESCAPE},
	    {Key::Up, VTERM_KEY_UP},
	    {Key::Down, VTERM_KEY_DOWN},
	    {Key::Left, VTERM_KEY_LEFT},
	    {Key::Right, VTERM_KEY_RIGHT},
	    {Key::Insert, VTERM_KEY_INS},
	    {Key::Delete, VTERM_KEY_DEL},
	    {Key::Home, VTERM_KEY_HOME},
	    {Key::End, VTERM_KEY_END},
	    {Key::PageUp, VTERM_KEY_PAGEUP},
	}};
	if(key == Key::PageDown) return VTERM_KEY_PAGEDOWN;
	if(key >= Key::F1 && key <= Key::F12)
		return static_cast<VTermKey>(VTERM_KEY_FUNCTION_0 + 1 + (static_cast<int>(key) - static_cast<int>(Key::F1)));
	for(const auto& [from, to] : keys)
		if(from == key) return to;
	return std::nullopt;
}

} // namespace

/// @brief The libvterm callbacks: static functions whose user data is the TerminalModel.
struct TerminalCallbacks {
	/// @brief Keeps a line scrolled off the top, trailing blanks trimmed, dropping the oldest past the limit.
	/// @param columns The cells in the line.
	/// @param cells The cells.
	/// @param user The model.
	/// @return 1.
	static int pushLine(int columns, const VTermScreenCell* cells, void* user) {
		auto& model = *static_cast<TerminalModel*>(user);
		if(model.scrollbackLimit_ == 0) return 1;
		std::vector<TerminalCell> line;
		line.reserve(static_cast<std::size_t>(columns));
		for(int column = 0; column < columns; ++column)
			line.push_back(cellFrom(model.screen_, cells[column])); // NOLINT(*-pointer-arithmetic): libvterm's array
		while(!line.empty() && line.back() == TerminalCell{}) line.pop_back();
		line.shrink_to_fit();
		model.scrollback_.push_back(std::move(line));
		while(model.scrollback_.size() > model.scrollbackLimit_) model.scrollback_.pop_front();
		return 1;
	}

	/// @brief Gives back the newest scrollback line, when the terminal grows taller.
	/// @param columns The cells to fill.
	/// @param cells The cells.
	/// @param user The model.
	/// @return 1 if there was one, else 0.
	static int popLine(int columns, VTermScreenCell* cells, void* user) {
		auto& model = *static_cast<TerminalModel*>(user);
		if(model.scrollback_.empty()) return 0;
		const std::vector<TerminalCell>& line = model.scrollback_.back();
		for(int column = 0; column < columns; ++column) {
			const auto index = static_cast<std::size_t>(column);
			cells[column] = cellTo(index < line.size() ? line[index] : TerminalCell{}); // NOLINT(*-pointer-arithmetic)
		}
		model.scrollback_.pop_back();
		return 1;
	}

	/// @brief Forgets the scrollback (the program cleared it, CSI 3 J).
	/// @param user The model.
	/// @return 1.
	static int clearScrollback(void* user) {
		static_cast<TerminalModel*>(user)->scrollback_.clear();
		return 1;
	}

	/// @brief Keeps the properties the model reports: the cursor's visibility, the alternate screen and, if allowed,
	/// the title. The others (icon name, mouse modes, cursor shape) are dropped.
	/// @param property The property.
	/// @param value Its value.
	/// @param user The model.
	/// @return 1.
	static int setProperty(VTermProp property, VTermValue* value, void* user) {
		auto& model = *static_cast<TerminalModel*>(user);
		if(property == VTERM_PROP_CURSORVISIBLE) model.cursorVisible_ = value->boolean != 0;
		else if(property == VTERM_PROP_ALTSCREEN) model.alternateScreen_ = value->boolean != 0;
		else if(property == VTERM_PROP_TITLE && model.allowTitle_) {
			if(value->string.initial) model.title_.clear();
			const std::size_t room = mostTitleBytes - std::min(mostTitleBytes, model.title_.size());
			std::size_t take = std::min<std::size_t>(value->string.len, room);
			while(take > 0 && take < value->string.len && (value->string.str[take] & 0xC0) == 0x80)
				--take; // a cut title ends on a whole character
			model.title_.append(value->string.str, take);
		}
		return 1;
	}

	/// @brief Collects the bytes for the program, at most mostPendingOutput.
	/// @param bytes The bytes.
	/// @param length How many.
	/// @param user The model.
	static void output(const char* bytes, std::size_t length, void* user) {
		auto& model = *static_cast<TerminalModel*>(user);
		const std::size_t room = mostPendingOutput - std::min(mostPendingOutput, model.output_.size());
		model.output_.append(bytes, std::min(length, room));
	}
};

namespace {

/// @brief The screen callbacks: no damage tracking (the widget draws what it shows), no bell.
constexpr VTermScreenCallbacks screenCallbacks{
    .damage = nullptr,
    .moverect = nullptr,
    .movecursor = nullptr,
    .settermprop = &TerminalCallbacks::setProperty,
    .bell = nullptr,
    .resize = nullptr,
    .sb_pushline = &TerminalCallbacks::pushLine,
    .sb_popline = &TerminalCallbacks::popLine,
    .sb_clear = &TerminalCallbacks::clearScrollback,
};

} // namespace

void TerminalModel::VTermDeleter::operator()(VTerm* terminal) const noexcept { vterm_free(terminal); }

TerminalModel::TerminalModel(int rows, int columns, std::size_t scrollbackLines)
    : terminal_(vterm_new(clampCells(rows), clampColumns(columns))),
      scrollbackLimit_(std::min(scrollbackLines, mostScrollbackLines)), rows_(clampCells(rows)),
      columns_(clampColumns(columns)) {
	vterm_set_utf8(terminal_.get(), 1);
	vterm_output_set_callback(terminal_.get(), &TerminalCallbacks::output, this);
	screen_ = vterm_obtain_screen(terminal_.get()); // NOLINT(cppcoreguidelines-prefer-member-initializer): once set up
	vterm_screen_enable_altscreen(screen_, 1);
	vterm_screen_set_callbacks(screen_, &screenCallbacks, this);
	vterm_screen_reset(screen_, 1);
}

TerminalModel::~TerminalModel() = default;

void TerminalModel::write(std::string_view bytes) {
	if(!bytes.empty()) vterm_input_write(terminal_.get(), bytes.data(), bytes.size());
}

void TerminalModel::resize(int rows, int columns) {
	rows = clampCells(rows);
	columns = clampColumns(columns);
	if(rows == rows_ && columns == columns_) return;
	rows_ = rows;
	columns_ = columns;
	vterm_set_size(terminal_.get(), rows, columns);
}

std::string TerminalModel::takeOutput() { return std::exchange(output_, {}); }

void TerminalModel::typeCharacter(char32_t character, Mods mods) {
	vterm_keyboard_unichar(terminal_.get(), character, modifiersOf(mods));
}

bool TerminalModel::pressKey(Key key, Mods mods) {
	const std::optional<VTermKey> vterm = vtermKey(key);
	if(!vterm) return false;
	vterm_keyboard_key(terminal_.get(), *vterm, modifiersOf(mods));
	return true;
}

void TerminalModel::setScrollbackLimit(std::size_t lines) {
	scrollbackLimit_ = std::min(lines, mostScrollbackLines);
	while(scrollback_.size() > scrollbackLimit_) scrollback_.pop_front();
}

TerminalCell TerminalModel::cell(std::size_t line, int column) const {
	if(column < 0 || column >= columns_) return {};
	if(line < scrollback_.size()) {
		const std::vector<TerminalCell>& cells = scrollback_[line];
		const auto index = static_cast<std::size_t>(column);
		return index < cells.size() ? cells[index] : TerminalCell{};
	}
	const std::size_t row = line - scrollback_.size();
	if(std::cmp_greater_equal(row, rows_)) return {};
	VTermScreenCell source{};
	if(vterm_screen_get_cell(screen_, VTermPos{static_cast<int>(row), column}, &source) == 0) return {};
	return cellFrom(screen_, source);
}

std::u32string TerminalModel::lineText(std::size_t line) const {
	std::u32string text;
	for(int column = 0; column < columns_; ++column) {
		const TerminalCell at = cell(line, column);
		if(at.width != 0) text.push_back(at.character == 0 ? U' ' : at.character);
	}
	while(!text.empty() && text.back() == U' ') text.pop_back();
	return text;
}

TerminalCursor TerminalModel::cursor() const {
	VTermPos position{};
	vterm_state_get_cursorpos(vterm_obtain_state(terminal_.get()), &position);
	return {position.row, position.col, cursorVisible_};
}

void TerminalModel::setAllowTitle(bool allow) {
	allowTitle_ = allow;
	if(!allow) title_.clear();
}

} // namespace affresco::format
