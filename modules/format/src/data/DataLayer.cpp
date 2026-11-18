/// @file
/// @brief format::DataLayer: the store a screen's records live in, the `data` variable and the data rules of writes
/// (DataLayer.hpp).

#include "DataLayer.hpp"

#include "Engine.hpp"

#include "affresco/core/DocumentRules.hpp"
#include "affresco/format/Evaluate.hpp"

#include <algorithm>
#include <format>

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

/// @brief Raises a flag while it lives (a variable being built).
class Raised {
public:
	/// @brief Raises it.
	/// @param flag The flag; outlives this.
	explicit Raised(bool& flag) noexcept : flag_(flag) { flag_ = true; }
	/// @brief Lowers it.
	~Raised() { flag_ = false; }
	Raised(const Raised&) = delete;
	Raised& operator=(const Raised&) = delete;
	Raised(Raised&&) = delete;
	Raised& operator=(Raised&&) = delete;

private:
	bool& flag_; ///< The flag.
};

} // namespace

std::string dataPlace(std::string_view name) {
	std::string place(dataPlaces);
	appendKey(place, name);
	return place;
}

DataLayer::DataLayer(Engine& engine)
    : engine_(engine), own_(std::make_unique<data::Store>()), store_(own_.get()),
      queriesId_(engine.tracker.add(queries_)) {
	store_->setEvaluator(evaluate);
	watch(*store_);
	follow();
	ReadSet reads;
	reads.addPath(dataPlaces);
	engine_.tracker.setReads(queriesId_, std::move(reads));
}

DataLayer::~DataLayer() {
	engine_.tracker.remove(queriesId_);
	if(watched_) watched_->onChange.disconnect(connection_);
}

void DataLayer::Queries::update(const StateChanges& /*changes*/) { (void)layer_.variable(); }

std::uint64_t DataLayer::Queries::order() const noexcept { return queriesOrder; }

json DataLayer::evaluate(const std::string& expression, const json& variables) {
	return evaluateExpression(json(expression), variables, "data");
}

void DataLayer::watch(data::Store& store) {
	if(watched_) watched_->onChange.disconnect(connection_);
	watched_ = &store;
	connection_ = store.onChange.connect([this](const std::string& collection) { recordsChanged(collection); });
}

void DataLayer::recordsChanged(const std::string& collection) {
	if(collection.empty()) allChanged_ = true;
	else changed_.insert(collection);
	const std::string place = collection.empty() ? std::string(dataPlaces) : dataPlace(collection);
	engine_.document.markPlaceChanged(place); // what reads its records, and the queries (Queries reads "/~d")
	engine_.computed.placeChanged(place);     // computed values that read them are stale
	for(const auto& [name, arguments] : queryArguments_) { // and those reading a query over them, read before an update
		const json& from = arguments.is_object() ? sectionOf(arguments, "from") : arguments;
		if(collection.empty() || !from.is_string() || from.get_ref<const std::string&>() == collection)
			engine_.computed.placeChanged(dataPlace(name));
	}
	engine_.reconciler.requestStateUpdate();
}

void DataLayer::markChanged(std::string_view name) {
	const std::string place = dataPlace(name);
	engine_.document.markPlaceChanged(place);
	engine_.computed.placeChanged(place);
}

void DataLayer::setStore(data::Store* store, const json* oldState) {
	eventBatch_.reset();
	std::unique_ptr<data::Store> own = store ? nullptr : std::make_unique<data::Store>();
	data::Store& next = store ? *store : *own;
	next.setEvaluator(evaluate);
	(void)next.follow(sectionOf(engine_.versions.document(), "models"), "app", oldState);
	preview_.reset();
	store_ = &next;
	watch(next); // before the old store goes: it disconnects from it
	own_ = std::move(own);
	++storeVersion_;
	recordsChanged({}); // every record may differ
}

void DataLayer::groupEvent() {
	if(eventBatch_ || !engine_.window) return;
	eventBatch_ = std::make_unique<data::Store::Batch>(store());
	engine_.scheduler.defer([this] { eventBatch_.reset(); });
}

data::Store& DataLayer::store() noexcept { return preview_ ? *preview_ : *store_; }

void DataLayer::follow() {
	eventBatch_.reset(); // a version lands between events
	const json& models = sectionOf(engine_.versions.document(), "models");
	if(preview_) { // the last preview's copy goes: stop watching it first
		watch(*store_);
		preview_.reset();
	}
	if(engine_.versions.previewing()) {
		if(!store_->differs(models)) return; // the proposal leaves the models alone: the store itself
		std::unique_ptr<data::Store> copy = store_->copy();
		copy->setEvaluator(evaluate);
		(void)copy->follow(models, "preview"); // refused: the preview is, and the store is untouched
		preview_ = std::move(copy);
		watch(*preview_);
		return;
	}
	if(store_->differs(models)) (void)store_->follow(models, engine_.versions.head().author);
}

void DataLayer::documentChanged() {
	follow();
	++storeVersion_;
}

const json* DataLayer::variable() {
	const std::uint64_t state = engine_.document.stateVersion();
	const bool rebuilt = builtStore_ != storeVersion_;
	if(!rebuilt && !allChanged_ && changed_.empty() && builtState_ == state) return &value_;
	if(building_) return nullptr; // a query's declaration reads `data` itself
	const Raised building(building_);
	const data::Store& store = this->store();
	std::set<std::string, std::less<>> changed;
	if(rebuilt) { // every collection's records read again, every query run again
		value_ = json::object();
		queryArguments_.clear();
	}
	for(const auto& [name, model] : store.models()) {
		if(!rebuilt && !allChanged_ && !changed_.contains(name)) continue; // its records are the same
		try {
			value_[name] = store.run({{"from", name}}, "data." + name);
		} catch(const DocumentError& error) {
			engine_.report(error.what());
			value_[name] = json::array();
		}
		changed.insert(name);
	}
	changed_.clear();
	allChanged_ = false;
	builtStore_ = storeVersion_;
	builtState_ = state;
	runQueries(rebuilt, changed);
	return &value_;
}

void DataLayer::runQueries(bool rebuilt, const std::set<std::string, std::less<>>& changed) {
	const data::Store& store = this->store();
	queryReads_.clear();
	const json& queries = sectionOf(engine_.versions.document(), "queries");
	if(queries.is_object())
		for(const auto& [name, declaration] : queries.items()) {
			if(store.models().contains(name)) continue; // a collection's name: refused by the rules
			json arguments;
			try {
				const DocumentContext::ReadScope reads(engine_.document, &queryReads_); // what the queries read
				arguments = engine_.document.evaluate(declaration, Scope{}, json::object(), "queries." + name);
			} catch(const DocumentError& error) {
				engine_.report(error.what());
				continue;
			}
			const json& from = arguments.is_object() ? sectionOf(arguments, "from") : arguments;
			const bool recordsMoved = !from.is_string() || changed.contains(from.get_ref<const std::string&>());
			const auto last = queryArguments_.find(name);
			if(last != queryArguments_.end() && last->second == arguments && !recordsMoved) continue; // the same
			json results;
			try {
				results = store.run(arguments, "queries." + name);
			} catch(const DocumentError& error) {
				engine_.report(error.what());
				results = json::array();
			}
			const bool differs = !value_.contains(name) || value_[name] != results;
			if(differs) value_[name] = std::move(results);
			if(differs && !rebuilt) markChanged(name);
			queryArguments_[name] = std::move(arguments);
		}
	// The queries run again when what they read changes, or a collection's records do. They read each collection's
	// place, not all of dataPlaces: that holds their own results, and marking one that differs would find them again.
	ReadSet reads = queryReads_;
	for(const auto& [name, model] : store.models()) reads.addPath(dataPlace(name));
	engine_.tracker.setReads(queriesId_, std::move(reads));
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
		const auto [end, inside] = std::ranges::mismatch(base, path);
		if(end == base.end() && std::filesystem::exists(path, error)) return path;
	}
	fail(where, "'" + file + "' isn't a file in the screen's folder or one the host allows");
}

const std::string& DataLayer::author() const noexcept {
	// A peer's run's runner, whatever function led here; else who the step authority names first, of those who aren't
	// the person: a record a timer of a peer's run writes later is still the runner's, whatever the host set
	if(engine_.authority && engine_.authority->run) return engine_.authority->run->runner;
	if(engine_.authority)
		for(const std::string& author : engine_.authority->authors)
			if(!isPersonsAuthor(author)) return author;
	return author_;
}

void DataLayer::checkWrite(const std::string& collection, const std::string& where) const {
	if(engine_.authority && engine_.authority->run && engine_.authority->run->refused)
		throw data::DataError("denied", where + ": the run that scheduled this was refused, so it writes no record");
	// The writer, and every other author whose code led here: a function by one may not write for another
	checkWriteAs(author(), collection, where);
	if(engine_.authority)
		for(const std::string& other : engine_.authority->authors)
			if(!isPersonsAuthor(other) && other != author()) checkWriteAs(other, collection, where);
}

void DataLayer::checkWriteAs(const std::string& writer, const std::string& collection, const std::string& where) const {
	if(writer == "app") return; // the person using the app
	const json& permissions = sectionOf(engine_.versions.document(), "permissions");
	const json& rules = permissions.is_object() ? sectionOf(permissions, "data") : permissions;
	if(!rules.is_array()) return;
	for(const json& rule : rules) {
		if(!rule.is_object() || !covers(rule.value("author", std::string("*")), writer)) continue;
		if(anyCovers(rule.value("never", json()), collection))
			throw data::DataError(
			    "denied", std::format("{}: data rule: '{}' may never touch /data/{}", where, writer, collection));
		if(rule.contains("write") && !anyCovers(rule["write"], collection))
			throw data::DataError("denied",
			                      std::format("{}: data rule: '{}' may not write /data/{}", where, writer, collection));
	}
}

} // namespace affresco::format
