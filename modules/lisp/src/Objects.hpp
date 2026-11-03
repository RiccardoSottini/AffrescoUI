#pragma once

/// @file
/// @brief What values point at, and the heap that owns them all.
///
/// Every object of a run lives in one heap and goes with it, in one flat loop, so that freeing a long list or a chain
/// of closures takes no recursion, and a Value is a plain copy. A region (Heap::mark()) gives back what one event made,
/// so that a long-running app doesn't grow.

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Value.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::lisp {

class Machine;

/// @brief What a primitive or the machine raises: a condition a `try` can catch, unless a limit ran out.
struct Problem : std::runtime_error {
	/// @brief A problem.
	/// @param what What is wrong: what an uncaught error says.
	/// @param limit A limit ran out: no `try` catches it.
	explicit Problem(const std::string& what, bool limit = false)
	    : std::runtime_error(what), message(what), exhausted(limit) {}
	/// @brief What `throw` raises.
	/// @param what What an uncaught error says.
	/// @param thrown What `catch` binds.
	Problem(const std::string& what, const Value& thrown)
	    : std::runtime_error(what), message(what), value(thrown), hasValue(true) {}
	std::string message;    ///< See the constructor.
	Value value;            ///< What `catch` binds, when thrown (hasValue).
	bool hasValue = false;  ///< Thrown by `throw`, with this value.
	bool exhausted = false; ///< See the constructor.
};

/// @brief An object of the heap.
struct Object {
	Object() = default;
	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;
	virtual ~Object() = default;
};

/// @brief A string, a symbol or a keyword.
struct TextObject final : Object {
	std::string text; ///< Its text.
	Place place;      ///< Where it was read, if it was.
};

/// @brief The storage of lists and vectors. It only grows: a value sees the part it was made with, so that `conj` on a
/// vector's end needn't copy.
struct Items final : Object {
	std::vector<Value> values; ///< The items.
	std::uint64_t epoch = 0;   ///< The region it was made in: another region's values may not be added to it.
};

/// @brief A list or a vector: a part of some Items.
struct SequenceObject final : Object {
	const Items* items = nullptr; ///< The storage.
	std::uint32_t offset = 0;     ///< Its first item there.
	std::uint32_t size = 0;       ///< How many it has.
	Place place;                  ///< Where it was read, if it was.
};

/// @brief One member of a map. The key's text is in the heap, or in JSON a view keeps alive.
struct MapEntry {
	std::string_view key; ///< Its key.
	Value value;          ///< Its value.
};

/// @brief A map: its members in the order written, with a hash index past a few.
struct MapObject final : Object {
	std::vector<MapEntry> entries; ///< The members.
	Place place;                   ///< Where it was read, if it was.

	/// @brief The index of a key. Past a few members the first search builds an index, so a big map isn't searched
	/// member by member.
	/// @param key The key.
	/// @return Its index, or entries.size().
	[[nodiscard]] std::size_t find(std::string_view key) const;

private:
	/// Keys found by a search one by one; past this many, through the index.
	static constexpr std::size_t searched = 16;
	mutable std::unique_ptr<std::map<std::string_view, std::uint32_t, std::less<>>> index_; ///< See find().
};

/// @brief A set of authors, by their number in the machine's table. Sets are interned: one pointer per set.
struct AuthorSet {
	std::vector<std::uint32_t> authors; ///< The authors, sorted.
};

/// @brief What an instruction does (Machine.cpp, one function each).
enum class Op : std::uint8_t {
	Const,            ///< Pushes constant a.
	Local,            ///< Pushes local a.
	SetLocal,         ///< Pops into local a.
	Capture,          ///< Pushes captured value a.
	Self,             ///< Pushes the function being run.
	Global,           ///< Pushes global a.
	Pop,              ///< Drops the top.
	Jump,             ///< Goes to a.
	JumpIfFalse,      ///< Pops; goes to a if it is false.
	JumpIfFalseKeep,  ///< Goes to a if the top is false; else pops it.
	JumpIfTrueKeep,   ///< Goes to a if the top is true; else pops it.
	JumpIfNotNilKeep, ///< Goes to a if the top isn't nil; else pops it.
	Call,             ///< Calls with a arguments.
	TailCall,         ///< Calls with a arguments, in place of this function.
	Return,           ///< Returns the top.
	Closure,          ///< Pushes a closure of function a.
	BuildList,        ///< Replaces the top a values by a list; constant b holds a value with its place.
	BuildVector,      ///< Replaces the top a values by a vector.
	BuildMap,         ///< Replaces the top 2a values, key then value, by a map.
	Splice,           ///< Marks the top to be spread by the Build that takes it.
	Recur,            ///< Pops a values into the locals from b.
	Throw,            ///< Raises the top; constant a is its message, if it was written in the form.
	TryBegin,         ///< On a condition in what follows, goes to a with it on the stack.
	TryEnd,           ///< Ends the last TryBegin.
	Count             ///< How many there are.
};

/// @brief An instruction.
struct Instruction {
	Op op = Op::Pop;     ///< What it does.
	std::uint32_t a = 0; ///< Its first operand.
	std::uint32_t b = 0; ///< Its second operand.
};

struct Code;

/// @brief A global: a name, and what it holds.
struct Global {
	/// @brief What is known of its value.
	enum class State : std::uint8_t {
		Undefined, ///< Nothing defines it.
		Deferred,  ///< Defined, not compiled yet: pending does that.
		Lazy,      ///< A `def` not worked out yet: thunk does that.
		Running,   ///< The thunk is running.
		Ready      ///< value holds it.
	};
	std::string name;               ///< Its name.
	State state = State::Undefined; ///< See State.
	Value value;                    ///< Its value, when Ready.
	const Code* thunk = nullptr;    ///< What works out a `def`.
	bool macro = false;             ///< A macro, not a function.
	bool library = false;           ///< One of the language's own.
	bool host = false;              ///< A host's primitive no definition may replace (Primitive::reserved).
	bool view = false;              ///< A `defview`: what calls it in a template runs at the build, never later.
	std::function<void()> pending;  ///< What compiles it.
};

/// A CaptureSource::index that stands for the closure being run, which a function inside it captures by its name.
inline constexpr std::uint32_t selfCapture = 0xFFFFFFFFU;

/// @brief Where a closure's captured value comes from, in the function that makes it.
struct CaptureSource {
	bool local = true;       ///< From its locals, else from its own captures.
	std::uint32_t index = 0; ///< Its index there.
};

/// @brief A call of a global whose first argument is a text written out: `(run "save")`, `(invoke :files.read)`.
struct NamedCall {
	std::uint32_t global = 0; ///< The global's index in Code::globals.
	std::string text;         ///< The text: a string's, or a keyword's without its colon.
};

/// @brief A compiled function, or a thunk: what runs when a form does.
struct Code final : Object {
	std::string name;                    ///< The function's name, or "".
	std::string definition;              ///< The definition it is in, or "".
	std::string author;                  ///< That definition's author, which a closure keeps.
	const AuthorSet* authors = nullptr;  ///< Its author and those of the macros that wrote it.
	bool library = false;                ///< The language's own: an error names its caller's place.
	std::uint32_t arity = 0;             ///< Parameters, without a rest parameter.
	bool variadic = false;               ///< A rest parameter follows them.
	std::uint32_t slots = 0;             ///< Locals, parameters included.
	std::vector<Instruction> ops;        ///< What it does.
	std::vector<Place> places;           ///< Where each instruction comes from.
	std::vector<Value> constants;        ///< Op::Const's values.
	std::vector<Global*> globals;        ///< Op::Global's globals.
	std::vector<NamedCall> named;        ///< The globals among them that head a call with a text written out first.
	std::vector<const Code*> functions;  ///< Op::Closure's functions.
	std::vector<CaptureSource> captures; ///< What its closures capture.
	std::vector<std::string> parameters; ///< Its parameters' names, without a rest parameter; "" for a pattern.
};

/// @brief A closure: a function and what it captured.
struct ClosureObject final : Object {
	const Code* code = nullptr;  ///< The function.
	std::vector<Value> captured; ///< What it captured.
};

/// @brief A function of the language's own.
using NativeFunction = Value (*)(Machine&, std::span<const Value>);

/// @brief A function of the language's own, or a host's primitive.
struct NativeObject final : Object {
	std::string name;                  ///< Its name.
	NativeFunction function = nullptr; ///< The language's own function, or nullptr for a host's.
	Primitive host;                    ///< A host's primitive.
	bool spread = false;               ///< `apply`: the machine spreads its last argument and calls its first.
	Value wrapped;                     ///< The function a host wraps with it (Interpreter::wrapGlobal()), or nil.
};

/// @brief A value `~@` spreads.
struct SpliceObject final : Object {
	Value value; ///< It.
};

/// @brief Owns a run's objects, and counts what they cost against a budget.
class Heap {
public:
	/// @brief Sets what allocating spends. @param budget The budget, or nullptr to spend nothing, as reading text does.
	void setBudget(Budget* budget) noexcept { budget_ = budget; }

	/// @brief Makes an object.
	/// @tparam T Its type.
	/// @param extra What it holds besides itself, in bytes.
	/// @return It, owned by the heap.
	/// @throws Problem If the budget runs out.
	template <class T> T* make(std::size_t extra = 0) {
		charge(sizeof(T) + extra);
		auto object = std::make_unique<T>();
		T* made = object.get();
		objects_.push_back(std::move(object));
		return made;
	}

	/// @brief What a region starts from: the objects made before it stay.
	struct Mark {
		std::size_t objects = 0; ///< How many objects there were.
		std::size_t adopted = 0; ///< How many JSON values were kept.
	};
	/// @brief Starts a region: release() gives back what is made from now on.
	/// @return Where it starts.
	[[nodiscard]] Mark mark() noexcept {
		++epoch_;
		pinned_ = false;
		return {objects_.size(), adopted_.size()};
	}
	/// @brief Ends a region: what it made goes, unless something lasting points at it (pin()). No value of the region
	/// may be read afterwards.
	/// @param start Where it started.
	void release(const Mark& start) {
		if(pinned_) return;
		objects_.resize(start.objects); // in one flat loop: nothing an object points at is followed
		adopted_.resize(start.adopted);
	}
	/// @brief How many objects it holds. @return The count.
	[[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }
	/// @brief Keeps what the running region made: something lasting (a global, compiled code) may point at it.
	void pin() noexcept { pinned_ = true; }

	/// @brief Spends bytes. @param bytes How many.
	/// @throws Problem If the budget runs out.
	void charge(std::size_t bytes) {
		if(!budget_) return;
		if(budget_->bytes < bytes) throw Problem("out of memory: the budget for allocation ran out", true);
		budget_->bytes -= bytes;
	}

	/// @brief A string. @param written Its text. @param place Where it was read. @return The value.
	Value string(std::string_view written, Place place = {});
	/// @brief A symbol. @param written Its text. @param place Where it was read. @return The value.
	Value symbol(std::string_view written, Place place = {});
	/// @brief A keyword. @param written Its text, without ':'. @param place Where it was read. @return The value.
	Value keyword(std::string_view written, Place place = {});
	/// @brief A list or a vector, of a copy of its items.
	/// @param kind Kind::List or Kind::Vector.
	/// @param items Its items.
	/// @param place Where it was read.
	/// @return The value.
	Value sequence(Kind kind, std::span<const Value> items, Place place = {});
	/// @brief A list or a vector that owns the storage it is given.
	/// @param kind Kind::List or Kind::Vector.
	/// @param items Its items.
	/// @param place Where it was read.
	/// @return The value.
	Value sequence(Kind kind, std::vector<Value>&& items, Place place = {});
	/// @brief A part of a list or a vector, sharing its storage; a view of JSON is copied in first.
	/// @param from The list or the vector.
	/// @param offset Its first item.
	/// @param size How many.
	/// @return The value, of the same kind.
	Value part(const Value& from, std::size_t offset, std::size_t size);
	/// @brief A vector with one more item: no copy when @p from ends its storage.
	/// @param from The vector.
	/// @param item The item.
	/// @return The value.
	Value appended(const Value& from, const Value& item);
	/// @brief A map. @param entries Its members. @param place Where it was read. @return The value.
	Value map(std::vector<MapEntry>&& entries, Place place = {});
	/// @brief Copies a text into the heap, for a map's key. @param written The text. @return The copy.
	std::string_view keep(std::string_view written);
	/// @brief Keeps a file's name for the places that name it. @param name The name. @return The kept name.
	const std::string* file(const std::string& name);
	/// @brief Keeps JSON alive as long as the heap. @param value The JSON.
	void adopt(std::shared_ptr<const json> value) { adopted_.push_back(std::move(value)); }

private:
	/// @brief A symbol, a keyword or a string.
	/// @param kind Which.
	/// @param written Its text.
	/// @param place Where it was read.
	/// @return The value.
	Value text(Kind kind, std::string_view written, Place place);

	Budget* budget_ = nullptr;                         ///< See setBudget().
	std::uint64_t epoch_ = 0;                          ///< The region being made in (Items::epoch).
	bool pinned_ = false;                              ///< See pin().
	std::vector<std::unique_ptr<Object>> objects_;     ///< What it owns.
	std::deque<std::string> files_;                    ///< See file().
	std::vector<std::shared_ptr<const json>> adopted_; ///< See adopt().
};

/// @brief A map's key from a value: a string's or a keyword's text.
/// @param key The value.
/// @return Its text.
/// @throws Problem If it is neither.
[[nodiscard]] std::string_view keyText(const Value& key);

/// @brief A number as an index, a count or a position takes it: an integer, or a number whose value is whole (`1.0`,
/// as JSON and an expression's arithmetic give some), up to 2^53, where every whole number has a double.
/// @param value The value.
/// @return Its whole value, or std::nullopt if it isn't a whole number.
[[nodiscard]] std::optional<std::int64_t> wholeNumber(const Value& value) noexcept;

} // namespace affresco::lisp
