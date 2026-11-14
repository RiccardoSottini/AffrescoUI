/// @file
/// @brief The language's everyday forms (Machine.hpp, defineForms()): the threading forms, `if-let` and its kin,
/// `case`, `for`, `doseq`, `letfn` and the others (reference/forms.md), each a macro of the library (Library.hpp)
/// that expands to the special forms and functions the language already has. The helpers whose names start with `%`
/// do a macro's work at expansion, so each macro is a line or two.
///
/// An expansion writes a function by word only when a binding can lower it (`not`, `=`, `nil?`, `some?`, `map`,
/// `filter`, `mapcat`), and lists it in the entry's `uses`, so a local hiding it where the macro is called is refused.
/// Any other function it writes as a value (`(list reduce ...)`, `` `(~first ...) ``): the language's own, which no
/// program can change.

#include "Library.hpp"
#include "Machine.hpp"

#include <array>

namespace affresco::lisp {

namespace {

/// The helpers, which work at expansion: functions of the library, compiled the first time a macro calls one.
constexpr auto helpers = std::to_array<LibraryEntry>({
    // One step of a threading form: the value as a call's first argument, or its last; a bare word is called
    {.name = "%thread", .source = R"lisp((defn %thread [step value last]
  (cond
    (not (list? step)) (list step value)
    last `(~@step ~value)
    :else `(~(first step) ~value ~@(rest step)))))lisp"},
    {.name = "%thread-all", .source = R"lisp((defn %thread-all [value steps last]
  (reduce (fn [result step] (%thread step result last)) value steps)))lisp"},
    // some-> and some->>: each step's value bound, and nil stops the chain
    {.name = "%some-thread", .source = R"lisp((defn %some-thread [value steps last]
  (let [size (count steps)
        names (map (fn [step] (gensym)) steps)]
    (if (= size 0)
      value
      (loop [at (- size 1)
             body (%thread (nth steps at) (nth names at) last)]
        (let [name (nth names at)
              guarded (list 'if (list 'nil? name) nil body)]
          (if (= at 0)
            (list 'let [name value] guarded)
            (recur (- at 1)
                   (list 'let [name (%thread (nth steps (- at 1)) (nth names (- at 1)) last)] guarded)))))))))lisp"},
    // cond-> and cond->>: the value rebound by each step whose test holds
    {.name = "%cond-thread", .source = R"lisp((defn %cond-thread [value clauses last]
  (let [name (gensym)
        size (count clauses)]
    (loop [bindings [name value] at 0]
      (if (< at size)
        (recur (conj bindings name
                     (list 'if (nth clauses at) (%thread (nth clauses (+ at 1)) name last) name))
               (+ at 2))
        (list 'let bindings name))))))lisp"},
    // if-let and its kin: [pattern value name test], the value bound to a name of its own, tested, then destructured;
    // nil when the bindings aren't one name and one value (a thrown message is the form's own, written in place)
    {.name = "%conditional-binding", .source = R"lisp((defn %conditional-binding [bindings some]
  (when (and (vector? bindings) (= (count bindings) 2))
    (let [name (gensym)]
      [(first bindings) (second bindings) name (if some (list 'some? name) name)]))))lisp"},
    // case's constants: a word or a vector is quoted, a list is alternatives (%case-test)
    {.name = "%case-constant", .source = R"lisp((defn %case-constant [constant]
  (cond
    (or (number? constant) (string? constant) (keyword? constant) (boolean? constant) (nil? constant)) constant
    (or (symbol? constant) (vector? constant)) (list 'quote constant)
    :else (throw "case takes pairs of a constant and a form, and maybe a default"))))lisp"},
    {.name = "%case-test", .source = R"lisp((defn %case-test [value constant]
  (if (list? constant)
    (apply list 'or (map (fn [one] (list '= value (%case-constant one))) constant))
    (list '= value (%case-constant constant)))))lisp"},
    // case and condp: a cond over the clauses, the value written in place when it is a word or a constant
    {.name = "%clauses", .source = R"lisp((defn %clauses [value clauses test fallback]
  (let [simple (not (or (list? value) (vector? value) (map? value)))
        name (if simple value (gensym))
        size (count clauses)
        pairs (- size (mod size 2))
        tests (loop [result [] at 0]
                (if (< at pairs)
                  (recur (conj result (test name (nth clauses at)) (nth clauses (+ at 1))) (+ at 2))
                  result))
        body (apply list 'cond (conj tests :else (if (< pairs size) (nth clauses pairs) fallback)))]
    (if simple body (list 'let [name value] body)))))lisp"},
    // for's and doseq's bindings, as [name collection options], each option [keyword value]; nil when they are none,
    // an odd number, or an option isn't :let [...], :when or :while after a name and its collection
    {.name = "%comprehension-groups", .source = R"lisp((defn %comprehension-groups [bindings]
  (let [size (if (vector? bindings) (count bindings) 0)]
    (when (and (> size 0) (= 0 (mod size 2)) (not (keyword? (first bindings))))
      (loop [groups [] at 0]
        (if (< at size)
          (let [key (nth bindings at)
                value (nth bindings (+ at 1))]
            (cond
              (not (keyword? key))
              (recur (conj groups [key value []]) (+ at 2))
              (or (= key :when) (= key :while) (and (= key :let) (vector? value)))
              (let [group (last groups)]
                (recur (conj (vec (take (- (count groups) 1) groups))
                             [(first group) (second group) (conj (nth group 2) [key value])])
                       (+ at 2)))
              :else nil))
          groups))))))lisp"},
    // A binding's collection, cut at the first item failing each :while (seeing the :let before it)
    {.name = "%while-source", .source = R"lisp((defn %while-source [name collection options]
  (loop [source collection lets [] at 0]
    (if (< at (count options))
      (let [option (nth options at)
            key (first option)
            value (second option)]
        (cond
          (= key :let) (recur source (vec (concat lets value)) (+ at 1))
          (= key :while)
          (recur (list %take-while (list 'fn [name] (if (empty? lets) value (list 'let lets value))) source)
                 lets (+ at 1))
          :else (recur source lets (+ at 1))))
      source))))lisp"},
    // A binding's :let and :when around what is inside it
    {.name = "%wrap-options", .source = R"lisp((defn %wrap-options [options inner collect]
  (reduce (fn [result option]
            (let [key (first option) value (second option)]
              (cond
                (= key :let) (list 'let value result)
                (= key :when) (if collect (list 'if value result []) (list 'when value result))
                :else result)))
          inner (reverse options))))lisp"},
    {.name = "%comprehension-level", .source = R"lisp((defn %comprehension-level [groups at body collect]
  (let [group (nth groups at)
        name (first group)
        options (nth group 2)
        source (%while-source name (second group) options)
        innermost (= at (- (count groups) 1))
        inner (cond (not innermost) (%comprehension-level groups (+ at 1) body collect)
                    collect [body]
                    :else body)]
    (cond
      (and collect innermost (every? (fn [option] (not (= (first option) :let))) options))
      (list 'map (list 'fn [name] body)
            (reduce (fn [result option]
                      (if (= (first option) :when) (list 'filter (list 'fn [name] (second option)) result) result))
                    source options))
      collect (list 'mapcat (list 'fn [name] (%wrap-options options inner true)) source)
      :else (list reduce (list 'fn [(gensym) name] (%wrap-options options inner false) nil) nil source)))))lisp"},
    // letfn's group: several functions that call each other, each given the group as a hidden argument
    {.name = "%letfn-caller", .source = R"lisp((defn %letfn-caller [group at parameters]
  (if (some (fn [parameter] (= parameter '&)) parameters)
    (let [arguments (gensym)]
      (list 'fn ['& arguments] (list apply (list (list nth group at) group) arguments)))
    (let [arguments (vec (map (fn [parameter] (gensym)) parameters))]
      (list 'fn arguments (apply list (list (list nth group at) group) arguments))))))lisp"},
    {.name = "%letfn", .source = R"lisp((defn %letfn [bindings body]
  (when (not (and (vector? bindings)
                  (every? (fn [binding] (and (list? binding) (> (count binding) 1) (symbol? (first binding))
                                             (vector? (second binding))))
                          bindings)))
    (throw "letfn takes a vector of (name [params] body...)"))
  (if (= (count bindings) 1)
    (let [binding (first bindings)]
      `(let [~(first binding) (fn ~@binding)] ~@body))
    (let [group (gensym)
          callers (vec (mapcat (fn [at]
                                 (let [binding (nth bindings at)]
                                   [(first binding) (%letfn-caller group at (second binding))]))
                               (range (count bindings))))
          makers (vec (map (fn [binding] `(fn [~group] (let ~callers (fn ~@(rest binding))))) bindings))]
      `(let [~group ~makers ~@callers] ~@body)))))lisp"},
    {.name = "%take-while",
	 .source = R"lisp((defn %take-while [pred coll]
  (let [items (%seq coll) size (count items)]
    (loop [result [] at 0]
      (if (and (< at size) (pred (nth items at)))
        (recur (conj result (nth items at)) (inc at))
        result)))))lisp",
	 .takesFunctions = true},
});

/// The forms, each a macro of the library.
constexpr auto forms = std::to_array<LibraryEntry>({
    {.name = "->", .source = R"lisp((defmacro -> [value & steps] (%thread-all value steps false)))lisp", .macro = true},
    {.name = "->>",
	 .source = R"lisp((defmacro ->> [value & steps] (%thread-all value steps true)))lisp",
	 .macro = true},
    {.name = "as->",
	 .source = R"lisp((defmacro as-> [value name & steps]
  (when (not (symbol? name)) (throw "as-> takes a value, a name and forms"))
  `(let [~name ~value ~@(mapcat (fn [step] [name step]) steps)] ~name)))lisp",
	 .macro = true},
    {.name = "some->",
	 .source = R"lisp((defmacro some-> [value & steps] (%some-thread value steps false)))lisp",
	 .macro = true,
	 .uses = "nil?"},
    {.name = "some->>",
	 .source = R"lisp((defmacro some->> [value & steps] (%some-thread value steps true)))lisp",
	 .macro = true,
	 .uses = "nil?"},
    {.name = "cond->",
	 .source = R"lisp((defmacro cond-> [value & clauses]
  (when (not (= 0 (mod (count clauses) 2))) (throw "cond-> takes a value and pairs of a test and a form"))
  (%cond-thread value clauses false)))lisp",
	 .macro = true},
    {.name = "cond->>",
	 .source = R"lisp((defmacro cond->> [value & clauses]
  (when (not (= 0 (mod (count clauses) 2))) (throw "cond->> takes a value and pairs of a test and a form"))
  (%cond-thread value clauses true)))lisp",
	 .macro = true},
    {.name = "if-not",
	 .source = R"lisp((defmacro if-not [test then & else]
  (when (> (count else) 1) (throw "if-not takes a test, a form, and maybe another"))
  `(if (not ~test) ~then ~@else)))lisp",
	 .macro = true,
	 .uses = "not"},
    {.name = "when-not",
	 .source = R"lisp((defmacro when-not [test & body] `(when (not ~test) ~@body)))lisp",
	 .macro = true,
	 .uses = "not"},
    {.name = "if-let",
	 .source = R"lisp((defmacro if-let [bindings then & else]
  (when (> (count else) 1) (throw "if-let takes a vector of one name and one value, a form, and maybe another"))
  (let [[pattern value name test] (or (%conditional-binding bindings false)
                                      (throw "if-let takes a vector of one name and one value"))]
    `(let [~name ~value] (if ~test (let [~pattern ~name] ~then) ~@else)))))lisp",
	 .macro = true},
    {.name = "when-let",
	 .source = R"lisp((defmacro when-let [bindings & body]
  (let [[pattern value name test] (or (%conditional-binding bindings false)
                                      (throw "when-let takes a vector of one name and one value"))]
    `(let [~name ~value] (when ~test (let [~pattern ~name] ~@body))))))lisp",
	 .macro = true},
    {.name = "if-some",
	 .source = R"lisp((defmacro if-some [bindings then & else]
  (when (> (count else) 1) (throw "if-some takes a vector of one name and one value, a form, and maybe another"))
  (let [[pattern value name test] (or (%conditional-binding bindings true)
                                      (throw "if-some takes a vector of one name and one value"))]
    `(let [~name ~value] (if ~test (let [~pattern ~name] ~then) ~@else)))))lisp",
	 .macro = true,
	 .uses = "some?"},
    {.name = "when-some",
	 .source = R"lisp((defmacro when-some [bindings & body]
  (let [[pattern value name test] (or (%conditional-binding bindings true)
                                      (throw "when-some takes a vector of one name and one value"))]
    `(let [~name ~value] (when ~test (let [~pattern ~name] ~@body))))))lisp",
	 .macro = true,
	 .uses = "some?"},
    {.name = "when-first",
	 .source = R"lisp((defmacro when-first [bindings & body]
  (let [[pattern value name] (or (%conditional-binding bindings false)
                                      (throw "when-first takes a vector of one name and one value"))]
    `(let [~name ~value] (when (not (~empty? ~name)) (let [~pattern (~first ~name)] ~@body))))))lisp",
	 .macro = true,
	 .uses = "not"},
    {.name = "case",
	 .source = R"lisp((defmacro case [value & clauses]
  (%clauses value clauses %case-test '(throw "no clause of case matches"))))lisp",
	 .macro = true,
	 .uses = "="},
    {.name = "condp",
	 .source = R"lisp((defmacro condp [pred value & clauses]
  (when (some (fn [clause] (and (keyword? clause) (= clause (keyword ">>")))) clauses)
    (throw "condp's :>> isn't supported: bind the value with let"))
  (if (list? pred)
    (let [name (gensym)]
      `(let [~name ~pred]
         ~(%clauses value clauses (fn [item constant] (list name constant item))
                    '(throw "no clause of condp matches"))))
    (%clauses value clauses (fn [item constant] (list pred constant item)) '(throw "no clause of condp matches")))))lisp",
	 .macro = true},
    {.name = "for",
	 .source = R"lisp((defmacro for [bindings body]
  (%comprehension-level (or (%comprehension-groups bindings) (throw "for takes a vector of names and collections")) 0 body true)))lisp",
	 .macro = true,
	 .uses = "map filter mapcat"},
    {.name = "doseq",
	 .source = R"lisp((defmacro doseq [bindings & body]
  (list 'do (%comprehension-level (or (%comprehension-groups bindings) (throw "doseq takes a vector of names and collections")) 0 `(do ~@body) false) nil)))lisp",
	 .macro = true},
    {.name = "dotimes",
	 .source = R"lisp((defmacro dotimes [bindings & body]
  (when (not (and (vector? bindings) (= (count bindings) 2) (symbol? (first bindings))))
    (throw "dotimes takes [name count]"))
  (let [name (first bindings) end (gensym)]
    `(let [~end ~(second bindings)]
       (loop [~name 0] (when (~< ~name ~end) ~@body (recur (~inc ~name))))))))lisp",
	 .macro = true},
    {.name = "letfn", .source = R"lisp((defmacro letfn [bindings & body] (%letfn bindings body)))lisp", .macro = true},
    {.name = "doto",
	 .source = R"lisp((defmacro doto [value & steps]
  (let [name (gensym)]
    `(let [~name ~value] ~@(map (fn [step] (%thread step name false)) steps) ~name))))lisp",
	 .macro = true},
    {.name = "comment", .source = R"lisp((defmacro comment [& forms] nil))lisp", .macro = true},
    {.name = "assert",
	 .source = R"lisp((defmacro assert [test & message]
  (when (> (count message) 1) (throw "assert takes a test and maybe a message"))
  (let [text (first message)]
    `(when (not ~test)
       (throw ~(cond (empty? message) "Assert failed"
                     (string? text) (str "Assert failed: " text)
                     :else (list str "Assert failed: " text)))))))lisp",
	 .macro = true,
	 .uses = "not"},
    {.name = "defn-", .source = R"lisp((defmacro defn- [name & more] `(defn ~name ~@more)))lisp", .macro = true},
    {.name = "defonce", .source = R"lisp((defmacro defonce [name value] `(def ~name ~value)))lisp", .macro = true},
});

} // namespace

void defineForms(Machine& machine) {
	defineLibrary(machine, helpers);
	defineLibrary(machine, forms);
}

} // namespace affresco::lisp
