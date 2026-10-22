/// @file
/// @brief format::TypeRegistry: adding, finding and creating node types.

#include "TypeRegistry.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace affresco::format {

const PropertySpec* PropertySet::find(std::string_view name) const noexcept {
	const auto it = std::ranges::find(properties, name, &PropertySpec::name);
	return it == properties.end() ? nullptr : &*it;
}

void TypeRegistry::add(NodeType type) {
	if(!type.own) type.own = std::make_shared<const PropertySet>();
	if(type.own->appliesTo && std::ranges::find(inherited_, type.own) == inherited_.end())
		inherited_.push_back(type.own);
	if(!types_.contains(type.name)) order_.push_back(type.name);
	std::string name = type.name;
	types_.insert_or_assign(std::move(name), std::move(type));
}

void TypeRegistry::addProperties(std::shared_ptr<const PropertySet> set) {
	if(set && set->appliesTo && std::ranges::find(inherited_, set) == inherited_.end())
		inherited_.push_back(std::move(set));
}

std::vector<const NodeType*> TypeRegistry::inOrder() const {
	std::vector<const NodeType*> result;
	result.reserve(order_.size());
	for(const std::string& name : order_) result.push_back(&types_.at(name));
	return result;
}

const NodeType* TypeRegistry::find(std::string_view name) const {
	const auto it = types_.find(name);
	return it == types_.end() ? nullptr : &it->second;
}

std::unique_ptr<Panel> TypeRegistry::create(const std::string& type, const std::string& path, bool placeholders) const {
	if(const NodeType* found = find(type)) {
		auto panel = found->create();
		if(!panel) expect::fail(path, "the factory of type '" + type + "' returned nothing");
		return panel;
	}

	if(placeholders) {
		log(LogLevel::Warning, "AffrescoUI screen: " + path + ": unknown type '" + type + "', shown as a plain panel");
		return std::make_unique<Panel>();
	}
	expect::fail(path, "unknown type '" + type +
	                       "' (built in: panel, label, textarea, editor, stack, row, column, grid, toggle, checkbox, "
	                       "slider, image, outlet; others must be registered)");
}

PropertySets TypeRegistry::setsFor(const NodeType* type, const Panel& panel) const {
	PropertySets sets{};
	std::size_t count = 0;
	// A set that only describes properties the runtime applies itself (the core types') has nothing to do here
	if(type && (type->own->apply || type->own->value.show)) sets[count++] = type->own;
	for(const auto& set : inherited_) {
		if(std::ranges::find(sets, set) != sets.end() || !set->appliesTo(panel)) continue;
		if(count == sets.size()) throw std::logic_error("AffrescoUI screen: a panel inherits too many property sets");
		sets[count++] = set;
	}
	return sets;
}

bool TypeRegistry::transitionable(std::string_view name) const {
	return std::ranges::any_of(types_, [name](const auto& entry) {
		const PropertySpec* spec = entry.second.own->find(name);
		return spec && spec->transition;
	});
}

} // namespace affresco::format
