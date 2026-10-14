// A chat app whose whole UI is described in assets/screens/chat.screen.json (sGLGUI Screen Format).
// This file only adds behaviour: actions from the screen ("newChat", "selectThread", "send"), the threads and their
// messages (built from the screen's templates), and one custom panel type, the sidebar's resize handle.

#include <sgl/sgl.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace sgl::literals;

namespace {

// Thin vertical strip on the sidebar's right edge, like the splitter next to VS Code's file explorer:
// drag it to resize the sidebar. Shows a line while hovered or dragged. Used in the screen as "type": "resizeHandle".
class ResizeHandle : public sgl::Panel {
public:
	sgl::Signal<float> onDrag; // requested x of the split line, in window coordinates

	explicit ResizeHandle(sgl::Color lineColor) : lineColor_(lineColor) {
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
		painter.fillRect({x - 1.0f, frame().top, x + 1.0f, frame().bottom}, lineColor_);
	}

private:
	[[nodiscard]] float splitX() const noexcept { return frame().left + frame().width() / 2.0f; }

	sgl::Color lineColor_;
	bool hovered_ = false;
	bool dragging_ = false;
	float grabOffset_ = 0.0f;
};

class ChatApp {
public:
	ChatApp()
	    : screen_(sgl::Screen::fromFile(SGLGUI_EXAMPLES_ASSETS "/screens/chat.screen.json")), window_(createWindow()) {
		sidebar_ = &screen_.get("sidebar");
		main_ = &screen_.get("main");
		splitter_ = &screen_.get<ResizeHandle>("splitter");
		threadList_ = &screen_.get("threadList");
		threadViews_ = &screen_.get("threadViews");
		title_ = &screen_.get<sgl::Label>("title");
		input_ = &screen_.get<sgl::TextArea>("input");

		screen_.onAction("newChat", [this](sgl::Panel&) { select(newThread()); });
		screen_.onAction("selectThread", [this](sgl::Panel& item) { select(indexOf(item)); });
		screen_.onAction("send", [this](sgl::Panel&) { send(); });

		input_->onKey.connect([this](const sgl::KeyEvent& event) {
			const bool enter = event.key == sgl::Key::Enter || event.key == sgl::Key::KeypadEnter;
			if(enter && event.action == sgl::Action::Press) send();
		});

		// Whatever was clicked, typing keeps going to the input box
		window_.onMouseButton.connect([this](const sgl::MouseButtonEvent& event) {
			if(event.action == sgl::Action::Press) window_.setFocus(input_);
		});

		sidebarWidth_ = sidebar_->width().value;
		splitter_->onDrag.connect([this](float x) { setSidebarWidth(x); });
		window_.onResize.connect([this](const sgl::ResizeEvent&) { setSidebarWidth(sidebarWidth_); });

		select(newThread());
	}

	int run() { return app_.run(); }

private:
	static constexpr float minSidebarWidth = 180.0f;
	static constexpr float maxSidebarWidth = 520.0f;
	static constexpr float minChatWidth = 400.0f;

	struct Thread {
		sgl::Label* item;  // in the sidebar
		sgl::Panel* view;  // messages area
		std::string title; // its first message
		std::size_t messages = 0;
	};

	sgl::Window& createWindow() {
		screen_.registerType("resizeHandle",
		                     [this] { return std::make_unique<ResizeHandle>(screen_.color("accent")); });
		return screen_.createWindow(app_);
	}

	// Moves the split between sidebar and main area, keeping both usable
	void setSidebarWidth(float width) {
		const float maxWidth = std::min(maxSidebarWidth, static_cast<float>(window_.width()) - minChatWidth);
		sidebarWidth_ = std::round(std::clamp(width, minSidebarWidth, std::max(minSidebarWidth, maxWidth)));

		sidebar_->setWidth(sgl::px(sidebarWidth_));
		main_->setAnchor(sgl::Edge::Left, sgl::px(sidebarWidth_));
		splitter_->setX(sgl::px(sidebarWidth_ - splitter_->width().value / 2.0f));
	}

	std::size_t newThread() {
		auto& item = dynamic_cast<sgl::Label&>(screen_.instantiate("threadItem", *threadList_));
		auto& view = screen_.instantiate("thread", *threadViews_);
		threads_.push_back({&item, &view, item.text()});
		return threads_.size() - 1;
	}

	[[nodiscard]] std::size_t indexOf(const sgl::Panel& item) const {
		const auto it = std::ranges::find(threads_, &item, &Thread::item);
		return it == threads_.end() ? current_ : static_cast<std::size_t>(it - threads_.begin());
	}

	void select(std::size_t index) {
		current_ = index;
		for(std::size_t i = 0; i < threads_.size(); ++i) {
			threads_[i].view->setVisible(i == index);
			threads_[i].item->setColor(screen_.color(i == index ? "sidebarSelected" : "sidebarItem"));
		}

		title_->setText(threads_[index].title);
		window_.setFocus(input_);
	}

	void send() {
		const std::string message = input_->text();
		if(message.empty()) return;
		input_->clear();

		Thread& thread = threads_[current_];
		if(thread.messages++ == 0) { // a thread is named after its first message
			thread.title = message;
			thread.item->setText(message);
			title_->setText(message);
			thread.view->findById("emptyHint")->setVisible(false);
		}

		auto& bubble = dynamic_cast<sgl::Label&>(screen_.instantiate("message", *thread.view->findById("messages")));
		bubble.setText(message);
	}

	sgl::Application app_; // first: it must outlive everything below
	sgl::Screen screen_;
	sgl::Window& window_;

	sgl::Panel* sidebar_ = nullptr;
	sgl::Panel* main_ = nullptr;
	ResizeHandle* splitter_ = nullptr;
	sgl::Panel* threadList_ = nullptr;
	sgl::Panel* threadViews_ = nullptr;
	sgl::Label* title_ = nullptr;
	sgl::TextArea* input_ = nullptr;

	std::vector<Thread> threads_;
	std::size_t current_ = 0;
	float sidebarWidth_ = 0.0f;
};

} // namespace

int main() {
	try {
		ChatApp app;
		return app.run();
	} catch(const sgl::ScreenError& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
