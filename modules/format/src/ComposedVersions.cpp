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
	syncState();
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
	syncState();
	if(observer_) observer_->documentChanged();
}

void ComposedVersions::proposalsChanged() {
	if(observer_) observer_->proposalsChanged();
}

// The rules refuse a revision whose features don't fit, so the problems here are only ever of a document loaded
// from disk: its features that don't fit are left out, and the checker's findings say why
void ComposedVersions::recompose() {
	composing_ = needsComposition(inner_.document());
	composed_ = composing_ ? compose(inner_.document()).screen : json();
}

void ComposedVersions::syncState() {
	json& state = inner_.state();
	if(!state.is_object()) return;
	const json wanted = featureStates(inner_.document());
	for(const std::string& name : featureStates_)
		if(!wanted.contains(name)) state.erase(name); // removed or disabled: its state goes with it
	featureStates_.clear();
	for(const auto& [name, initial] : wanted.items()) {
		if(!state.contains(name)) state[name] = initial;
		featureStates_.insert(name);
	}
}

} // namespace affresco::format
