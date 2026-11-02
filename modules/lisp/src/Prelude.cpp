/// @file
/// @brief The language's functions written in itself. Those that call a function back live here, so that they run on
/// the machine's stack: a recursion through `map` ends at the call-depth limit like any other, never in a native
/// overflow. They see the language's own functions as constants, so a program redefining `count` doesn't change
/// `reduce`.

#include "Compiler.hpp"
#include "Machine.hpp"
#include "Reader.hpp"

#include <string>
#include <string_view>

namespace affresco::lisp {

namespace {

/// The functions, in the order they use each other.
constexpr std::string_view prelude = R"lisp(
(defn identity [x] x)

(defn second [coll] (first (rest coll)))

(defn last [coll]
  (let [items (%seq coll) size (count items)]
    (if (> size 0) (nth items (- size 1)))))

(defn reduce [f start & more]
  (let [seeded (not (empty? more))
        items (%seq (if seeded (first more) start))
        size (count items)]
    (loop [result (if seeded start (nth items 0 nil))
           at (if seeded 0 1)]
      (if (< at size)
        (recur (f result (nth items at)) (inc at))
        result))))

(defn map [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (recur (conj result (f (nth items at))) (inc at))
        result))))

(defn map-indexed [f coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (recur (conj result (f at (nth items at))) (inc at))
        result))))

(defn filter [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (< at size)
        (let [item (nth items at)]
          (recur (if (pred item) (conj result item) result) (inc at)))
        result))))

(defn remove [pred coll] (filter (fn [item] (not (pred item))) coll))

(defn find [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (when (< at size)
        (let [item (nth items at)]
          (if (pred item) item (recur (inc at))))))))

(defn some [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (when (< at size)
        (or (pred (nth items at)) (recur (inc at)))))))

(defn every? [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [at 0]
      (if (< at size)
        (if (pred (nth items at)) (recur (inc at)) false)
        true))))

(defn mapcat [f coll] (apply concat (map f coll)))

(defn take [size coll]
  (let [items (%seq coll) most (min size (count items))]
    (loop [result [] at 0]
      (if (< at most)
        (recur (conj result (nth items at)) (inc at))
        result))))

(defn drop [size coll]
  (let [items (%seq coll) end (count items)]
    (loop [result [] at (max size 0)]
      (if (< at end)
        (recur (conj result (nth items at)) (inc at))
        result))))

(defn zipmap [keys values]
  (let [names (%seq keys) items (%seq values) most (min (count names) (count items))]
    (loop [result {} at 0]
      (if (< at most)
        (recur (assoc result (nth names at) (nth items at)) (inc at))
        result))))

(defn sort-by [key coll]
  (let [items (vec coll)]
    (%sort-by-keys (map key items) items)))

(defn update [target key f & more]
  (assoc target key (apply f (get target key) more)))

(defn get-in [target path & fallback]
  (let [names (%seq path) size (count names)]
    (loop [at 0 here target]
      (cond
        (= at size) here
        (nil? here) (first fallback)
        :else (recur (inc at) (get here (nth names at)))))))

(defn assoc-in [target path value]
  (let [key (first path) more (rest path)]
    (if (empty? more)
      (assoc target key value)
      (assoc target key (assoc-in (get target key) more value)))))

(defn update-in [target path f & more]
  (assoc-in target path (apply f (get-in target path) more)))
)lisp";

} // namespace

void definePrelude(Machine& machine) {
	Reader reader(machine.heap, prelude, "<library>");
	while(const auto read = reader.next()) {
		const std::string name(read->form.at(1).text());
		CompileContext context;
		context.definition = name;
		context.library = true;
		const Code* code = Compiler(machine, context).function(read->form);
		auto* closure = machine.heap.make<ClosureObject>();
		closure->code = code;
		Global& cell = machine.global(name);
		cell.value = Value::object(Kind::Function, closure);
		cell.state = Global::State::Ready;
		cell.library = true;
	}
}

} // namespace affresco::lisp
