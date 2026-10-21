/// @file
/// @brief format::DocumentContext: colours, fonts, the theme, the globals, computed values and navigation.

#include "DocumentContext.hpp"
#include "Engine.hpp"
#include "Limits.hpp"
#include "PropertyValues.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace affresco::format {

namespace {

constexpr std::size_t fallbackLimit = 8; ///< Fallback fonts one font may name.

/// @brief Reads a font's "fallback": the files of the fonts it names, in order. A fallback's own fallbacks are not
/// followed, and its size is not used: its glyphs are drawn at the size of the font that names it.
/// @param fonts The "fonts" section.
/// @param font The font's entry.
/// @param path The entry's place, for errors.
/// @param base The directory the files are relative to.
/// @return The files; none without "fallback".
/// @throws DocumentError If it isn't a list of names of fonts in the section, or names more than eight.
std::vector<std::filesystem::path> fallbackFiles(const json& fonts, const json& font, const std::string& path,
                                                 const std::filesystem::path& base) {
	std::vector<std::filesystem::path> files;
	if(!font.contains("fallback")) return files;

	const std::string at = path + ".fallback";
	const json& names = font["fallback"];
	if(!names.is_array()) fail(at, "expected a list of font names, like [\"symbols\"]");
	if(names.size() > fallbackLimit) fail(at, "at most " + std::to_string(fallbackLimit) + " fallback fonts");

	for(std::size_t i = 0; i < names.size(); ++i) {
		const std::string place = at + "[" + std::to_string(i) + "]";
		const std::string name = string(names[i], place);
		if(!fonts.contains(name) || !fonts[name].is_object() || !fonts[name].contains("file"))
			fail(place, "unknown font '" + name + "'");
		files.push_back(base / string(fonts[name]["file"], "fonts." + name + ".file"));
	}
	return files;
}

/// @brief Reads a font's "size": a number, or "@name" of a number token (in the base theme: fonts don't switch).
/// @param value The value.
/// @param path Its place, for errors.
/// @param tokens The number tokens.
/// @return The size.
/// @throws DocumentError If it is neither.
float fontSize(const json& value, const std::string& path, const NumberTable& tokens) {
	if(value.is_string() && value.get_ref<const std::string&>().starts_with('@')) {
		const auto it = tokens.find(std::string_view(value.get_ref<const std::string&>()).substr(1));
		if(it == tokens.end()) fail(path, "unknown number token '" + value.get<std::string>() + "'");
		return it->second.base;
	}
	return number(value, path);
}

/// @brief Reads a hex colour.
/// @param hex The value.
/// @param at Its place, for errors.
/// @return The colour.
/// @throws DocumentError If it isn't one.
Color hexColor(const json& hex, const std::string& at) {
	const auto parsed = Color::parseHex(string(hex, at));
	if(!parsed) fail(at, "expected a hex color like \"#6366f1\"");
	return *parsed;
}

} // namespace

DocumentContext::DocumentContext(Engine& engine, std::filesystem::path baseDirectory)
    : engine_(engine), baseDirectory_(std::move(baseDirectory)),
      computedLookup_([this](std::string_view name) { return computed(name); }),
      nodeLookup_([this](std::string_view id) { return node(id); }),
      modelLookup_([this](std::string_view place) { return engine_.models.at(place); }),
      variableLookup_([this](std::string_view name) { return variable(name); }) {
	if(!baseDirectory_.empty()) images_.allowDirectory(baseDirectory_);
}

std::filesystem::path DocumentContext::imageFile(const json& value, const std::string& path) const {
	const std::filesystem::path file = baseDirectory_ / string(value, path);
	// A revision may change the path: it must not show a picture from anywhere on the disk
	if(!images_.allows(file))
		fail(path, "the file is outside the screen's folder and the folders the host allows: " + file.string());
	return file;
}

std::map<std::string, DocumentContext::NamedColor, std::less<>> DocumentContext::readColors(const json& doc) {
	std::map<std::string, NamedColor, std::less<>> result;
	if(!doc.contains("colors")) return result;
	for(const auto& [name, value] : object(doc["colors"], "colors").items()) {
		const std::string path = "colors." + name;
		NamedColor named;
		if(value.is_object()) { // per theme: {"light": "#ffffff", "dark": "#111827"}
			if(value.empty()) fail(path, "expected at least one theme");
			for(const auto& [themeName, hex] : value.items())
				named.themes.emplace(themeName, hexColor(hex, std::format("{}.{}", path, themeName)));
			named.base = named.themes.at(value.begin().key());
		} else {
			named.base = hexColor(value, path);
		}
		result.emplace(name, std::move(named));
	}
	return result;
}

// Throws, changing nothing, if a color or a font is invalid
void DocumentContext::readDerived() {
	const json& doc = engine_.versions.document();
	auto newColors = readColors(doc);
	auto newNumbers = readTokens(doc, newColors);
	decltype(fonts_) newFonts;
	decltype(fontSources_) newSources;

	if(doc.contains("fonts")) {
		for(const auto& [name, value] : object(doc["fonts"], "fonts").items()) {
			const std::string path = "fonts." + name;
			object(value, path);
			if(!value.contains("file") || !value.contains("size")) fail(path, R"(a font needs "file" and "size")");

			const std::string file = (baseDirectory_ / string(value["file"], path + ".file")).string();
			const float size = fontSize(value["size"], path + ".size", newNumbers);
			const std::vector<std::filesystem::path> fallbacks =
			    fallbackFiles(doc["fonts"], value, path, baseDirectory_);
			try {
				std::shared_ptr<const Font> font = fontCache_.load(file, size, fallbacks);
				newSources[font->id()] = {file, fallbacks};
				newFonts.emplace(name, std::move(font));
			} catch(const std::runtime_error& error) {
				fail(path, error.what());
			}
		}
	}

	colors_ = std::move(newColors);
	numberTokens_ = std::move(newNumbers);
	fonts_ = std::move(newFonts);
	fontSources_ = std::move(newSources);
	variants_.clear();
	changeAppearance();
}

/* Values */

const DocumentContext::Globals& DocumentContext::globals() const {
	if(!globalsCache_) {
		Globals& g = globalsCache_.emplace();
		const VersionControl& versions = engine_.versions;
		const Window* window = engine_.window;
		g.window = window ? json{{"width", window->width()},
		                         {"height", window->height()},
		                         {"scale", window->contentScale()},
		                         {"colorScheme", window->colorScheme() == ColorScheme::Dark ? "dark" : "light"},
		                         {"nativeMenuBar", window->nativeMenuBar()},
		                         {"reducedMotion", window->reducedMotion()}}
		                  : json::object();
		g.revision = toJson(versions.head());

		json list = json::array();
		for(const ProposalInfo& p : versions.proposals()) list.push_back(toJson(p));
		json log = json::array();
		for(const CommitInfo& c : versions.log()) log.push_back(toJson(c));

		const auto previewing = versions.previewing();
		g.other = {{"proposals", list},
		           {"history", log},
		           {"preview", previewing ? json(*previewing) : json()},
		           {"mode", toString(versions.mode())},
		           {"theme", theme_},
		           {"screen", screen_}};
	}

	return *globalsCache_;
}

Context DocumentContext::context(const Scope& locals, const json& event) const {
	const Globals& g = globals();
	return Context{engine_.versions.state(), g.window, g.revision, event, locals, g.other, &nodeLookup_,
	               &computedLookup_, budget_, reads_, &modelLookup_, &variableLookup_};
}

void DocumentContext::addVariable(std::string name, std::function<json()> build) {
	builders_.insert_or_assign(std::move(name), std::move(build));
	++variablesEpoch_;
}

const json* DocumentContext::variable(std::string_view name) const {
	const auto builder = builders_.find(name);
	if(builder == builders_.end()) return nullptr;
	if(building_.empty() && (builtEpoch_ != variablesEpoch_ || builtStateVersion_ != stateVersion_)) {
		built_.clear(); // only between builds: a value read in an evaluation stays valid until it ends
		builtEpoch_ = variablesEpoch_;
		builtStateVersion_ = stateVersion_;
	}
	// a variable is built from what no state change announces (the focus, a chord): what reads it reads always
	if(reads_) reads_->always = true;
	if(const auto found = built_.find(name); found != built_.end()) return &found->second;
	if(std::ranges::find(building_, name) != building_.end()) return nullptr; // it reads itself
	building_.emplace_back(name);
	json value;
	try {
		value = builder->second();
	} catch(...) {
		building_.pop_back();
		throw;
	}
	building_.pop_back();
	return &built_.insert_or_assign(std::string(name), std::move(value)).first->second;
}

/* The work budget */

DocumentContext::BudgetScope::BudgetScope(DocumentContext& context) noexcept : context_(context) {
	if(context_.budget_) return; // inside another event or update: its budget goes on
	budget_ = {stepsPerEvent, operationsPerEvent, bytesPerEvent};
	context_.budget_ = &budget_;
	outermost_ = true;
}

DocumentContext::BudgetScope::~BudgetScope() {
	if(outermost_) context_.budget_ = nullptr;
}

void DocumentContext::spendStep(const std::string& where) const {
	if(!budget_) return;
	if(budget_->steps == 0)
		fail(where, "the event ran more than " + std::to_string(stepsPerEvent) +
		                " steps, its work budget: the action stopped");
	--budget_->steps;
}

void DocumentContext::spendBytes(const json& value, const std::string& where) const {
	if(!budget_) return;
	const std::size_t bytes = approximateBytes(value, budget_->bytes);
	if(bytes > budget_->bytes)
		fail(where, "the event wrote more than " + std::to_string(bytesPerEvent >> 20) +
		                " MiB into the state, its work budget: the action stopped");
	budget_->bytes -= bytes;
}

json DocumentContext::evaluate(const json& value, const Scope& locals, const json& event,
                               const std::string& where) const {
	return format::evaluate(value, context(locals, event), where);
}

json DocumentContext::computed(std::string_view name) const {
	const json& document = engine_.versions.document();
	const std::string key(name);
	if(!document.contains("computed") || !document["computed"].contains(key))
		fail("computed", "no computed value '" + key + "'");
	if(computedVersion_ != stateVersion_) {
		computedValues_ = json::object();
		computedReads_.clear();
		computedVersion_ = stateVersion_;
	}
	if(computedValues_.contains(key)) {
		if(reads_) reads_->add(computedReads_[key]); // what reads it reads what it read
		return computedValues_[key];
	}
	if(std::ranges::find(computing_, key) != computing_.end())
		fail("computed." + key, "computed values refer to each other in a circle");
	if(computing_.size() >= maxComputedDepth)
		fail("computed." + key, "computed values read each other too deeply (more than 32 levels)");
	computing_.push_back(key);
	json value;
	ReadSet read;
	try {
		const ReadScope scope(*this, &read);
		const json none = json::object();
		value = format::evaluate(document["computed"][key], context(*noVariables(), none), "computed." + key);
	} catch(...) {
		computing_.pop_back();
		throw;
	}
	computing_.pop_back();
	if(reads_) reads_->add(read);
	computedValues_[key] = value;
	computedReads_[key] = std::move(read);
	return value;
}

json DocumentContext::node(std::string_view id) const {
	const Panel* panel = engine_.window ? engine_.window->root().findById(id) : nullptr;
	if(!panel) return nullptr;
	const Rect& f = panel->frame();
	return {{"x", f.left}, {"y", f.top}, {"width", f.width()}, {"height", f.height()}, {"visible", panel->isShown()}};
}

/* Colours, fonts, theme */

// "#rrggbb", "@name" (from "colors", in the current theme) or "transparent"
Color DocumentContext::color(const json& value, const std::string& path) const {
	const std::string name = string(value, path);
	if(name == "transparent") return colors::transparent;

	if(name.starts_with('@')) {
		const auto it = colors_.find(std::string_view(name).substr(1));
		if(it == colors_.end()) fail(path, "unknown color '" + name + "'");
		return it->second.in(theme_);
	}

	const auto parsed = Color::parseHex(name);
	if(!parsed) fail(path, R"(expected a color: "#rrggbb", "#rrggbbaa", "@name" or "transparent")");
	return *parsed;
}

Color DocumentContext::namedColor(std::string_view name) const {
	const auto it = colors_.find(name);
	if(it == colors_.end()) throw DocumentError("AffrescoUI screen: unknown color '" + std::string(name) + "'");
	return it->second.in(theme_);
}

std::optional<float> DocumentContext::numberToken(std::string_view name) const {
	const auto it = numberTokens_.find(name);
	if(it == numberTokens_.end()) return std::nullopt;
	return it->second.in(theme_);
}

const json& DocumentContext::resolveToken(const json& value, json& scratch) const {
	if(!value.is_string()) return value;
	const auto& text = value.get_ref<const std::string&>();
	if(!text.starts_with('@')) return value;
	const auto number = numberToken(std::string_view(text).substr(1));
	if(!number) return value;
	scratch = *number;
	return scratch;
}

const json& DocumentContext::resolveMemberTokens(const json& value, json& scratch) const {
	bool copied = false;
	for(const auto& [key, member] : value.items()) {
		json number;
		const json& resolved = resolveToken(member, number);
		if(&resolved == &member) continue;
		if(!std::exchange(copied, true)) scratch = value;
		scratch[key] = std::move(number);
	}
	// NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter): like resolveToken(), callers pass the document's values
	return copied ? scratch : value;
}

// Like CSS box-shadow: "x" and "y" move it and "blur" softens it
Shadow DocumentContext::shadow(const json& value, const std::string& path) const {
	Shadow result{.color = colors::black.withAlpha(0.25f)};
	for(const auto& [key, item] : object(value, path).items()) {
		const std::string at = std::format("{}.{}", path, key);
		if(key == "x") result.offset.x = finiteNumber(item, at);
		else if(key == "y") result.offset.y = finiteNumber(item, at);
		else if(key == "blur") result.blur = pixels(item, at);
		else if(key == "color") result.color = color(item, at);
		else fail(at, "unknown property (expected x, y, blur or color)");
	}
	return result;
}

std::shared_ptr<const Font> DocumentContext::findFont(std::string_view name) const {
	const auto it = fonts_.find(name);
	return it == fonts_.end() ? nullptr : it->second;
}

std::shared_ptr<const Font> DocumentContext::fontVariant(const Font* base, float pixelHeight, FontStyle style,
                                                         const std::string& at) {
	constexpr std::size_t variantLimit = 32;
	if(!base) {
		const auto found = fonts_.find("default");
		if(found == fonts_.end()) return nullptr;
		base = found->second.get();
	}
	const auto source = fontSources_.find(base->id());
	if(source == fontSources_.end()) return nullptr;
	if(pixelHeight == base->pixelHeight() && style == base->style())
		return fontCache_.load(source->second.file, pixelHeight, source->second.fallbacks);
	const std::string key = std::format("{}/{}/{}{}", base->id(), pixelHeight, style.bold, style.italic);
	if(const auto found = variants_.find(key); found != variants_.end()) return found->second;
	if(variants_.size() >= variantLimit)
		fail(at, std::format("at most {} sizes and styles of fonts in a document", variantLimit));
	try {
		auto font = fontCache_.load(source->second.file, pixelHeight, source->second.fallbacks, style);
		return variants_.emplace(key, std::move(font)).first->second;
	} catch(const std::runtime_error& error) {
		fail(at, error.what());
	}
}

void DocumentContext::updateTheme() {
	const json& document = engine_.versions.document();
	std::string theme =
	    document.contains("theme") ? text(evaluate(document["theme"], *noVariables(), json::object(), "theme")) : "";
	if(document.contains("theme")) globalsChanged();
	if(theme == theme_) return;
	theme_ = std::move(theme);
	changeAppearance();
}

/* Navigation */

void DocumentContext::keepScreen(const json& document) {
	if(document.contains("screens") && document["screens"].is_object() && !document["screens"].empty() &&
	   !document["screens"].contains(screen_)) {
		screen_ = document["screens"].begin().key();
		screenHistory_.clear();
	}
}

void DocumentContext::navigate(const std::string& name) {
	if(name != screen_) screenHistory_.push_back(screen_);
	screen_ = name;
}

bool DocumentContext::back() {
	if(screenHistory_.empty()) return false;
	screen_ = screenHistory_.back();
	screenHistory_.pop_back();
	return true;
}

} // namespace affresco::format
