/// @file
/// @brief format::ComputedValues: the cached "computed" values.

#include "ComputedValues.hpp"
#include "Engine.hpp"

#include <algorithm>

namespace affresco::format {

json ComputedValues::computed(std::string_view name) { return value(name); }

const json& ComputedValues::value(std::string_view name) {
	const DocumentContext& context = engine_.document;
	const json& document = engine_.versions.document();
	const std::string key(name);
	if(!document.contains("computed") || !document["computed"].contains(key))
		fail("computed", "no computed value '" + key + "'");
	if(version_ != context.stateVersion() && computing_.empty()) { // only between evaluations: values read stay
		values_.clear();
		reads_.clear();
		version_ = context.stateVersion();
	}
	ReadSet* const reading = context.reads();
	if(const auto cached = values_.find(key); cached != values_.end()) {
		if(reading) reading->add(reads_[key]); // what reads it reads what it read
		return cached->second;
	}
	if(std::ranges::find(computing_, key) != computing_.end())
		fail("computed." + key, "computed values refer to each other in a circle");
	if(computing_.size() >= maxComputedDepth)
		fail("computed." + key, "computed values read each other too deeply (more than 32 levels)");
	computing_.push_back(key);
	json result;
	ReadSet read;
	try {
		const DocumentContext::ReadScope scope(context, &read);
		const json none = json::object();
		result = format::evaluate(document["computed"][key], context.context(*noVariables(), none), "computed." + key);
	} catch(...) {
		computing_.pop_back();
		throw;
	}
	computing_.pop_back();
	if(reading) reading->add(read);
	reads_[key] = std::move(read);
	return values_.insert_or_assign(key, std::move(result)).first->second; // a map: its address stays
}

} // namespace affresco::format
