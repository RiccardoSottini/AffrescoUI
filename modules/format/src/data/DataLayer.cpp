/// @file
/// @brief format::DataLayer: the store a screen's records live in, the `data` variable and the data rules of writes
/// (DataLayer.hpp).

#include "DataLayer.hpp"

#include "Engine.hpp"

#include "affresco/core/DocumentRules.hpp"
#include "affresco/format/Evaluate.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

/// @brief A section of the document, or null.
/// @param document The document.
/// @param name The section.
/// @return It.
const json& sectionOf(const json& document, const char* name) {
	static const json none;
	const auto it = document.find(name);
	return it == document.end() ? none : *it;
}

/// @brief Whether a data rule's author covers an author: "*", the name, or its channel ("agent" covers
/// "agent/claude-code").
/// @param rule The rule's author.
/// @param author The author.
/// @return true if it does.
bool covers(const std::string& rule, const std::string& author) {
	return rule == "*" || rule == author ||
	       (author.starts_with(rule) && author.size() > rule.size() && author[rule.size()] == '/');
}

/// @brief Whether a path pattern of a data rule covers a collection's records, `/data/<collection>`.
/// @param pattern The pattern: "/data", "/data/*", "/data/songs", "*".
/// @param collection The collection.
/// @return true if it does.
bool coversRecords(const std::string& pattern, const std::string& collection) {
	std::string_view p = pattern;
	if(p.ends_with("/**")) p.remove_suffix(3);
	return p == "*" || p == "/data" || p == "/data/*" || p == "/data/" + collection;
}

/// @brief Whether a list of patterns covers a collection's records.
/// @param patterns The patterns, or null.
/// @param collection The collection.
/// @return true if one does.
bool anyCovers(const json& patterns, const std::string& collection) {
	if(!patterns.is_array()) return false;
	return std::ranges::any_of(patterns, [&](const json& p) { return p.is_string() && coversRecords(p, collection); });
}

} // namespace

DataLayer::DataLayer(Engine& engine) : engine_(engine), own_(std::make_unique<data::Store>()) {
	store_ = own_.get();
	store_->setEvaluator(evaluate);
	watch(*store_);
	follow();
}

DataLayer::~DataLayer() {
	if(watched_) watched_->onChange.disconnect(connection_);
}

json DataLayer::evaluate(const std::string& expression, const json& variables) {
	return evaluateExpression(json(expression), variables, "data");
}

void DataLayer::watch(data::Store& store) {
	if(watched_) watched_->onChange.disconnect(connection_);
	watched_ = &store;
	connection_ = store.onChange.connect([this](const std::string&) {
		++storeVersion_;
		engine_.document.requestUpdate();
		engine_.reconciler.requestReconcile();
	});
}

void DataLayer::setStore(data::Store* store, const json* oldState) {
	std::unique_ptr<data::Store> own = store ? nullptr : std::make_unique<data::Store>();
	data::Store& next = store ? *store : *own;
	next.setEvaluator(evaluate);
	(void)next.follow(sectionOf(engine_.versions.document(), "models"), "app", oldState);
	preview_.reset();
	store_ = &next;
	watch(next); // before the old store goes: it disconnects from it
	own_ = std::move(own);
	++storeVersion_;
}

data::Store& DataLayer::store() noexcept { return preview_ ? *preview_ : *store_; }

void DataLayer::follow() {
	const json& models = sectionOf(engine_.versions.document(), "models");
	if(engine_.versions.previewing()) {
		if(!store_->differs(models)) { // the proposal leaves the models alone: the store itself
			if(preview_) {
				preview_.reset();
				watch(*store_);
			}
			return;
		}
		preview_ = store_->copy();
		preview_->setEvaluator(evaluate);
		(void)preview_->follow(models, "preview");
		watch(*preview_);
		return;
	}
	if(preview_) {
		preview_.reset();
		watch(*store_);
	}
	if(store_->differs(models)) (void)store_->follow(models, engine_.versions.head().author);
}

void DataLayer::documentChanged() {
	follow();
	++storeVersion_;
}

const json* DataLayer::variable() {
	const std::uint64_t state = engine_.document.stateVersion();
	if(builtStore_ == storeVersion_ && builtState_ == state) return &value_;
	if(building_) return nullptr; // a query's declaration reads `data` itself
	building_ = true;
	struct Done {
		bool& flag; ///< The flag to clear.
		~Done() { flag = false; }
	} done{building_};
	data::Store& store = this->store();
	if(builtStore_ != storeVersion_) { // every collection's records read again, every query run again
		value_ = json::object();
		queryArguments_.clear();
		for(const auto& [name, model] : store.models()) {
			try {
				value_[name] = store.run({{"from", name}}, "data." + name);
			} catch(const DocumentError& error) {
				engine_.report(error.what());
				value_[name] = json::array();
			}
		}
	}
	builtStore_ = storeVersion_;
	builtState_ = state;
	const json& queries = sectionOf(engine_.versions.document(), "queries");
	if(!queries.is_object()) return &value_;
	for(const auto& [name, declaration] : queries.items()) {
		if(store.models().contains(name)) continue; // a collection's name: refused by the rules
		json arguments;
		try {
			arguments = engine_.document.evaluate(declaration, Scope{}, json::object(), "queries." + name);
		} catch(const DocumentError& error) {
			engine_.report(error.what());
			continue;
		}
		const auto last = queryArguments_.find(name);
		if(last != queryArguments_.end() && last->second == arguments) continue; // the same results
		try {
			value_[name] = store.run(arguments, "queries." + name);
		} catch(const DocumentError& error) {
			engine_.report(error.what());
			value_[name] = json::array();
		}
		queryArguments_[name] = std::move(arguments);
	}
	return &value_;
}

json DataLayer::outline() const {
	json queries = json::array();
	const json& declared = sectionOf(engine_.versions.document(), "queries");
	if(declared.is_object())
		for(const auto& [name, query] : declared.items()) queries.push_back(name);
	return {{"collections", store_->outline()}, {"queries", queries}};
}

void DataLayer::allowDirectory(const std::filesystem::path& directory) {
	std::error_code error;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(directory, error);
	if(!error) allowed_.push_back(canonical);
}

std::filesystem::path DataLayer::allowedFile(const std::string& file, const std::string& where) const {
	const std::filesystem::path written(file);
	if(file.empty() || written.is_absolute() || written.has_root_name())
		fail(where, "expected a file inside the screen's folder, written relative to it");
	std::vector<std::filesystem::path> roots = allowed_;
	if(!engine_.document.baseDirectory().empty()) roots.insert(roots.begin(), engine_.document.baseDirectory());
	for(const std::filesystem::path& root : roots) {
		std::error_code error;
		const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
		if(error) continue;
		const std::filesystem::path path = std::filesystem::weakly_canonical(base / written, error);
		if(error) continue;
		const auto [end, inside] = std::mismatch(base.begin(), base.end(), path.begin(), path.end());
		if(end == base.end() && std::filesystem::exists(path, error)) return path;
	}
	fail(where, "'" + file + "' isn't a file in the screen's folder or one the host allows");
}

void DataLayer::checkWrite(const std::string& collection, const std::string& where) const {
	if(author_ == "app") return; // the person using the app
	const json& permissions = sectionOf(engine_.versions.document(), "permissions");
	const json& rules = permissions.is_object() ? sectionOf(permissions, "data") : permissions;
	if(!rules.is_array()) return;
	for(const json& rule : rules) {
		if(!rule.is_object() || !covers(rule.value("author", std::string("*")), author_)) continue;
		if(anyCovers(rule.value("never", json()), collection))
			fail(where, "data rule: '" + author_ + "' may never touch /data/" + collection);
		if(rule.contains("write") && !anyCovers(rule["write"], collection))
			fail(where, "data rule: '" + author_ + "' may not write /data/" + collection);
	}
}

} // namespace affresco::format
