/// @file
/// @brief Sinopia's presenting natives: the audience window, a second window sharing the workspace's GL context, and
/// the frames it mirrors, with the slide transitions blended between them.

#include "Sinopia.hpp"
#include "Transition.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace sinopia {

using affresco::json;

namespace {

/// @brief The workspace actions the audience window's keys may run.
constexpr std::array<std::string_view, 4> forwardedActions{"nextStep", "previousStep", "toggleBlack", "stopPresenting"};

/// @brief How often the audience window checks whether the workspace's window was closed, in milliseconds.
constexpr double watchMs = 250.0;

/// @brief The widest frame presented, in pixels.
constexpr float frameWidth = 1280.0f;

/// @brief A slide's image as a frame to blend.
/// @param image The image (opaque, so its premultiplied pixels are its plain ones).
/// @return The frame.
[[nodiscard]] Frame frameOf(const affresco::Bitmap& image) {
	return {image.width(), image.height(), {image.pixels().begin(), image.pixels().end()}};
}

/// @brief A frame as an image.
/// @param frame The frame.
/// @return The image.
[[nodiscard]] std::shared_ptr<const affresco::Bitmap> imageOf(const Frame& frame) {
	return affresco::Bitmap::fromPixels(frame.width, frame.height, frame.pixels, {});
}

/// @brief Shows a frame on the workspace (the presenter's current slide) and in the audience window.
/// @param session The session.
/// @param image The frame.
void showFrame(const Session& session, const std::shared_ptr<const affresco::Bitmap>& image) {
	session.screen->setImage("present", image);
	if(session.audienceWindow != nullptr && !session.audiences.empty())
		session.audiences.back()->setImage("present", image);
}

/// @brief Quits once the workspace's window is closed: a hidden audience window would keep Sinopia running.
/// @param session The session.
/// @param window The audience window, which checks.
void watchWorkspace(const std::shared_ptr<Session>& session, affresco::Window& window) {
	window.setTimer(watchMs, [session, &window] {
		if(session->window != nullptr && session->window->shouldClose() && session->app != nullptr)
			session->app->quit();
		else watchWorkspace(session, window);
	});
}

/// @brief Opens the audience window, or shows it again: a window closed by its own button can't be shown again, so a
/// new one is made (the screens of closed ones are kept, as windows can't be destroyed).
/// @param session The session.
/// @return `{ opened }`: false without a window system (in scenarios).
[[nodiscard]] json openAudience(const std::shared_ptr<Session>& session) {
	if(session->app == nullptr || session->window == nullptr) return {{"opened", false}};
	if(session->audienceWindow != nullptr && !session->audienceWindow->shouldClose()) {
		session->audienceWindow->setVisible(true);
		return {{"opened", true}};
	}
	auto audience = std::make_unique<affresco::Screen>(affresco::Screen::fromFile(
	    session->folder / "audience.screen.json", session->folder, affresco::Screen::History::InMemory));
	std::weak_ptr<Session> weak = session;
	audience->onCall("sinopia.forward", [weak](const std::string& arguments) {
		const json with = affresco::parseUntrusted(arguments);
		const std::string action = with.is_object() ? with.value("action", std::string()) : std::string();
		const std::shared_ptr<Session> shared = weak.lock();
		affresco::Panel* sender = shared && shared->screen ? shared->screen->find("workspace") : nullptr;
		if(sender == nullptr || !std::ranges::contains(forwardedActions, std::string_view(action)))
			throw std::runtime_error("Sinopia: the audience window can't run " + action);
		shared->screen->runAction(action, *sender);
		return json(true).dump();
	});
	affresco::Window& window = audience->createWindow(*session->app, *session->window);
	session->audiences.push_back(std::move(audience));
	session->audienceWindow = &window;
	watchWorkspace(session, window);
	if(session->lastFrame) session->audiences.back()->setImage("present", session->lastFrame);
	return {{"opened", true}};
}

/// @brief Draws the frame presented: the slide at its build step, blended with the one it replaces while the
/// transition runs, or black.
/// @param session The session.
/// @param with `{ slide, step, t, kind, black, width }`: the slide's id, its build step, how far along the transition
/// (or the build) is from 0 to 1, the transition's kind, whether the screen is blacked out, and the slide's width.
/// @return `{ shown }`: false headless.
[[nodiscard]] json mirror(Session& session, const json& with) {
	const std::string slide = with.value("slide", std::string());
	affresco::Panel* panel = stagePanel(session, slide);
	if(session.window == nullptr || panel == nullptr) return {{"shown", false}};
	const auto image = session.window->renderToImage(*panel, frameWidth / std::max(with.value("width", 800.0f), 1.0f));
	if(!image) return {{"shown", false}};
	const std::string key = slide + "#" + std::to_string(with.value("step", 0));
	if(key != session.presenting) {
		session.leaving = session.lastFrame;
		session.presenting = key;
	}
	const float time = with.value("t", 1.0f);
	Frame frame = frameOf(*image);
	if(with.value("black", false)) std::ranges::fill(frame.pixels, std::uint8_t{0});
	else if(session.leaving && time < 1.0f)
		frame = blendTransition(frameOf(*session.leaving), frame, with.value("kind", std::string("fade")), time);
	for(std::size_t i = 3; i < frame.pixels.size(); i += 4) frame.pixels[i] = 255;
	const std::shared_ptr<const affresco::Bitmap> shown = imageOf(frame);
	if(time >= 1.0f) session.lastFrame = image;
	showFrame(session, shown);
	return {{"shown", true}};
}

} // namespace

void registerPresenting(affresco::Screen& screen, const std::shared_ptr<Session>& session) {
	screen.onCall("sinopia.openAudience", [session](const std::string&) { return openAudience(session).dump(); });
	screen.onCall("sinopia.closeAudience", [session](const std::string&) {
		if(session->audienceWindow != nullptr) session->audienceWindow->setVisible(false);
		session->leaving = nullptr;
		session->presenting.clear();
		return json({{"closed", true}}).dump();
	});
	screen.onCall("sinopia.mirror", [session](const std::string& arguments) {
		const json with = affresco::parseUntrusted(arguments);
		return mirror(*session, with.is_object() ? with : json::object()).dump();
	});
}

} // namespace sinopia
