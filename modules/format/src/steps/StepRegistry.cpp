/// @file
/// @brief format::StepRegistry: groups of steps, and the order the built-in ones are tried in.

#include "StepRegistry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace affresco::format {

void StepRegistry::add(StepGroup group) {
	if(!group.run) throw std::logic_error("AffrescoUI screen: the step group '" + group.name + "' has no runner");
	for(const StepType& step : group.steps)
		if(find(step.name))
			throw std::logic_error("AffrescoUI screen: the step '" + step.name + "' is registered twice");
	groups_.push_back(std::move(group));
}

std::optional<bool> StepRegistry::run(const Running& step) const {
	for(const StepGroup& group : groups_)
		if(const std::optional<bool> keepGoing = group.run(step)) return keepGoing;
	return std::nullopt;
}

const StepType* StepRegistry::find(std::string_view name) const {
	for(const StepGroup& group : groups_)
		if(const auto it = std::ranges::find(group.steps, name, &StepType::name); it != group.steps.end()) return &*it;
	return nullptr;
}

std::shared_ptr<const StepRegistry> builtInSteps() {
	static const std::shared_ptr<const StepRegistry> steps = [] {
		auto registry = std::make_shared<StepRegistry>();
		registerBuiltInSteps(*registry);
		return registry;
	}();
	return steps;
}

// The order steps are tried in: a step with the keys of two groups runs in the first
void registerBuiltInSteps(StepRegistry& registry) {
	registerStateSteps(registry);
	registerFlowSteps(registry);
	registerOutsideSteps(registry);
	registerServiceSteps(registry);
	registerAuthSteps(registry);
	registerAssistantSteps(registry);
	registerReviewSteps(registry);
	registerNavigationSteps(registry);
	registerEditorSteps(registry);
	registerTimeSteps(registry);
	registerOtherSteps(registry);
	registerSceneSteps(registry);
}

} // namespace affresco::format
