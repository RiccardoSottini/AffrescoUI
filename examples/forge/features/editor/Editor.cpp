/// @file
/// @brief Forge's editor feature, native side (Editor.hpp).

#include "features/editor/Editor.hpp"

#include "CodeEditor.hpp"
#include "features/editor/Buffers.hpp"

#include <affresco/core/Json.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace forge {

namespace {

/// @brief The "id" of a call's arguments.
/// @param arguments The arguments, from the document (untrusted).
/// @return It.
/// @throws std::runtime_error If there is none.
std::string idOf(const std::string& arguments) {
	const affresco::json with = affresco::parseUntrusted(arguments);
	if(!with.is_object() || !with.contains("id") || !with["id"].is_string() || with["id"].get_ref<const std::string&>().empty())
		throw std::runtime_error("expected { \"id\": file }");
	return with["id"].get<std::string>();
}

/// @brief What a buffer call returns: its id and whether it is dirty, or the error it failed with.
/// @param buffers The buffers.
/// @param id The buffer.
/// @param error What the call returned.
/// @return `{ "id", "dirty" }`, as text.
/// @throws std::runtime_error With @p error, if there is one.
std::string answer(const Buffers& buffers, const std::string& id, const std::optional<std::string>& error) {
	if(error) throw std::runtime_error(*error);
	return affresco::json{{"id", id}, {"dirty", buffers.dirty(id)}}.dump();
}

/// @brief Tells the document, once the current event is delivered, that a buffer became dirty or clean.
/// @param screen The screen.
/// @param buffers The buffers, which say what the buffer is by then.
/// @param id The buffer.
void tellDocument(affresco::Screen& screen, const std::weak_ptr<Buffers>& buffers, const std::string& id) {
	affresco::Panel* root = screen.find("root");
	if(!root) return;
	const auto run = [&screen, buffers, id] {
		const std::shared_ptr<Buffers> alive = buffers.lock(); // gone with the screen
		affresco::Panel* sender = alive ? screen.find("root") : nullptr;
		if(!sender) return;
		screen.runAction("bufferChanged", *sender, affresco::json{{"id", id}, {"dirty", alive->dirty(id)}}.dump());
	};
	// Deferred: the edit is still being delivered to an editor the action may rebuild
	if(affresco::Window* window = root->window()) window->defer(run);
	else run();
}

} // namespace

void registerEditor(affresco::Screen& screen, std::shared_ptr<Workspace> workspace) {
	const auto buffers = std::make_shared<Buffers>();
	examples::registerCodeEditor(screen, [weak = std::weak_ptr<Buffers>(buffers)](std::string_view id) {
		const std::shared_ptr<Buffers> alive = weak.lock();
		return alive ? alive->find(id) : nullptr;
	});
	screen.setModel("buffers", buffers->model());
	buffers->onDirty([&screen, weak = std::weak_ptr<Buffers>(buffers)](const std::string& id, bool) {
		tellDocument(screen, weak, id);
	});
	screen.onCall("forge.open", [buffers, workspace](const std::string& arguments) {
		const std::string id = idOf(arguments);
		return answer(*buffers, id, buffers->open(workspace->root, id));
	});
	screen.onCall("forge.save", [buffers](const std::string& arguments) {
		const std::string id = idOf(arguments);
		return answer(*buffers, id, buffers->save(id));
	});
	screen.onCall("forge.revert", [buffers](const std::string& arguments) {
		const std::string id = idOf(arguments);
		return answer(*buffers, id, buffers->revert(id));
	});
	screen.onCall("forge.close", [buffers](const std::string& arguments) {
		const std::string id = idOf(arguments);
		buffers->close(id);
		return affresco::json{{"id", id}, {"dirty", false}}.dump();
	});
	screen.onCall("forge.saveAll", [buffers](const std::string&) {
		const std::vector<std::string> saved = buffers->dirtyIds();
		if(auto error = buffers->saveAll()) throw std::runtime_error(*error);
		return affresco::json{{"saved", saved}}.dump();
	});
	screen.onCall("forge.todos", [buffers](const std::string&) { return buffers->todos().dump(); });
}

} // namespace forge
