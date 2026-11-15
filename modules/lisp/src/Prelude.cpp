/// @file
/// @brief The language's functions written in itself. Those that call a function back live here, so that they run on
/// the machine's stack: a recursion through `map` ends at the call-depth limit like any other, never in a native
/// overflow. They are entries of the library (Library.hpp), compiled on first use, and see the language's own
/// functions, so a program redefining `count` doesn't change `reduce`.

#include "Library.hpp"
#include "Machine.hpp"

#include <array>
#include <initializer_list>
#include <string_view>

namespace affresco::lisp {

namespace {

/// The functions, in the order they use each other.
constexpr auto prelude = std::to_array<LibraryEntry>({
    {.name = "identity", .source = R"lisp((defn identity [x] x))lisp"},
    {.name = "second", .source = R"lisp((defn second [coll] (first (rest coll))))lisp"},
    {.name = "last", .source = R"lisp((defn last [coll]
  (let [items (%seq coll) size (count items)]
    (if (> size 0) (nth items (- size 1))))))lisp"},
    {.name = "reduce",
	 .source = R"lisp((defn reduce [f start & more]
  (let [seeded (not (empty? more))
        items (%seq (if seeded (first more) start))
        size (count items)]
    (if (and (not seeded) (= size 0))
      (f)
      (loop [result (if seeded start (nth items 0))
             at (if seeded 0 1)]
        (if (< at size)
          (recur (f result (nth items at)) (inc at))
          result))))))lisp",
	 .takesFunctions = true},
    {.name = "map",
	 .source = R"lisp((defn map [f coll & colls]
  (if (empty? colls)
    (let [items (%seq coll) size (count items)]
      (loop [result [] at 0]
        (if (< at size)
          (recur (conj result (f (nth items at))) (inc at))
          result)))
    (let [columns (%columns (cons coll colls)) size (count columns)]
      (loop [result [] at 0]
        (if (< at size)
          (recur (conj result (apply f (nth columns at))) (inc at))
          result))))))lisp",
	 .takesFunctions = true},
    {.name = "map-indexed",
	 .source = R"lisp((defn map-indexed [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (recur (conj result (f at (nth items at))) (inc at))
        result)))))lisp",
	 .takesFunctions = true},
    {.name = "filter",
	 .source = R"lisp((defn filter [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (let [item (nth items at)]
          (recur (if (pred item) (conj result item) result) (inc at)))
        result)))))lisp",
	 .takesFunctions = true},
    {.name = "remove",
	 .source = R"lisp((defn remove [pred coll] (filter (fn [item] (not (pred item))) coll)))lisp",
	 .takesFunctions = true},
    {.name = "find",
	 .source = R"lisp((defn find [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (when (< at size)
        (let [item (nth items at)]
          (if (pred item) item (recur (inc at)))))))))lisp",
	 .takesFunctions = true},
    {.name = "some",
	 .source = R"lisp((defn some [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (when (< at size)
        (or (pred (nth items at)) (recur (inc at))))))))lisp",
	 .takesFunctions = true},
    {.name = "every?",
	 .source = R"lisp((defn every? [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (if (< at size)
        (if (pred (nth items at)) (recur (inc at)) false)
        true)))))lisp",
	 .takesFunctions = true},
    {.name = "mapcat",
	 .source = R"lisp((defn mapcat [f coll & colls] (apply concat (apply map f coll colls))))lisp",
	 .takesFunctions = true},
    {.name = "take", .source = R"lisp((defn take [size coll]
  (let [items (%seq coll) most (min size (count items))]
    (loop [result [] at 0]
      (if (< at most)
        (recur (conj result (nth items at)) (inc at))
        result)))))lisp"},
    {.name = "drop", .source = R"lisp((defn drop [size coll]
  (let [items (%seq coll) end (count items)]
    (loop [result [] at (max size 0)]
      (if (< at end)
        (recur (conj result (nth items at)) (inc at))
        result)))))lisp"},
    {.name = "sort",
	 .source = R"lisp((defn sort [a & more]
  (cond
    (empty? more) (%sort a)
    (or (%sorts-natively? a) (> (count more) 1)) (apply %sort a more)
    :else
    (let [comparator a items (vec (first more)) size (count items)]
      (loop [width 1 run items]
        (if (>= width size)
          run
          (recur (* width 2)
            (loop [start 0 merged []]
              (if (>= start size)
                merged
                (let [middle (min (+ start width) size) end (min (+ middle width) size)]
                  (recur end
                    (loop [left start right middle result merged]
                      (cond
                        (and (< left middle) (< right end))
                        (let [l (nth run left) r (nth run right)]
                          (if (%before? (comparator r l))
                            (recur left (inc right) (conj result r))
                            (recur (inc left) right (conj result l))))
                        (< left middle) (recur (inc left) right (conj result (nth run left)))
                        (< right end) (recur left (inc right) (conj result (nth run right)))
                        :else result)))))))))))))lisp",
	 .takesFunctions = true},
    {.name = "sort-by",
	 .source = R"lisp((defn sort-by [key a & more]
  (let [comparator (if (empty? more) nil a)
        items (vec (if (empty? more) a (first more)))
        keys (map key items)]
    (cond
      (nil? comparator) (%sort-by-keys keys items)
      (%sorts-natively? comparator) (%sort-by-keys comparator keys items)
      :else (map second (sort (fn [x y] (comparator (first x) (first y))) (map vector keys items)))))))lisp",
	 .takesFunctions = true},
    {.name = "update",
	 .source = R"lisp((defn update [target key f & more]
  (assoc target key (apply f (get target key) more))))lisp",
	 .takesFunctions = true},
    {.name = "get-in", .source = R"lisp((defn get-in [target path & fallback]
  (let [names (%seq path) size (count names)]
    (loop [at 0 here target]
      (cond
        (= at size) here
        (nil? here) (first fallback)
        :else (recur (inc at) (get here (nth names at))))))))lisp"},
    {.name = "assoc-in", .source = R"lisp((defn assoc-in [target path value]
  (let [key (first path) more (rest path)]
    (if (empty? more)
      (assoc target key value)
      (assoc target key (assoc-in (get target key) more value))))))lisp"},
    {.name = "update-in",
	 .source = R"lisp((defn update-in [target path f & more]
  (assoc-in target path (apply f (get-in target path) more))))lisp",
	 .takesFunctions = true},
});

} // namespace

void definePrelude(Machine& machine) {
	defineLibrary(machine, prelude);
	// The language's functions that take functions, written natively or by a host
	for(const std::string_view name : {"apply", "run-hooks"}) machine.global(name).takesFunctions = true;
}

} // namespace affresco::lisp
