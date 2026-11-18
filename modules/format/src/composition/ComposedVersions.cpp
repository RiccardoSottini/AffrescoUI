/// @file
/// @brief format::ComposedVersions.

#include "ComposedVersions.hpp"
#include "Composition.hpp"

namespace affresco::format {

ComposedVersions::ComposedVersions(VersionControl& inner) : inner_(inner) {
	forwarded_ = {inner_.onDecision.connect([this](const Decision& decision) { onDecision(decision); }),
	              inner_.onCommit.connect([this](const CommitInfo& commit) { onCommit(commit); }),
	              inner_.onReviewChanged.connect([this] { onReviewChanged(); })};
	recompose();
	static_cast<void>(syncState());
	inner_.setObserver(this);
}

ComposedVersions::~ComposedVersions() {
	inner_.setObserver(nullptr);
	inner_.onDecision.disconnect(forwarded_[0]);
	inner_.onCommit.disconnect(forwarded_[1]);
	inner_.onReviewChanged.disconnect(forwarded_[2]);
}

const json& ComposedVersions::document() const { return composing_ ? composed_ : inner_.document(); }

void ComposedVersions::documentChanged() {
	recompose();
	static_cast<void>(syncState());
	if(observer_) observer_->documentChanged();
}

void ComposedVersions::documentChangedWith(const DocumentChange& change) {
	if(change.everything || !change.before) {
		documentChanged();
		return;
	}
	DocumentChange passed = change;
	json composedBefore; // what the observer saw, while it compares
	if(change.document.empty()) passed.before = &document(); // the same object: nothing to compose again
	else {
		const bool wasComposing = composing_;
		composedBefore = std::move(composed_);
		recompose();
		if(wasComposing || composing_) passed = DocumentChange{}; // everything
		passed.before = wasComposing ? &composedBefore : change.before;
	}
	const std::vector<std::string> features = syncState();
	passed.state.insert(passed.state.end(), features.begin(), features.end());
	if(observer_) observer_->documentChangedWith(passed);
}

void ComposedVersions::proposalsChanged() {
	if(observer_) observer_->proposalsChanged();
}

// The rules refuse a revision whose features don't fit, so the problems here are only ever of a document loaded
// from disk: its features that don't fit are left out, and the checker's findings say why
void ComposedVersions::recompose() {
	composing_ = needsComposition(inner_.document());
	merged_.clear();
	if(!composing_) {
		composed_ = json();
		return;
	}
	Composition composition = compose(inner_.document());
	composed_ = std::move(composition.screen);
	merged_ = std::move(composition.merged);
}

std::vector<std::string> ComposedVersions::syncState() {
	std::vector<std::string> changed;
	json& state = inner_.state();
	if(!state.is_object()) return changed;
	const json wanted = featureStates(inner_.document());
	const auto place = [](const std::string& name) { return (json::json_pointer() / name).to_string(); };
	for(const std::string& name : featureStates_)
		if(!wanted.contains(name) && state.erase(name) > 0U) changed.push_back(place(name)); // its state goes with it
	featureStates_.clear();
	for(const auto& [name, initial] : wanted.items()) {
		if(!state.contains(name)) {
			state[name] = initial;
			changed.push_back(place(name));
		}
		featureStates_.insert(name);
	}
	return changed;
}

} // namespace affresco::format
