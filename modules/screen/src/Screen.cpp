#include "sgl/Screen.hpp"

#include "sgl/channel/Session.hpp"
#include "sgl/format/Rules.hpp"
#include "sgl/format/Runtime.hpp"
#include "sgl/vcs/Repository.hpp"

#include <fstream>
#include <sstream>

namespace sgl {

// Declaration order is construction order: rules, then the repository that uses them, then the runtime and the
// session that use the repository (destroyed the other way round)
struct Screen::Impl {
	format::Rules rules;
	vcs::Repository repository;
	format::Runtime runtime;
	std::unique_ptr<channel::Session> session;
	Signal<const std::string&> events;

	Impl(json document, std::filesystem::path baseDirectory)
	    : repository(rules, std::move(document)), runtime(repository, std::move(baseDirectory)) {
		runtime.onEvent.connect([this](const json& event) {
			if(session) session->send(event);
			events(event.dump());
		});
		runtime.onError.connect([this](const std::string& message) {
			if(session)
				session->send({{"type", "error"}, {"message", message}, {"revision", repository.head().number}});
		});
	}
};

namespace {

std::optional<json> parseJson(std::string_view text, const char* what, std::string* error) {
	try {
		return json::parse(text);
	} catch(const json::parse_error& parseError) {
		if(error) *error = std::string("sGLGUI screen: invalid ") + what + " JSON: " + parseError.what();
		return std::nullopt;
	}
}

bool succeeded(const std::optional<std::string>& failure, std::string* error) {
	if(failure && error) *error = *failure;
	return !failure;
}

} // namespace

Screen::Screen(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Screen::Screen(Screen&&) noexcept = default;
Screen& Screen::operator=(Screen&&) noexcept = default;
Screen::~Screen() = default;

Screen Screen::fromFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if(!file) {
		throw ScreenError(
		    "sGLGUI screen: could not open " + std::filesystem::absolute(path).string() +
		    " (relative paths start from the current folder: " + std::filesystem::current_path().string() + ")");
	}

	std::stringstream content;
	content << file.rdbuf();
	return fromString(content.str(), path.parent_path());
}

Screen Screen::fromString(std::string_view content, std::filesystem::path baseDirectory) {
	json document;
	try {
		document = json::parse(content);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: invalid JSON: ") + error.what());
	}
	return Screen(std::make_unique<Impl>(std::move(document), std::move(baseDirectory)));
}

/* Runtime */

void Screen::registerType(std::string name, Factory factory) {
	impl_->runtime.registerType(std::move(name), std::move(factory));
}

void Screen::setPlaceholderTypes(bool enabled) { impl_->runtime.setPlaceholderTypes(enabled); }

Window& Screen::createWindow(Application& app) { return impl_->runtime.createWindow(app); }

void Screen::build(Window& window) { impl_->runtime.build(window); }

Panel& Screen::instantiate(std::string_view name, Panel& parent) { return impl_->runtime.instantiate(name, parent); }

void Screen::onAction(std::string name, Action action) { impl_->runtime.onAction(std::move(name), std::move(action)); }

void Screen::onUnhandledAction(UnhandledAction fallback) { impl_->runtime.onUnhandledAction(std::move(fallback)); }

void Screen::runAction(std::string_view name, Panel& sender, std::string_view eventJson) {
	json event;
	try {
		event = json::parse(eventJson);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: runAction: invalid event JSON: ") + error.what());
	}
	impl_->runtime.runAction(name, sender, event);
}

Panel* Screen::find(std::string_view id) noexcept { return impl_->runtime.find(id); }

Color Screen::color(std::string_view name) const { return impl_->runtime.color(name); }

std::shared_ptr<const Font> Screen::font(std::string_view name) const { return impl_->runtime.font(name); }

Connection Screen::onEvent(std::function<void(const std::string&)> listener) {
	return impl_->events.connect(std::move(listener));
}

/* Version control */

bool Screen::applyRevision(std::string_view revisionJson, std::string* error) {
	const auto revision = parseJson(revisionJson, "revision", error);
	return revision && succeeded(impl_->repository.commit(*revision), error);
}

RevisionResult Screen::submitRevision(std::string_view revisionJson) {
	std::string error;
	const auto parsed = parseJson(revisionJson, "revision", &error);
	if(!parsed) return {RevisionStatus::Rejected, revision(), 0, error};
	return impl_->repository.submit(*parsed);
}

std::vector<ProposalInfo> Screen::proposals() const { return impl_->repository.proposals(); }

bool Screen::acceptProposal(int id, std::string* error) { return succeeded(impl_->repository.accept(id), error); }

bool Screen::rejectProposal(int id, std::string_view reason) { return impl_->repository.reject(id, reason); }

bool Screen::previewProposal(int id, std::string* error) { return succeeded(impl_->repository.preview(id), error); }

void Screen::endPreview() { impl_->repository.endPreview(); }

std::optional<int> Screen::previewedProposal() const noexcept { return impl_->repository.previewing(); }

std::vector<HistoryEntry> Screen::history() const { return impl_->repository.history(); }

bool Screen::restore(int revision, std::string* error) {
	try {
		return succeeded(impl_->repository.commit(impl_->repository.restoreRevision(revision, "user")), error);
	} catch(const DocumentError& failure) {
		return succeeded(failure.what(), error);
	}
}

RevisionResult Screen::submitRestore(int revision, std::string_view author) {
	try {
		return impl_->repository.submit(impl_->repository.restoreRevision(revision, author));
	} catch(const DocumentError& failure) {
		return {RevisionStatus::Rejected, this->revision(), 0, failure.what()};
	}
}

bool Screen::undo(std::string* error) {
	const auto entries = history();
	if(entries.size() < 2) return succeeded("sGLGUI screen: nothing to undo", error);
	return restore(entries[entries.size() - 2].number, error);
}

int Screen::revision() const noexcept { return impl_->repository.head().number; }

std::string Screen::exportChanges(int fromRevision, std::string_view name) const {
	return impl_->repository.exportChanges(fromRevision, name).dump(2);
}

bool Screen::importChanges(std::string_view changesJson, std::string* error) {
	const auto changes = parseJson(changesJson, "changes", error);
	return changes && succeeded(impl_->repository.importChanges(*changes), error);
}

std::string Screen::documentJson() const { return impl_->repository.document().dump(); }

std::string Screen::stateJson() const { return impl_->repository.state().dump(); }

VersionControl& Screen::versions() noexcept { return impl_->repository; }

/* Channel */

void Screen::connect(Channel& channel, std::string author) {
	impl_->session.reset(); // one channel at a time
	impl_->session = std::make_unique<channel::Session>(channel, impl_->repository, std::move(author));
}

} // namespace sgl
