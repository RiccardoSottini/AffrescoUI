/// @file
/// @brief What an entry's text reads to, kept for the process (TopLevel.hpp, readEntry()): a document's program is read
/// by its split, by its build and by the interpreter that runs it, and again each time the app opens. Reading is pure
/// (the same text, file and place give the same form), so each text is read once and its form copied into the heap that
/// asks, which costs a fraction of reading it: no lexing, no escapes, no numbers to parse.

#include "Reader.hpp"
#include "TopLevel.hpp"

#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

/// What the kept texts and their forms cost, at most, in bytes: the largest apps' programs are well under it.
constexpr std::size_t keptBytes = 32U << 20U;
/// A text longer than this is read each time: one definition that large is rare, and would crowd out the others.
constexpr std::size_t largestKept = 1U << 20U;

/// @brief A form read once, in a heap of its own that nothing changes afterwards.
struct Kept {
	Heap heap;             ///< Owns the form; nothing is made in it once it is kept.
	Value form;            ///< The form.
	std::size_t bytes = 0; ///< What its key and its form cost, counted against keptBytes.
};

/// @brief The forms kept, by their text and its place, least recently used first to go.
class KeptForms {
public:
	/// @brief Finds a form.
	/// @param key Its key.
	/// @return It, or null.
	std::shared_ptr<const Kept> find(const std::string& key) {
		const std::scoped_lock lock(mutex_);
		const auto found = forms_.find(key);
		if(found == forms_.end()) return nullptr;
		order_.splice(order_.end(), order_, found->second.second);
		return found->second.first;
	}

	/// @brief Keeps a form, letting the least recently used go past keptBytes.
	/// @param key Its key.
	/// @param kept The form.
	void keep(const std::string& key, std::shared_ptr<const Kept> kept) {
		const std::scoped_lock lock(mutex_);
		if(forms_.contains(key)) return;
		bytes_ += kept->bytes;
		order_.push_back(key);
		forms_.emplace(key, std::pair(std::move(kept), std::prev(order_.end())));
		while(bytes_ > keptBytes && !order_.empty()) {
			const auto oldest = forms_.find(order_.front());
			bytes_ -= oldest->second.first->bytes;
			forms_.erase(oldest);
			order_.pop_front();
		}
	}

private:
	std::mutex mutex_;             ///< Programs load on any thread.
	std::list<std::string> order_; ///< Keys, least recently used first.
	std::unordered_map<std::string, std::pair<std::shared_ptr<const Kept>, std::list<std::string>::iterator>>
	    forms_;             ///< By key.
	std::size_t bytes_ = 0; ///< What they cost.
};

/// @brief The process's forms. @return Them.
KeptForms& keptForms() {
	static KeptForms forms;
	return forms;
}

/// @brief A place in another heap: the same line and column, its file kept there.
/// @param to The heap.
/// @param place The place.
/// @return It.
Place placeIn(Heap& to, Place place) {
	if(place.file) place.file = to.file(*place.file);
	return place;
}

/// @brief Copies a form read by the reader into a heap: a symbol, a keyword or a string with its place, a list, a
/// vector or a map of copies, and anything else as it is.
/// @param to The heap.
/// @param form The form.
/// @return The copy.
Value copied(Heap& to, const Value& form) {
	switch(form.kind()) {
	case Kind::String:
		return to.string(form.text(), placeIn(to, form.place()));
	case Kind::Symbol:
		return to.symbol(form.text(), placeIn(to, form.place()));
	case Kind::Keyword:
		return to.keyword(form.text(), placeIn(to, form.place()));
	case Kind::List:
	case Kind::Vector: {
		std::vector<Value> items;
		items.reserve(form.count());
		for(std::size_t at = 0; at < form.count(); ++at) items.push_back(copied(to, form.at(at)));
		return to.sequence(form.kind(), std::move(items), placeIn(to, form.place()));
	}
	case Kind::Map: {
		std::vector<MapEntry> entries;
		entries.reserve(form.count());
		for(std::size_t at = 0; at < form.count(); ++at)
			entries.push_back({to.keep(form.keyAt(at)), copied(to, form.valueAt(at))});
		return to.map(std::move(entries), placeIn(to, form.place()));
	}
	default:
		return form;
	}
}

/// @brief Reads an entry's one form.
/// @param heap Where.
/// @param text The text.
/// @param file Its file.
/// @param line Its first line.
/// @param column Its first column.
/// @return The form, or nil and false when the text isn't one form.
std::pair<Value, bool> readOne(Heap& heap, std::string_view text, const std::string& file, std::size_t line,
                               std::size_t column) {
	Reader reader(heap, text, file, line, column);
	const auto read = reader.next();
	if(!read || reader.next()) return {Value(), false};
	return {read->form, true};
}

} // namespace

std::optional<Value> readEntry(Heap& heap, std::string_view text, const std::string& file, std::size_t line,
                               std::size_t column) {
	if(text.size() > largestKept) {
		const auto [form, one] = readOne(heap, text, file, line, column);
		return one ? std::optional(form) : std::nullopt;
	}
	std::string key = file;
	key.append(1, '\0').append(std::to_string(line)).append(1, ':').append(std::to_string(column)).append(1, '\0');
	key.append(text);
	std::shared_ptr<const Kept> kept = keptForms().find(key);
	if(!kept) {
		const auto made = std::make_shared<Kept>();
		// What the form costs is measured as it is made: a text of nested brackets makes many objects per byte
		Budget measured{.operations = 0, .bytes = std::numeric_limits<std::size_t>::max()};
		made->heap.setBudget(&measured);
		const auto [form, one] = readOne(made->heap, text, file, line, column); // reads, or throws as reading does
		made->heap.setBudget(nullptr);
		if(!one) return std::nullopt;
		made->form = form;
		made->bytes = key.size() + (std::numeric_limits<std::size_t>::max() - measured.bytes);
		if(made->bytes > largestKept * 8) return copied(heap, form); // would crowd out the others: read each time
		kept = made;
		keptForms().keep(key, kept);
	}
	return copied(heap, kept->form);
}

} // namespace affresco::lisp
