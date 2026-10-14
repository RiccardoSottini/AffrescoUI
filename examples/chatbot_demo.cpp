// A chat app built on sGLGUI: threads in the sidebar, each with its own message history.
// Type a message and press Enter (or click Send) to append it to the current thread; "+ New chat" starts a new one.
//
// Each region is a custom widget (a Panel subclass that builds its own children), and ChatApp wires them together.

#include <sgl/sgl.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace sgl::literals;
using sgl::Align;
using sgl::Edge;

namespace theme {

inline constexpr sgl::Color sidebar = sgl::Color::hex(0x111827);
inline constexpr sgl::Color sidebarItem = sgl::Color::hex(0x1f2937);
inline constexpr sgl::Color sidebarSelected = sgl::Color::hex(0x374151);
inline constexpr sgl::Color sidebarText = sgl::Color::hex(0xe5e7eb);
inline constexpr sgl::Color accent = sgl::Color::hex(0x6366f1);
inline constexpr sgl::Color accentPressed = sgl::Color::hex(0x4f46e5);
inline constexpr sgl::Color background = sgl::Color::hex(0xf8fafc);
inline constexpr sgl::Color surface = sgl::colors::white;
inline constexpr sgl::Color divider = sgl::Color::hex(0xe5e7eb);
inline constexpr sgl::Color text = sgl::Color::hex(0x111827);
inline constexpr sgl::Color mutedText = sgl::Color::hex(0x9ca3af);
inline constexpr sgl::Color input = sgl::Color::hex(0xf1f5f9);
inline constexpr sgl::Color inputFocused = sgl::Color::hex(0xeef2ff);

} // namespace theme

namespace {

// Anchors `panel` to the given edges of its parent, `offset` away
void pin(sgl::Panel& panel, std::initializer_list<Edge> edges, sgl::Length offset = 0_px) {
	for(Edge edge : edges) panel.setAnchor(edge, offset);
}

constexpr std::initializer_list<Edge> allEdges = {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right};

// A 1px horizontal line along the top or bottom edge of `parent`
void addDivider(sgl::Panel& parent, Edge edge) {
	auto& line = parent.add();
	pin(line, {edge, Edge::Left, Edge::Right});
	line.setHeight(1_px);
	line.setColor(theme::divider);
}

/* Widgets */

// "+ New chat" button and the list of threads
class Sidebar : public sgl::Panel {
public:
	static constexpr float defaultWidth = 260.0f;
	static constexpr float minWidth = 180.0f;
	static constexpr float maxWidth = 520.0f;

	sgl::Signal<> onNewChat;
	sgl::Signal<std::size_t> onSelect;

	Sidebar() {
		setColor(theme::sidebar);

		auto& newChat = add<sgl::Label>("+  New chat");
		pin(newChat, {Edge::Left, Edge::Right, Edge::Top}, 16_px);
		newChat.setHeight(44_px);
		newChat.setColor(theme::accent);
		newChat.setTextColor(sgl::colors::white);
		newChat.setAlignment(Align::Center, Align::Center);
		newChat.setCursor(sgl::Cursor::Hand);
		newChat.onMouseButton.connect([this](const sgl::MouseButtonEvent& event) {
			if(event.action == sgl::Action::Press) onNewChat();
		});
	}

	std::size_t addThread(std::string_view title) {
		const std::size_t index = items_.size();

		auto& item = add<sgl::Label>(title);
		pin(item, {Edge::Left, Edge::Right}, 12_px);
		item.setAnchor(Edge::Top, sgl::px(84.0f + static_cast<float>(index) * 46.0f));
		item.setHeight(40_px);
		item.setPadding(12.0f);
		item.setAlignment(Align::Start, Align::Center);
		item.setTextColor(theme::sidebarText);
		item.setCursor(sgl::Cursor::Hand);
		item.onMouseButton.connect([this, index](const sgl::MouseButtonEvent& event) {
			if(event.action == sgl::Action::Press) onSelect(index);
		});

		items_.push_back(&item);
		return index;
	}

	void setTitle(std::size_t index, std::string_view title) { items_[index]->setText(title); }

	void highlight(std::size_t index) {
		for(std::size_t i = 0; i < items_.size(); ++i)
			items_[i]->setColor(i == index ? theme::sidebarSelected : theme::sidebarItem);
	}

private:
	std::vector<sgl::Label*> items_;
};

// Thin vertical strip on the sidebar's right edge, like the splitter next to VS Code's file explorer:
// drag it to resize the sidebar. Shows an accent line while hovered or dragged.
class ResizeHandle : public sgl::Panel {
public:
	static constexpr float thickness = 6.0f;

	sgl::Signal<float> onDrag; // requested x of the split line, in window coordinates

	ResizeHandle() {
		setColor(sgl::colors::transparent);
		setCursor(sgl::Cursor::ResizeHorizontal);

		onHover.connect([this](bool hovered) {
			hovered_ = hovered;
			invalidate();
		});

		// The window keeps sending mouse events here until the button is released, even outside the strip
		onMouseButton.connect([this](const sgl::MouseButtonEvent& event) {
			if(event.button != sgl::MouseButton::Left) return;
			dragging_ = event.action == sgl::Action::Press;
			grabOffset_ = event.position.x - splitX(); // keeps the line where it was grabbed, no jump
			invalidate();
		});
		onMouseMove.connect([this](const sgl::MouseMoveEvent& event) {
			if(dragging_) onDrag(event.position.x - grabOffset_);
		});
	}

protected:
	// Drawn rather than added as a child panel: a child would be hit-tested first and steal the clicks
	void paint(sgl::Painter& painter) const override {
		if(!hovered_ && !dragging_) return;
		const float x = splitX();
		painter.fillRect({x - 1.0f, frame().top, x + 1.0f, frame().bottom}, theme::accent);
	}

private:
	[[nodiscard]] float splitX() const noexcept { return frame().left + thickness / 2.0f; }

	bool hovered_ = false;
	bool dragging_ = false;
	float grabOffset_ = 0.0f;
};

// White bar above the messages, showing the thread title
class ChatHeader : public sgl::Panel {
public:
	static constexpr float height = 64.0f;

	ChatHeader() {
		setColor(theme::surface);
		addDivider(*this, Edge::Bottom);

		title_ = &add<sgl::Label>();
		pin(*title_, allEdges);
		title_->setPadding(24.0f);
		title_->setAlignment(Align::Start, Align::Center);
		title_->setTextColor(theme::text);
	}

	void setTitle(std::string_view title) { title_->setText(title); }

private:
	sgl::Label* title_ = nullptr;
};

// Messages of one thread, growing from the bottom: older messages move up and get clipped by this panel
class ChatHistory : public sgl::Panel {
public:
	explicit ChatHistory(std::shared_ptr<const sgl::Font> font) : font_(std::move(font)) {
		setColor(sgl::colors::transparent);

		emptyHint_ = &add<sgl::Label>("No messages yet. Type below to start.");
		pin(*emptyHint_, allEdges);
		emptyHint_->setAlignment(Align::Center, Align::Center);
		emptyHint_->setTextColor(theme::mutedText);
	}

	[[nodiscard]] bool empty() const noexcept { return rows_.empty(); }

	void post(std::string_view text) {
		emptyHint_->setVisible(false);

		auto& row = add();
		row.setColor(sgl::colors::transparent);
		pin(row, {Edge::Left, Edge::Right}, 24_px);
		row.setAnchor(Edge::Bottom, sgl::px(gap));

		auto& bubble = row.add<sgl::Label>(text);
		pin(bubble, {Edge::Top, Edge::Bottom, Edge::Right});
		bubble.setPadding(padding);
		bubble.setWrap(true);
		bubble.setColor(theme::accent);
		bubble.setTextColor(sgl::colors::white);

		// Size the bubble to its text, wrapped at maxTextWidth
		const auto lines = font_->wrap(bubble.codepoints(), maxTextWidth);
		float textWidth = 0.0f;
		for(std::u32string_view line : lines) textWidth = std::max(textWidth, font_->measure(line));

		const float height = static_cast<float>(lines.size()) * font_->lineHeight() + 2.0f * padding;
		bubble.setWidth(sgl::px(std::ceil(textWidth) + 2.0f * padding));
		row.setHeight(sgl::px(height));

		// Make room: every older message moves up by the new one's height
		for(sgl::Panel* older : rows_)
			older->setAnchor(Edge::Bottom, sgl::px(older->anchor(Edge::Bottom)->value + height + gap));
		rows_.push_back(&row);
	}

private:
	static constexpr float gap = 12.0f;
	static constexpr float padding = 12.0f;
	static constexpr float maxTextWidth = 380.0f;

	std::shared_ptr<const sgl::Font> font_;
	sgl::Label* emptyHint_ = nullptr;
	std::vector<sgl::Panel*> rows_;
};

// Text input + Send button. Emits onSend with the typed text on Enter or on a click on the button.
class Composer : public sgl::Panel {
public:
	static constexpr float height = 96.0f;

	sgl::Signal<const std::string&> onSend;

	Composer() {
		setColor(theme::surface);
		addDivider(*this, Edge::Top);

		input_ = &add<sgl::TextArea>();
		pin(*input_, {Edge::Top, Edge::Bottom}, 20_px);
		input_->setAnchor(Edge::Left, 24_px);
		input_->setAnchor(Edge::Right, 120_px);
		input_->setPadding(14.0f);
		input_->setPlaceholder("Type a message...");
		input_->setColor(theme::input);
		input_->setTextColor(theme::text);
		input_->onFocusChanged.connect(
		    [this](bool focused) { input_->setColor(focused ? theme::inputFocused : theme::input); });
		input_->onKey.connect([this](const sgl::KeyEvent& event) {
			const bool enter = event.key == sgl::Key::Enter || event.key == sgl::Key::KeypadEnter;
			if(enter && event.action == sgl::Action::Press) send();
		});

		auto& button = add<sgl::Label>("Send");
		pin(button, {Edge::Top, Edge::Bottom}, 20_px);
		button.setAnchor(Edge::Right, 24_px);
		button.setWidth(80_px);
		button.setColor(theme::accent);
		button.setTextColor(sgl::colors::white);
		button.setAlignment(Align::Center, Align::Center);
		button.setCursor(sgl::Cursor::Hand);
		button.onMouseButton.connect([this, &button](const sgl::MouseButtonEvent& event) {
			if(event.button != sgl::MouseButton::Left) return;
			button.setColor(event.action == sgl::Action::Press ? theme::accentPressed : theme::accent);
			if(event.action == sgl::Action::Release && button.bounds().contains(event.position)) send();
		});
	}

	[[nodiscard]] sgl::TextArea& input() noexcept { return *input_; }

private:
	void send() {
		const std::string message = input_->text();
		if(message.empty()) return;

		input_->clear();
		onSend(message);
	}

	sgl::TextArea* input_ = nullptr;
};

/* Application */

class ChatApp {
public:
	ChatApp()
	    : font_(sgl::Font::load(SGLGUI_EXAMPLES_ASSETS "/fonts/Roboto-Regular.ttf", 16.0f)),
	      window_(app_.createWindow("sGLGUI Chat", 1200, 800)) {
		window_.setMinSize(760, 520);
		window_.setDefaultFont(font_);
		window_.root().setColor(theme::background);

		sidebar_ = &window_.add<Sidebar>();
		pin(*sidebar_, {Edge::Left, Edge::Top, Edge::Bottom});

		// Everything right of the sidebar
		main_ = &window_.add();
		pin(*main_, {Edge::Right, Edge::Top, Edge::Bottom});
		main_->setColor(theme::background);

		header_ = &main_->add<ChatHeader>();
		pin(*header_, {Edge::Top, Edge::Left, Edge::Right});
		header_->setHeight(sgl::px(ChatHeader::height));

		composer_ = &main_->add<Composer>();
		pin(*composer_, {Edge::Bottom, Edge::Left, Edge::Right});
		composer_->setHeight(sgl::px(Composer::height));

		// Added last so it sits on top of both the sidebar and the main area
		resizeHandle_ = &window_.add<ResizeHandle>();
		pin(*resizeHandle_, {Edge::Top, Edge::Bottom});
		resizeHandle_->setWidth(sgl::px(ResizeHandle::thickness));
		resizeHandle_->onDrag.connect([this](float x) { setSidebarWidth(x); });
		window_.onResize.connect([this](const sgl::ResizeEvent&) { setSidebarWidth(sidebarWidth_); });
		setSidebarWidth(Sidebar::defaultWidth);

		sidebar_->onNewChat.connect([this] { select(newThread()); });
		sidebar_->onSelect.connect([this](std::size_t index) { select(index); });
		composer_->onSend.connect([this](const std::string& message) { send(message); });

		// Whatever was clicked, typing keeps going to the input box
		window_.onMouseButton.connect([this](const sgl::MouseButtonEvent& event) {
			if(event.action == sgl::Action::Press) window_.setFocus(&composer_->input());
		});

		select(newThread());
	}

	int run() { return app_.run(); }

private:
	struct Thread {
		ChatHistory* history;
		std::string title;
	};

	// Moves the split between sidebar and main area, keeping both usable
	void setSidebarWidth(float width) {
		const float maxWidth = std::min(Sidebar::maxWidth, static_cast<float>(window_.width()) - 400.0f);
		sidebarWidth_ = std::round(std::clamp(width, Sidebar::minWidth, std::max(Sidebar::minWidth, maxWidth)));

		sidebar_->setWidth(sgl::px(sidebarWidth_));
		main_->setAnchor(Edge::Left, sgl::px(sidebarWidth_));
		resizeHandle_->setX(sgl::px(sidebarWidth_ - ResizeHandle::thickness / 2.0f));
	}

	std::size_t newThread() {
		auto& history = main_->add<ChatHistory>(font_);
		pin(history, {Edge::Left, Edge::Right});
		history.setAnchor(Edge::Top, sgl::px(ChatHeader::height));
		history.setAnchor(Edge::Bottom, sgl::px(Composer::height));

		threads_.push_back({&history, "New chat"});
		return sidebar_->addThread(threads_.back().title);
	}

	void select(std::size_t index) {
		current_ = index;
		for(std::size_t i = 0; i < threads_.size(); ++i) threads_[i].history->setVisible(i == index);

		sidebar_->highlight(index);
		header_->setTitle(threads_[index].title);
		window_.setFocus(&composer_->input());
	}

	void send(const std::string& message) {
		Thread& thread = threads_[current_];

		// A thread is named after its first message
		if(thread.history->empty()) {
			thread.title = message;
			sidebar_->setTitle(current_, message);
			header_->setTitle(message);
		}

		thread.history->post(message);
	}

	sgl::Application app_; // first: it must outlive everything below
	std::shared_ptr<const sgl::Font> font_;
	sgl::Window& window_;

	Sidebar* sidebar_ = nullptr;
	ResizeHandle* resizeHandle_ = nullptr;
	float sidebarWidth_ = Sidebar::defaultWidth;
	sgl::Panel* main_ = nullptr;
	ChatHeader* header_ = nullptr;
	Composer* composer_ = nullptr;

	std::vector<Thread> threads_;
	std::size_t current_ = 0;
};

} // namespace

int main() {
	ChatApp app;
	return app.run();
}
