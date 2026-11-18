/// @file
/// @brief format::ScopeLayers: the layers of variables ("with", "let", a component's parameters) of the panels'
/// scopes, each a reader of what its variables read.

#include "ScopeLayers.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace affresco::format {

namespace {

/// @brief Whether a read lies in what a repeat's row numbers: its item, or its number ("<list>/~i").
/// @param path The read, as a JSON Pointer.
/// @param row The row's scope.
/// @return true if it does.
bool readsRow(std::string_view path, const Scope& row) {
	const std::string list = row.listPlace();
	if(path.size() == list.size() + 1U + indexKey.size() && path.starts_with(list) &&
	   path.substr(list.size() + 1U) == indexKey)
		return true;
	const std::string& origin = row.origin();
	return path.starts_with(origin) && (path.size() == origin.size() || path[origin.size()] == '/');
}

/// @brief Writes a read of a row's item or number under the row's key (rowKey), as the panels' reads are, so an
/// insert or a removal before the row numbers it again in place (PlaceIndex::shift()).
/// @param path The read; rewritten if it lies in a row.
/// @param chain The scopes it was read in, innermost first.
void underRowKey(std::string& path, const Scope* chain) {
	for(const Scope* scope = chain; scope; scope = scope->parent) {
		if(scope->place.row == ItemPlace::noRow || !readsRow(path, *scope)) continue;
		const std::size_t at = scope->listPlace().size() + 1U;
		if(std::string_view(path).substr(at) == indexKey)
			path.insert(at, std::string(rowKey) + std::to_string(scope->place.row) + '/');
		else path.insert(at, rowKey);
	}
}

/// @brief Whether a read lies in what a row of the chain numbers.
/// @param path The read.
/// @param chain The scopes, innermost first.
/// @return true if it does.
bool inRow(std::string_view path, const Scope* chain) {
	for(const Scope* scope = chain; scope; scope = scope->parent)
		if(scope->place.row != ItemPlace::noRow && readsRow(path, *scope)) return true;
	return false;
}

} // namespace

std::string layerPlace(std::uint32_t layer) { return std::string(places::scopes) + "/" + std::to_string(layer); }

void LayerReads::expand(ReadSet& reads) const {
	if(prefix.empty()) return;
	const std::string_view whole = std::string_view(prefix).substr(0, prefix.size() - 1U);
	ReadSet result;
	if(reads.always) result.addAlways(reads.why);
	for(const std::string& path : reads.paths) {
		if(path == whole) { // collapsed to the layer (ReadSet::collapse()): all of its variables
			for(const auto& [name, read] : byName) result.add(read);
			continue;
		}
		if(!path.starts_with(prefix)) {
			result.addPath(path);
			continue;
		}
		const std::string_view rest = std::string_view(path).substr(prefix.size());
		const std::string_view key = rest.substr(0, rest.find('/'));
		const auto variable = std::ranges::find(byName, key, &std::pair<std::string, ReadSet>::first);
		if(variable != byName.end()) result.add(variable->second);
		else result.addAlways(AlwaysReason::Behaviour); // anyKey, after a collapse: read again on every update
	}
	reads = std::move(result);
}

bool ScopeLayer::tracks(std::string_view variable) const noexcept {
	return std::ranges::find(tracked_, variable) != tracked_.end();
}

void ScopeLayer::evaluate(LayerReads& reads) const {
	const DocumentContext& document = owner_.engine().document;
	Context context = document.context(*scope_, emptyObject()); // reads the layer as it grows
	const std::array<std::pair<const std::vector<CompiledVariable>*, std::string_view>, 2> lists{
	    {{&node_->with, "with"}, {&node_->let, "let"}}};
	for(const auto& [variables, key] : lists)
		for(const CompiledVariable& variable : *variables) { // in order: each sees the ones before it
			if(!variable.value.dynamic()) {
				scope_->vars[variable.name] = variable.value.written();
				continue;
			}
			ReadSet read;
			{
				const DocumentContext::ReadScope reading(document, &read); // what the lookups record
				context.reads = &read;                                     // what the expression records
				scope_->vars[variable.name] = variable.value.evaluate(context, at_, key, variable.name);
			}
			reads.expand(read); // what it read of the ones before it: what those read
			std::string name;
			appendKey(name, variable.name);
			name.erase(0, 1); // the key, without its '/'
			const auto known = std::ranges::find(reads.byName, name, &std::pair<std::string, ReadSet>::first);
			if(known != reads.byName.end()) known->second = std::move(read);
			else reads.byName.emplace_back(std::move(name), std::move(read));
		}
	const Scope* const rows = scope_->parent;
	for(const auto& [name, read] : reads.byName) {
		if(read.always) reads.rows.addAlways(read.why);
		for(const std::string& path : read.paths)
			if(inRow(path, rows)) reads.rows.addPath(path);
	}
}

void ScopeLayer::read(const LayerReads& reads) const {
	ReadSet all;
	for(const auto& [name, read] : reads.byName) all.add(read);
	for(std::string& path : all.paths) underRowKey(path, scope_->parent);
	owner_.engine().tracker.setReads(id_, std::move(all));
}

bool ScopeLayer::rowsMayMove(const StateChanges& changes) const {
	for(const Scope* scope = scope_->parent; scope; scope = scope->parent) {
		if(scope->place.row == ItemPlace::noRow) continue;
		const std::string list = scope->listPlace();
		if(std::ranges::any_of(changes.paths, [&](const std::string& path) { return contains(path, list); }) ||
		   std::ranges::any_of(changes.lists, [&](const ListChange& change) { return contains(change.place, list); }))
			return true;
	}
	return false;
}

void ScopeLayer::markChanged(const json& old) const {
	DocumentContext& document = owner_.engine().document;
	for(const std::string& variable : tracked_) {
		const auto before = old.find(variable);
		const auto now = scope_->vars.find(variable);
		if(before != old.end() && now != scope_->vars.end() && *before == *now) continue;
		std::string place = layerPlace(number_);
		appendKey(place, variable);
		document.markPlaceChanged(place);
	}
}

// What it read of a repeat's rows went to the rows' reads as it was built: when it reads more of them now, or such a
// row may move in this update (it would read its item where the row was), the full pass builds them as before
void ScopeLayer::update(const StateChanges& changes) {
	if(!scope_) return; // its scope died: swept at the next make()
	DocumentContext& document = owner_.engine().document;
	if(!changes.all && !rows_.paths.empty() && rowsMayMove(changes)) {
		document.requestUpdate("a variable reads a repeat's row that may move");
		return;
	}
	json old = std::move(scope_->vars);
	scope_->vars = json::object();
	LayerReads reads{.prefix = layerPlace(number_) + "/"};
	try {
		evaluate(reads);
	} catch(const DocumentError&) {
		scope_->vars = std::move(old); // the full pass builds the instance as before, and reports what is wrong
		if(!changes.all) document.requestUpdate("a variable can't be evaluated");
		return;
	}
	markChanged(old);
	read(reads);
	const bool rowsCovered = std::ranges::all_of(reads.rows.paths, [this](const std::string& path) {
		return std::ranges::any_of(rows_.paths, [&](const std::string& known) { return contains(known, path); });
	});
	if((!rowsCovered || (reads.rows.always && !rows_.always)) && !changes.all) {
		rows_ = std::move(reads.rows); // the full pass gives them to the row
		document.requestUpdate("a variable reads more of a repeat's row");
	}
}

ScopeLayers::ScopeLayers(Engine& engine) : engine_(engine), layers_(1), alive_(std::make_shared<ScopeLayers*>(this)) {}

ScopeLayers::~ScopeLayers() { alive_.reset(); } // the scopes left see it: they give nothing back

bool ScopeLayers::tracked(const CompiledNode& node) {
	if(node.own.contains("check") || node.malformedWith || node.malformedLet) return false;
	const auto dynamic = [](const CompiledVariable& variable) { return variable.value.dynamic(); };
	return std::ranges::any_of(node.with, dynamic) || std::ranges::any_of(node.let, dynamic);
}

void ScopeLayers::make(Scope& scope, const CompiledRef& node, const std::string& at, LayerReads& reads) {
	sweep();
	std::uint32_t number = 0;
	if(!free_.empty()) {
		number = free_.front();
		free_.pop_front();
	} else {
		number = static_cast<std::uint32_t>(layers_.size());
		layers_.emplace_back();
	}
	std::unique_ptr<ScopeLayer>& slot = layers_[number];
	slot = std::make_unique<ScopeLayer>(*this, number, ++sequence_);
	ScopeLayer& layer = *slot;
	layer.scope_ = &scope;
	layer.node_ = node;
	layer.at_ = at;
	for(const std::vector<CompiledVariable>* variables : {&node->with, &node->let})
		for(const CompiledVariable& variable : *variables)
			if(variable.value.dynamic() && !layer.tracks(variable.name)) layer.tracked_.push_back(variable.name);
	scope.layer = number;
	scope.tracking = &layer;
	scope.hold = std::shared_ptr<void>(&layer, [alive = std::weak_ptr<ScopeLayers*>(alive_), number](void*) noexcept {
		if(const std::shared_ptr<ScopeLayers*> owner = alive.lock()) (*owner)->release(number);
	});
	layer.id_ = engine_.tracker.add(layer);
	reads.prefix = layerPlace(number) + "/";
	layer.evaluate(reads);
	layer.rows_ = reads.rows;
	layer.read(reads);
}

void ScopeLayers::release(std::uint32_t number) noexcept {
	ScopeLayer& layer = *layers_[number];
	layer.scope_ = nullptr;
	layer.nextReleased_ = released_; // a list through the layers: giving one back allocates nothing
	released_ = number;
}

void ScopeLayers::sweep() {
	while(released_ != 0) {
		const std::uint32_t number = released_;
		released_ = layers_[number]->nextReleased_;
		engine_.tracker.remove(layers_[number]->id_);
		layers_[number].reset();
		free_.push_back(number);
	}
}

} // namespace affresco::format
