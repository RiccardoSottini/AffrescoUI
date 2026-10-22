#pragma once

/// @file
/// @brief format::DocumentContext: what the document's values are read against: colours, fonts, the theme, the
/// globals, the cached "computed" values and the screen an outlet shows.

#include "DesignTokens.hpp"
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

/// @brief What the document's values are read against: colours, fonts, the theme, the globals of expressions, the
/// cached "computed" values, and the screen an outlet shows (navigation).
///
/// Invariants: colours and fonts change all at once or not at all (readDerived()). The globals are built once and
/// reused until globalsChanged(); computed values are cached per stateVersion(), which markStateChanged() bumps.
/// Callers: every subsystem that evaluates a value (builder, properties, events, steps), the reconciler (theme and
/// screen per update) and Runtime's lookups. Main thread only.
class DocumentContext {
public:
	/// @brief A context for an engine's document.
	/// @param engine The runtime; outlives this object.
	/// @param baseDirectory Where font paths are resolved from.
	DocumentContext(Engine& engine, std::filesystem::path baseDirectory);

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
	/// @brief Drops the cached globals (history, proposals, preview, window size or screen changed).
	void globalsChanged() const {
		globalsCache_.reset();
		++variablesEpoch_;
	}
	/// @brief Adds a screen-wide variable built when first read and cached until the state, the globals or
	/// contextChanged() change (`context`, `commands`, `menus`, ...).
	/// @param name Its name.
	/// @param build Builds its value; it may evaluate the document's values, but not read itself.
	void addVariable(std::string name, std::function<json()> build);
	/// @brief Drops the built variables (the focus or a chord in progress changed what `context` holds).
	void contextChanged() const noexcept { ++variablesEpoch_; }
	/// @brief A variable addVariable() added, built if it is stale.
	/// @param name Its name.
	/// @return Its value, valid until it is stale; null if there is no such variable, or it is being built.
	[[nodiscard]] const json* variable(std::string_view name) const;
	/// @brief Records that the state changed in a way no place describes: computed values are stale and every value
	/// is evaluated again at the next update.
	void markStateChanged() noexcept {
		changes_.addAll();
		++stateVersion_;
	}
	/// @brief Records that one place of the state changed: computed values are stale, and the next update evaluates
	/// again only the values that read it.
	/// @param pointer The place.
	void markStateChanged(const json::json_pointer& pointer) {
		changes_.add(pointer.to_string());
		++stateVersion_;
	}
	/// @brief Records that a place outside the state changed, a data model's rows (Models.hpp): the next update
	/// evaluates again only the values that read it. Computed values never read there, so they stay.
	/// @param place The place, "/~m/<name>/<row>".
	void markPlaceChanged(std::string place) { changes_.add(std::move(place)); }
	/// @brief Whether the state changed, or an update was requested, since the last update began (takeChanges()).
	[[nodiscard]] bool stateChanged() const noexcept { return !changes_.empty(); }
	/// @brief What changed since the last update began, as an update begins: the state is then up to date with the
	/// panels.
	/// @return The changes.
	[[nodiscard]] StateChanges takeChanges() noexcept { return std::exchange(changes_, {}); }
	/// @brief Marks a full update as due (every value evaluated again), without staling computed values.
	void requestUpdate() noexcept { changes_.addAll(); }
	/// @brief Makes computed values stale (a revision may change the state too).
	void bumpStateVersion() noexcept { ++stateVersion_; }

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

	/// @brief Reads a colour: "#rrggbb", "@name" (from "colors", in the current theme) or "transparent".
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

	/* Images */

	/// @brief Where the screen's images come from: files of the allowed folders (the screen's own folder, and those the
	/// host allows), and images the host names.
	[[nodiscard]] ImageCache& images() noexcept { return images_; }
	/// @brief Reads an image's "file": relative to the screen's folder, inside an allowed folder.
	/// @param value The path.
	/// @param path Its place, for errors.
	/// @return The file.
	/// @throws DocumentError If it isn't text, or is outside the allowed folders.
	[[nodiscard]] std::filesystem::path imageFile(const json& value, const std::string& path) const;

	/* The screen an outlet shows */

	/// @brief The screen shown now.
	[[nodiscard]] const std::string& screen() const noexcept { return screen_; }
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
	/// @brief Screen-wide variables for expressions.
	struct Globals {
		json window, revision, other; ///< Window size, current revision, and the rest (proposals, computed, ...).
	};
	/// @brief A named colour, with variants per theme.
	using NamedColor = Themed<Color>;

	/// @brief The globals, built once and reused until globalsChanged().
	[[nodiscard]] const Globals& globals() const;
	/// @brief `computed.<name>`: evaluated when first read after a state change, then cached.
	/// @param name The computed value.
	/// @return Its value.
	json computed(std::string_view name) const;
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
	std::map<std::uint64_t, FontSource> fontSources_;                          ///< By Font::id(), for fonts_.
	std::map<std::string, std::shared_ptr<const Font>, std::less<>> variants_; ///< See fontVariant(), by key.
	FontCache fontCache_;                    ///< By file, size and fallbacks: kept across revisions, within its bound.
	ImageCache images_;                      ///< See images().
	std::string theme_;                      ///< "theme", evaluated at each update.
	Strings strings_;                        ///< "strings", and the locale chosen at each update.
	std::string hostLocale_;                 ///< See setHostLocale().
	std::string screen_;                     ///< The screen an "outlet" shows.
	std::vector<std::string> screenHistory_; ///< For {"navigate": "back"}.

	std::uint64_t appearance_ = 1;                  ///< See appearance().
	WorkBudget* budget_ = nullptr;                  ///< The running event's or update's budget (BudgetScope), if any.
	StateChanges changes_;                          ///< See takeChanges().
	mutable ReadSet* reads_ = nullptr;              ///< Where the running ReadScope collects, if any.
	std::uint64_t stateVersion_ = 0;                ///< Counts state changes (computed values are cached per version).
	mutable std::optional<Globals> globalsCache_;   ///< See globals().
	mutable json computedValues_ = json::object();  ///< "computed" values read since the state last changed.
	mutable std::uint64_t computedVersion_ = ~0ull; ///< The state version computedValues_ belong to.
	mutable std::map<std::string, ReadSet, std::less<>> computedReads_; ///< What each computed value read.
	mutable std::vector<std::string> computing_; ///< Computed values being evaluated (finds circles).
	NodeLookup computedLookup_;                  ///< computed(), for expressions.
	NodeLookup nodeLookup_;                      ///< node(), for expressions.
	NodeLookup modelLookup_;                     ///< Models::at(), for expressions.
	VariableLookup variableLookup_;              ///< variable(), for expressions.
	std::map<std::string, std::function<json()>, std::less<>> builders_; ///< See addVariable().
	mutable std::map<std::string, json, std::less<>> built_;             ///< Variables built since they were stale.
	mutable std::vector<std::string> building_;                          ///< Variables being built (finds circles).
	mutable std::uint64_t variablesEpoch_ = 0;                           ///< Counts what makes built variables stale.
	mutable std::uint64_t builtEpoch_ = ~0ull;                           ///< The epoch built_ belongs to.
	mutable std::uint64_t builtStateVersion_ = 0;                        ///< The state version built_ belongs to.
};

} // namespace affresco::format
