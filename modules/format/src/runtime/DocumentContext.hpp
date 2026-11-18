#pragma once

/// @file
/// @brief format::DocumentContext: what the document's values are read against: colours, fonts, the theme, the
/// globals and the screen an outlet shows.

#include "DesignTokens.hpp"
#include "HostVariables.hpp"
#include "Language.hpp"
#include "NodeKeys.hpp"
#include "Reads.hpp"
#include "Strings.hpp"

#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/FontCache.hpp"
#include "affresco/render/ImageCache.hpp"
#include "affresco/render/Painter.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief What the document's values are read against: colours, fonts, the theme, the globals of expressions (the
/// cached "computed" values among them, held by ComputedValues), and the screen an outlet shows (navigation).
///
/// Invariants: colours and fonts change all at once or not at all (readDerived()). The globals and built variables
/// are built when read and kept until a place they read is marked (HostVariables); computed values (ComputedValues)
/// are cached per stateVersion(), which every mark of the state, a global or a variable bumps.
/// Callers: every subsystem that evaluates a value (builder, properties, events, steps), the reconciler (theme and
/// screen per update) and Runtime's lookups. Main thread only.
class DocumentContext {
public:
	/// @brief A context for an engine's document.
	/// @param engine The runtime; outlives this object.
	/// @param baseDirectory Where font paths are resolved from.
	DocumentContext(Engine& engine, std::filesystem::path baseDirectory);
	/// @brief Where relative paths are resolved from: the screen's folder (empty for a screen from text without one).
	[[nodiscard]] const std::filesystem::path& baseDirectory() const noexcept { return baseDirectory_; }

	DocumentContext(const DocumentContext&) = delete;
	DocumentContext& operator=(const DocumentContext&) = delete;
	DocumentContext(DocumentContext&&) = delete;
	DocumentContext& operator=(DocumentContext&&) = delete;
	~DocumentContext() = default;

	/// @brief Reads "colors" and "fonts", changing nothing on error.
	/// @throws DocumentError If a colour or font is invalid.
	void readDerived();

	/* Values */

	/// @brief A context for expressions.
	/// @param locals Variables.
	/// @param event The event being handled.
	/// @return The context (it refers to the arguments).
	[[nodiscard]] Context context(const Scope& locals, const json& event) const;
	/// @brief Evaluates a value with the state, the globals and the given variables.
	/// @param value The value.
	/// @param locals Variables.
	/// @param event The event being handled.
	/// @param where Its place, for errors.
	/// @return The evaluated value.
	/// @throws DocumentError If it is invalid.
	[[nodiscard]] json evaluate(const json& value, const Scope& locals, const json& event,
	                            const std::string& where) const;
	/// @brief Makes every global and built variable stale, without marking their places: a change of everything
	/// (a revision, a run's grants), which a full update follows.
	void globalsChanged() const noexcept { values_.everythingChanged(); }
	/// @brief Records that a global changed: it is built again when next read, and what read it is evaluated again.
	/// @param name Its name, one of globalNames ("window", "history", ...).
	void markGlobalChanged(std::string_view name) {
		markPlaceChanged(std::string(places::globals) + "/" + std::string(name));
	}
	/// @brief Records that what a built variable was built from changed, in a way no place of the state describes
	/// (a service's status, a request, the focus): it is built again when next read, and what read it is updated.
	/// @param name Its name ("services", "context", ...).
	void markVariableChanged(std::string_view name) {
		markPlaceChanged(std::string(places::variables) + "/" + std::string(name));
	}
	/// @brief Records that what the versions show changed: the proposals, the history, the revision, the preview,
	/// the mode and the peers. For the version control's callbacks.
	void reviewChanged();
	/// @brief Adds a screen-wide variable built when first read and kept until a place it read changes (`context`,
	/// `commands`, `menus`, ...).
	/// @param name Its name.
	/// @param build Builds its value; it may evaluate the document's values, but not read itself.
	void addVariable(const std::string& name, std::function<json()> build) { values_.add(name, std::move(build)); }
	/// @brief Records that the focus or a chord in progress changed what `context` holds.
	void contextChanged() { markVariableChanged("context"); }
	/// @brief A variable addVariable() added, built if it is stale.
	/// @param name Its name.
	/// @return Its value, valid until it is stale; null if there is no such variable, or it is being built.
	[[nodiscard]] const json* variable(std::string_view name) const;
	/// @brief The globals and variables built when read.
	/// @return They.
	[[nodiscard]] const HostVariables& hostValues() const noexcept { return values_; }
	/// @brief Records that the state changed in a way no place describes: computed values are stale and every value
	/// is evaluated again at the next update.
	void markStateChanged() noexcept {
		requestUpdate();
		++stateVersion_;
		values_.everythingChanged();
	}
	/// @brief Records that one place of the state changed: computed values are stale, and the next update evaluates
	/// again only the values that read it.
	/// @param pointer The place.
	void markStateChanged(const json::json_pointer& pointer) {
		const std::string place = pointer.to_string();
		changes_.add(place);
		++stateVersion_;
		values_.placeMarked(place);
	}
	/// @brief Records that a list of the state changed shape, as markStateChanged(pointer) does for its place, with
	/// how it changed (StateChanges::lists).
	/// @param pointer The list.
	/// @param kind Items inserted or removed.
	/// @param index The first one.
	/// @param count How many.
	void markListChanged(const json::json_pointer& pointer, ListChange::Kind kind, std::size_t index,
	                     std::size_t count = 1) {
		std::string place = pointer.to_string();
		changes_.add(place);
		++stateVersion_;
		values_.placeMarked(place);
		if(!changes_.all) changes_.lists.push_back({std::move(place), kind, index, count});
	}
	/// @brief Records that a place outside the state changed: the next update evaluates again only the values that
	/// read it, and the built variables that read it are stale. A data model's rows ("/~m/<name>/<row>", Models.hpp)
	/// keep computed values, which never read there; a global or a variable ("/~g/...", "/~v/...") makes them stale.
	/// @param place The place.
	void markPlaceChanged(const std::string& place);
	/// @brief Records that a list outside the state changed shape, a data model's rows (Models.hpp), as
	/// markPlaceChanged() does for its place, with how it changed (StateChanges::lists).
	/// @param place The list, "/~m/<name>".
	/// @param kind Rows inserted or removed.
	/// @param index The first one.
	/// @param count How many.
	void markPlaceListChanged(std::string place, ListChange::Kind kind, std::size_t index, std::size_t count) {
		markPlaceChanged(place);
		if(!changes_.all) changes_.lists.push_back({std::move(place), kind, index, count});
	}
	/// @brief Whether the state changed, or an update was requested, since the last update began (takeChanges()).
	[[nodiscard]] bool stateChanged() const noexcept { return !changes_.empty(); }
	/// @brief What changed since the last update began, as an update begins: the state is then up to date with the
	/// panels.
	/// @return The changes.
	[[nodiscard]] StateChanges takeChanges() noexcept { return std::exchange(changes_, {}); }
	/// @brief Why the changes call for a full update, as an update begins: the reason given to requestUpdate().
	/// @return The first reason given since it was last taken; empty if none was.
	[[nodiscard]] std::string_view takeFullReason() noexcept { return std::exchange(fullReason_, {}); }
	/// @brief Marks a full update as due (every value evaluated again), without staling computed values.
	/// @param reason Why, for UpdateStatistics::fullReason: a string that lives as long as the program.
	void requestUpdate(std::string_view reason = "requested") noexcept {
		changes_.addAll();
		if(fullReason_.empty()) fullReason_ = reason;
	}
	/// @brief The changes of the update running, which places marked while it runs join (Tracker).
	/// @return They, or null outside an update.
	[[nodiscard]] const StateChanges* updating() const noexcept { return updating_; }
	/// @brief Holds the changes of the update running, for updating(), while it lives.
	class UpdatingScope {
	public:
		/// @brief Makes an update's changes the running ones.
		/// @param context The context.
		/// @param changes The changes; they outlive the scope.
		UpdatingScope(DocumentContext& context, const StateChanges& changes) noexcept
		    : context_(context), previous_(std::exchange(context.updating_, &changes)) {}
		~UpdatingScope() { context_.updating_ = previous_; }
		UpdatingScope(const UpdatingScope&) = delete;
		UpdatingScope& operator=(const UpdatingScope&) = delete;

	private:
		DocumentContext& context_;     ///< The context.
		const StateChanges* previous_; ///< The running changes before.
	};
	/// @brief Makes computed values stale (a revision may change the state too).
	void bumpStateVersion() noexcept { ++stateVersion_; }
	/// @brief Counts the state's changes: what is cached per version of the state compares it.
	/// @return The count.
	[[nodiscard]] std::uint64_t stateVersion() const noexcept { return stateVersion_; }

	/* What values read */

	/// @brief Records the places of the state read by every value evaluated while it lives (Reads.hpp), so a later
	/// update evaluates them again only when one of those places changed. Nested scopes collect into their own set.
	class ReadScope {
	public:
		/// @brief Starts collecting.
		/// @param context The context; outlives the scope.
		/// @param into Where the places go (nullptr: nowhere, until the scope ends).
		ReadScope(const DocumentContext& context, ReadSet* into) noexcept
		    : context_(context), outer_(std::exchange(context.reads_, into)) {}
		/// @brief Gives the collecting back to the enclosing scope.
		~ReadScope() { context_.reads_ = outer_; }
		ReadScope(const ReadScope&) = delete;
		ReadScope& operator=(const ReadScope&) = delete;
		ReadScope(ReadScope&&) = delete;
		ReadScope& operator=(ReadScope&&) = delete;

	private:
		const DocumentContext& context_; ///< The context.
		ReadSet* outer_;                 ///< The enclosing scope's set, or nullptr.
	};
	/// @brief Where the running ReadScope collects.
	/// @return The set, or nullptr.
	[[nodiscard]] ReadSet* reads() const noexcept { return reads_; }

	/* The work budget */

	/// @brief Gives the event or update it lives for a work budget (Expression.hpp, WorkBudget): every value evaluated
	/// and every step run in it spends from it. Nested scopes share the outermost one's.
	class BudgetScope {
	public:
		/// @brief Starts a budget, unless one is running.
		/// @param context The context; outlives the scope.
		explicit BudgetScope(DocumentContext& context) noexcept;
		/// @brief Ends the budget it started.
		~BudgetScope();
		BudgetScope(const BudgetScope&) = delete;
		BudgetScope& operator=(const BudgetScope&) = delete;
		BudgetScope(BudgetScope&&) = delete;
		BudgetScope& operator=(BudgetScope&&) = delete;

	private:
		DocumentContext& context_; ///< The context.
		WorkBudget budget_;        ///< The budget, if this scope started it.
		bool outermost_ = false;   ///< Whether it did.
	};
	/// @brief The running budget.
	/// @return It, or nullptr with no event or update running.
	[[nodiscard]] WorkBudget* budget() const noexcept { return budget_; }
	/// @brief Spends one step from the running budget.
	/// @param where The step's place, for the error.
	/// @throws DocumentError If the budget has no step left.
	void spendStep(const std::string& where) const;
	/// @brief Spends the bytes of a value a step writes into the state from the running budget.
	/// @param value The value.
	/// @param where The step's place, for the error.
	/// @throws DocumentError If the budget has fewer bytes left.
	void spendBytes(const json& value, const std::string& where) const;

	/* Colours, fonts, theme */

	/// @brief Reads a colour: "#rrggbb", "@name" (from "colors", in the current theme) or "transparent"; null (an
	/// expression whose data isn't there, which the checker reports) is transparent, so the screen still shows.
	/// @param value The value.
	/// @param path Its place, for errors.
	/// @return The colour.
	/// @throws DocumentError If it is not a colour.
	[[nodiscard]] Color color(const json& value, const std::string& path) const;
	/// @brief Looks a colour up in "colors", in the current theme.
	/// @param name The name.
	/// @return The colour.
	/// @throws DocumentError If there is no such colour.
	[[nodiscard]] Color namedColor(std::string_view name) const;
	/// @brief Looks a number up in "tokens", in the current theme.
	/// @param name The name, without "@".
	/// @return The number, or nothing if no number token has this name.
	[[nodiscard]] std::optional<float> numberToken(std::string_view name) const;
	/// @brief Replaces "@name" of a number token by its number, in the current theme.
	/// @param value The value.
	/// @param scratch Holds the number when there is one.
	/// @return The number in scratch, or value itself.
	[[nodiscard]] const json& resolveToken(const json& value, json& scratch) const;
	/// @brief Replaces "@name" of number tokens by their numbers in an object's members ("anchors", "radius" and
	/// "shadow"), in the current theme.
	/// @param value The object.
	/// @param scratch Holds a copy with the numbers when a member names one.
	/// @return The copy in scratch, or value itself.
	[[nodiscard]] const json& resolveMemberTokens(const json& value, json& scratch) const;
	/// @brief Reads a "shadow": { "x", "y", "blur", "color" }, each optional.
	/// @param value The value.
	/// @param path Its place, for errors.
	/// @return The shadow.
	/// @throws DocumentError If it is malformed.
	[[nodiscard]] Shadow shadow(const json& value, const std::string& path) const;
	/// @brief Finds a font of "fonts".
	/// @param name The name.
	/// @return The font, or nullptr.
	[[nodiscard]] std::shared_ptr<const Font> findFont(std::string_view name) const;
	/// @brief A font of "fonts" in another size or style (a bold run of rich text): its file and fallbacks, loaded
	/// through the font cache. A document has at most 32 variants, until its fonts change.
	/// @param base A font of "fonts", or nullptr for "default".
	/// @param pixelHeight The size.
	/// @param style The style.
	/// @param at Its place, for errors.
	/// @return The font, or nullptr if @p base isn't a font of "fonts".
	/// @throws DocumentError If the variants are used up, or the font can't load.
	[[nodiscard]] std::shared_ptr<const Font> fontVariant(const Font* base, float pixelHeight, FontStyle style,
	                                                      const std::string& at);
	/// @brief Evaluates "theme" for this update.
	void updateTheme();
	/// @brief Chooses the locale for this update: the "strings" section's "locale", else the host's, else the section's
	/// "default", else "en"; tells the window whether it writes right to left.
	void updateLocale();
	/// @brief Sets the locale the host found on the computer (systemLocale()), used where "strings" names none.
	/// @param tag "fr-CA"; "" for none.
	void setHostLocale(std::string tag) { hostLocale_ = std::move(tag); }
	/// @brief The document's translations and the locale chosen.
	[[nodiscard]] const Strings& strings() const noexcept { return strings_; }
	/// @brief Counts what changes how the same values look: the theme, the colours, the fonts, the node types. A panel
	/// applied under another count is applied again even if its values are the same.
	/// @return The count; never 0.
	[[nodiscard]] std::uint64_t appearance() const noexcept { return appearance_; }
	/// @brief Records that the same values may now look different (a node type was registered or replaced).
	void changeAppearance() noexcept { ++appearance_; }
	/// @brief Counts the changes of the theme and of the document's colours, tokens and fonts, which an application's
	/// type reads its colours from: it is applied again when the count changes (unlike appearance(), not at every
	/// revision).
	/// @return The count; never 0.
	[[nodiscard]] std::uint64_t looks() const noexcept { return looks_; }

	/* Images */

	/// @brief Where the screen's images come from: files of the allowed folders (the screen's own folder, and those the
	/// host allows), and images the host names.
	[[nodiscard]] ImageCache& images() noexcept { return images_; }
	/// @brief The languages of "languages", or nullptr for none.
	[[nodiscard]] const LanguageCatalog* languages() const noexcept { return languages_.get(); }
	/// @brief Reads an image's "file": relative to the screen's folder, inside an allowed folder.
	/// @param value The path.
	/// @param path Its place, for errors.
	/// @return The file.
	/// @throws DocumentError If it isn't text, or is outside the allowed folders.
	[[nodiscard]] std::filesystem::path imageFile(const json& value, const std::string& path) const;
	/// @brief Reads an image's asset ("asset:<name>") of the version shown, and decodes its bytes into images() the
	/// first time (a named image, "asset:<sha256>": the same bytes decode once, whatever their name).
	/// @param value The reference.
	/// @param path Its place, for errors.
	/// @return The named image's name.
	/// @throws DocumentError If there is no such asset, it isn't a picture, or it doesn't decode.
	[[nodiscard]] std::string assetImage(const json& value, const std::string& path);

	/* The screen an outlet shows */

	/// @brief The screen shown now.
	[[nodiscard]] const std::string& screen() const noexcept { return screen_; }
	/// @brief The theme "theme" last evaluated to.
	/// @return Its name; empty without one.
	[[nodiscard]] const std::string& theme() const noexcept { return theme_; }
	/// @brief Shows the first screen of "screens" if the one shown is gone (or none was), forgetting the way back.
	/// @param document The document.
	void keepScreen(const json& document);
	/// @brief Shows a screen, remembering the one shown for "back".
	/// @param name The screen.
	void navigate(const std::string& name);
	/// @brief Shows the screen shown before the last navigate().
	/// @return false if there is none.
	bool back();

private:
	/// @brief A named colour, with variants per theme.
	using NamedColor = Themed<Color>;

	/// @brief The `nodes` variable: a built panel's geometry by id.
	/// @param id The id.
	/// @return {x, y, width, height, visible}, or null.
	json node(std::string_view id) const;
	/// @brief The colours of "colors".
	/// @param document The document.
	/// @return The colours.
	/// @throws DocumentError If one is invalid.
	static std::map<std::string, NamedColor, std::less<>> readColors(const json& document);

	Engine& engine_;                                                        ///< The runtime.
	std::filesystem::path baseDirectory_;                                   ///< Where font paths are resolved from.
	ColorTable colors_;                                                     ///< From "colors" and colour tokens.
	NumberTable numberTokens_;                                              ///< Number tokens of "tokens".
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> fonts_; ///< From "fonts".
	/// @brief Where a font of "fonts" comes from, to load it in another size or style.
	struct FontSource {
		std::filesystem::path file;                   ///< Its file.
		std::vector<std::filesystem::path> fallbacks; ///< Its fallbacks' files.
	};
	/// @brief Loads the fonts of "fonts" (readDerived()).
	/// @param doc The document, which has "fonts".
	/// @param numbers The number tokens, for the sizes.
	/// @param fonts Gets each font, by name.
	/// @param sources Gets where each comes from, by Font::id().
	/// @throws DocumentError If one is invalid or can't be loaded.
	void readFonts(const json& doc, const NumberTable& numbers,
	               std::map<std::string, std::shared_ptr<const Font>, std::less<>>& fonts,
	               std::map<std::uint64_t, FontSource>& sources);
	std::map<std::uint64_t, FontSource> fontSources_;                          ///< By Font::id(), for fonts_.
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> variants_; ///< See fontVariant(), by key.
	FontCache fontCache_; ///< By file, size and fallbacks: kept across revisions, within its bound.
	ImageCache images_;   ///< See images().
	std::shared_ptr<const LanguageCatalog> languages_; ///< See languages().
	json languagesSource_;                             ///< The "languages" they were read from.
	std::string theme_;                                ///< "theme", evaluated at each update.
	Strings strings_;                                  ///< "strings", and the locale chosen at each update.
	std::string hostLocale_;                           ///< See setHostLocale().
	std::string screen_;                               ///< The screen an "outlet" shows.
	std::vector<std::string> screenHistory_;           ///< For {"navigate": "back"}.

	std::uint64_t appearance_ = 1;           ///< See appearance().
	std::uint64_t looks_ = 1;                ///< See looks().
	json lookSections_;                      ///< "colors", "tokens" and "fonts" when looks_ last changed.
	WorkBudget* budget_ = nullptr;           ///< The running event's or update's budget (BudgetScope), if any.
	StateChanges changes_;                   ///< See takeChanges().
	std::string_view fullReason_;            ///< See takeFullReason().
	const StateChanges* updating_ = nullptr; ///< See updating().
	mutable ReadSet* reads_ = nullptr;       ///< Where the running ReadScope collects, if any.
	std::uint64_t stateVersion_ = 0;         ///< Counts state changes (computed values are cached per version).
	NodeLookup computedLookup_;              ///< ComputedValues::computed(), for expressions.
	NodeLookup nodeLookup_;                  ///< node(), for expressions.
	NodeLookup modelLookup_;                 ///< Models::at(), for expressions.
	VariableLookup variableLookup_;          ///< variable(), for expressions.
	VariableLookup computedValueLookup_;     ///< ComputedValues::value(), for expressions to read in place.
	ProgramCall programCall_;                ///< The program's functions, for the function `lisp`.
	HostVariables values_{engine_};          ///< The globals and built variables.
};

} // namespace affresco::format
