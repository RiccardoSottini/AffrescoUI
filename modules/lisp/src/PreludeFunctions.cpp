/// @file
/// @brief The language's further functions on collections and functions (Machine.hpp, defineFunctions()): those that
/// call a function back, written in the language as entries of the library (Library.hpp), like Prelude.cpp's; and the
/// native ones that build maps (`zipmap`, `frequencies`, `hash-map`, `select-keys`), which find a key through a hash
/// index, so that building a map of n keys walks n items, not n times n. A map's key stays text (decision D3): a key
/// that is neither a keyword nor a string is an error that says what to write instead.

#include "Library.hpp"
#include "Machine.hpp"
#include "Sequences.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief A map being built, a key at a time, with an index from each key to its member.
class MapBuilder {
public:
	/// @brief Starts one. @param machine The machine. @param expected How many keys it expects, to reserve.
	MapBuilder(Machine& machine, std::size_t expected) : machine_(machine) {
		machine.heap.charge(expected * (sizeof(MapEntry) + 4 * sizeof(std::size_t)));
		members_.reserve(expected);
		index_.reserve(expected);
	}

	/// @brief A key's member, made with a value if the map hasn't it yet.
	/// @param key The key, as a keyword or a string.
	/// @param fallback The value it starts with.
	/// @param fix What to write instead when the key isn't text (textKey()).
	/// @return Its value, to set.
	Value& at(const Value& key, const Value& fallback, std::string_view fix) {
		machine_.spend(1);
		const std::string_view text = textKey(key, fix);
		machine_.spendText(text.size()); // hashing and comparing it: a key can be long, and come many times
		if(const auto found = index_.find(text); found != index_.end()) return members_[found->second].value;
		const std::string_view kept = machine_.heap.keep(text);
		index_.emplace(kept, members_.size());
		members_.push_back({kept, fallback});
		return members_.back().value;
	}

	/// @brief The members so far, to change before the map is made. @return Them.
	std::vector<MapEntry>& members() noexcept { return members_; }

	/// @brief The map. @return It, its keys in the order they first came.
	Value made() { return machine_.heap.map(std::move(members_)); }

private:
	Machine& machine_;                                        ///< The machine.
	std::vector<MapEntry> members_;                           ///< The members so far.
	std::unordered_map<std::string_view, std::size_t> index_; ///< Each key's member.
};

/// @brief A map from keys and values in step, a later key's value replacing an earlier one's.
/// @param machine The machine.
/// @param keys The keys.
/// @param values The values: as many are used as there are both.
/// @param function The function, for an error.
/// @param fix What to write instead when a key isn't text.
/// @return The map.
Value mapOf(Machine& machine, const Value& keys, const Value& values, std::string_view function, std::string_view fix) {
	const Value names = sequenceOf(machine, keys, function);
	const Value items = sequenceOf(machine, values, function);
	const std::size_t size = std::min(names.count(), items.count());
	MapBuilder map(machine, size);
	for(std::size_t at = 0; at < size; ++at) map.at(names.at(at), {}, fix) = items.at(at);
	return map.made();
}

/// @brief `zipmap`. @param machine The machine. @param arguments Keys and values. @return The map of each key to the
/// value in its place, up to the shorter.
Value zipMap(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "zipmap");
	return mapOf(machine, arguments[0], arguments[1], "zipmap", "(zipmap (map str ks) vs)");
}

/// @brief `%map-of`: what `update-keys` and `update-vals` end with.
/// @param machine The machine.
/// @param arguments Keys, values, and what to write instead when a key isn't text.
/// @return The map.
Value mapOfKeys(Machine& machine, Arguments arguments) {
	needCount(arguments, 3, 3, "%map-of");
	return mapOf(machine, arguments[0], arguments[1], "%map-of", arguments[2].text());
}

/// @brief `hash-map`. @param machine The machine. @param arguments Keys and values, in turn. @return The map of them.
Value hashMap(Machine& machine, Arguments arguments) {
	if(arguments.size() % 2 != 0) throw Problem("\"hash-map\" takes pairs of a key and a value");
	MapBuilder map(machine, arguments.size() / 2);
	for(std::size_t at = 0; at < arguments.size(); at += 2)
		map.at(arguments[at], {}, "(hash-map (str key) value)") = arguments[at + 1];
	return map.made();
}

/// @brief `frequencies`. @param machine The machine. @param arguments A collection of keywords or strings. @return The
/// map of each to how many times it comes.
Value frequencies(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "frequencies");
	const Value items = sequenceOf(machine, arguments[0], "frequencies");
	MapBuilder map(machine, items.count());
	for(std::size_t at = 0; at < items.count(); ++at) {
		Value& times = map.at(items.at(at), Value::integer(0), "(frequencies (map str xs))");
		times = Value::integer(times.asInteger() + 1);
	}
	return map.made();
}

/// @brief `%group-by`: what `group-by` ends with.
/// @param machine The machine.
/// @param arguments A vector of keys, and a vector of items as long.
/// @return The map of each key to the vector of the items with it, in order.
Value groupBy(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "group-by");
	const Value& keys = arguments[0];
	const Value& items = arguments[1];
	if(!keys.isSequence() || !items.isSequence() || keys.count() != items.count())
		throw Problem("\"%group-by\" takes two lists as long as each other");
	std::vector<std::vector<Value>> groups;
	MapBuilder map(machine, keys.count());
	machine.heap.charge(items.count() * sizeof(Value));
	for(std::size_t at = 0; at < keys.count(); ++at) {
		const Value& group =
		    map.at(keys.at(at), Value::integer(static_cast<std::int64_t>(groups.size())), "(group-by (comp str f) xs)");
		if(static_cast<std::size_t>(group.asInteger()) == groups.size()) groups.emplace_back();
		groups[static_cast<std::size_t>(group.asInteger())].push_back(items.at(at));
	}
	for(MapEntry& member : map.members()) // each holds its group's index until now
		member.value =
		    machine.heap.sequence(Kind::Vector, std::move(groups[static_cast<std::size_t>(member.value.asInteger())]));
	return map.made();
}

/// @brief `select-keys`. @param machine The machine. @param arguments A map or nil, and keys. @return The map of the
/// members with those keys that it has.
Value selectKeys(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "select-keys");
	const Value& source = arguments[0];
	if(!source.isNil() && source.kind() != Kind::Map)
		throw Problem("\"select-keys\" takes a map, and was given " + std::string(describeKind(source)));
	const Value names = sequenceOf(machine, arguments[1], "select-keys");
	MapBuilder map(machine, names.count());
	for(std::size_t at = 0; at < names.count(); ++at) {
		bool found = false;
		const std::string_view key = textKey(names.at(at), "(str key)");
		machine.spendLookup(source, key);
		const Value value = source.isNil() ? Value() : source.lookup(key, &found);
		if(found) map.at(names.at(at), {}, "(str key)") = value;
	}
	return map.made();
}

/// The functions written in the language, in the order they use each other.
constexpr auto functions = std::to_array<LibraryEntry>({
    {.name = "mapv",
	 .source = R"lisp((defn mapv [f coll & colls] (apply map f coll colls)))lisp",
	 .takesFunctions = true},
    {.name = "filterv", .source = R"lisp((defn filterv [pred coll] (filter pred coll)))lisp", .takesFunctions = true},
    {.name = "keep",
	 .source = R"lisp((defn keep [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (let [kept (f (nth items at))]
          (recur (if (nil? kept) result (conj result kept)) (inc at)))
        result)))))lisp",
	 .takesFunctions = true},
    {.name = "keep-indexed",
	 .source = R"lisp((defn keep-indexed [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (let [kept (f at (nth items at))]
          (recur (if (nil? kept) result (conj result kept)) (inc at)))
        result)))))lisp",
	 .takesFunctions = true},
    {.name = "%count-while", .source = R"lisp((defn %count-while [pred items]
  (let [size (count items)]
    (loop [at 0]
      (if (and (< at size) (pred (nth items at))) (recur (inc at)) at)))))lisp"},
    {.name = "take-while",
	 .source = R"lisp((defn take-while [pred coll]
  (let [items (vec coll)] (subvec items 0 (%count-while pred items)))))lisp",
	 .takesFunctions = true},
    {.name = "drop-while",
	 .source = R"lisp((defn drop-while [pred coll]
  (let [items (vec coll)] (subvec items (%count-while pred items)))))lisp",
	 .takesFunctions = true},
    {.name = "split-with",
	 .source = R"lisp((defn split-with [pred coll]
  (let [items (vec coll) size (%count-while pred items)]
    [(subvec items 0 size) (subvec items size)])))lisp",
	 .takesFunctions = true},
    {.name = "partition-by",
	 .source = R"lisp((defn partition-by [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [parts [] part [] last-key nil at 0]
      (if (< at size)
        (let [item (nth items at) key (f item)]
          (if (or (= at 0) (= key last-key))
            (recur parts (conj part item) key (inc at))
            (recur (conj parts part) [item] key (inc at))))
        (if (empty? part) parts (conj parts part)))))))lisp",
	 .takesFunctions = true},
    {.name = "group-by",
	 .source = R"lisp((defn group-by [f coll]
  (let [items (vec coll)] (%group-by (map f items) items))))lisp",
	 .takesFunctions = true},
    {.name = "reductions",
	 .source = R"lisp((defn reductions [f start & more]
  (let [seeded (not (empty? more))
        items (%seq (if seeded (first more) start))
        size (count items)]
    (if (and (not seeded) (= size 0))
      [(f)]
      (loop [result (if seeded start (nth items 0))
             steps [(if seeded start (nth items 0))]
             at (if seeded 0 1)]
        (if (< at size)
          (let [next-result (f result (nth items at))]
            (recur next-result (conj steps next-result) (inc at)))
          steps))))))lisp",
	 .takesFunctions = true},
    {.name = "reduce-kv",
	 .source = R"lisp((defn reduce-kv [f start coll]
  (if (map? coll)
    (reduce (fn [result pair] (f result (first pair) (second pair))) start coll)
    (let [items (%seq coll) size (count items)]
      (loop [result start at 0]
        (if (< at size)
          (recur (f result at (nth items at)) (inc at))
          result))))))lisp",
	 .takesFunctions = true},
    {.name = "merge-with",
	 .source = R"lisp((defn merge-with [f & maps]
  (reduce
    (fn [result m]
      (if (nil? m)
        result
        (reduce
          (fn [merged pair]
            (let [k (first pair) v (second pair)]
              (if (contains? merged k) (assoc merged k (f (get merged k) v)) (assoc merged k v))))
          (if (nil? result) {} result)
          m)))
    nil
    maps)))lisp",
	 .takesFunctions = true},
    {.name = "update-vals",
	 .source = R"lisp((defn update-vals [m f]
  (%map-of (keys m) (map f (vals m)) "(update-vals m f)")))lisp",
	 .takesFunctions = true},
    {.name = "update-keys",
	 .source = R"lisp((defn update-keys [m f]
  (%map-of (map f (keys m)) (vals m) "(update-keys m (comp str f))")))lisp",
	 .takesFunctions = true},
    {.name = "not-any?",
	 .source = R"lisp((defn not-any? [pred coll] (not (some pred coll))))lisp",
	 .takesFunctions = true},
    {.name = "not-every?",
	 .source = R"lisp((defn not-every? [pred coll] (not (every? pred coll))))lisp",
	 .takesFunctions = true},
    {.name = "partial",
	 .source = R"lisp((defn partial [f & given]
  (fn [& more] (apply f (concat given more)))))lisp",
	 .takesFunctions = true},
    {.name = "comp",
	 .source = R"lisp((defn comp [& fs]
  (let [fns (vec fs) size (count fns)]
    (if (= size 0)
      identity
      (fn [& args]
        (loop [result (apply (nth fns (- size 1)) args) at (- size 2)]
          (if (< at 0) result (recur ((nth fns at) result) (- at 1)))))))))lisp",
	 .takesFunctions = true},
    {.name = "juxt",
	 .source = R"lisp((defn juxt [f & fs]
  (let [fns (cons f fs)]
    (fn [& args] (map (fn [g] (apply g args)) fns)))))lisp",
	 .takesFunctions = true},
    {.name = "complement",
	 .source = R"lisp((defn complement [f] (fn [& args] (not (apply f args)))))lisp",
	 .takesFunctions = true},
    {.name = "constantly", .source = R"lisp((defn constantly [x] (fn [& args] x)))lisp"},
    {.name = "fnil",
	 .source = R"lisp((defn fnil [f & defaults]
  (let [size (count defaults)]
    (fn [& args]
      (apply f (map-indexed (fn [at arg] (if (and (nil? arg) (< at size)) (nth defaults at) arg)) args))))))lisp",
	 .takesFunctions = true},
    {.name = "every-pred",
	 .source = R"lisp((defn every-pred [p & ps]
  (let [preds (cons p ps)]
    (fn [& args] (every? (fn [x] (every? (fn [q] (boolean (q x))) preds)) args)))))lisp",
	 .takesFunctions = true},
    {.name = "some-fn",
	 .source = R"lisp((defn some-fn [p & ps]
  (let [preds (cons p ps)]
    (fn [& args] (some (fn [x] (some (fn [q] (q x)) preds)) args)))))lisp",
	 .takesFunctions = true},
    {.name = "max-key",
	 .source = R"lisp((defn max-key [k x & more]
  (reduce (fn [best y] (if (>= (k y) (k best)) y best)) x more)))lisp",
	 .takesFunctions = true},
    {.name = "min-key",
	 .source = R"lisp((defn min-key [k x & more]
  (reduce (fn [best y] (if (<= (k y) (k best)) y best)) x more)))lisp",
	 .takesFunctions = true},
    {.name = "run!",
	 .source = R"lisp((defn run! [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (when (< at size)
        (f (nth items at))
        (recur (inc at)))))))lisp",
	 .takesFunctions = true},
});

} // namespace

void defineFunctions(Machine& machine) {
	machine.defineNative("zipmap", zipMap);
	machine.defineNative("%map-of", mapOfKeys);
	machine.defineNative("hash-map", hashMap);
	machine.defineNative("frequencies", frequencies);
	machine.defineNative("%group-by", groupBy);
	machine.defineNative("select-keys", selectKeys);
	defineLibrary(machine, functions);
}

} // namespace affresco::lisp
