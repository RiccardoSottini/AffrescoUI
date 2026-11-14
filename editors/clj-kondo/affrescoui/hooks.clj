(ns affrescoui.hooks
  "How the linter reads AffrescoUI Lisp's definition forms (config.edn, :hooks :macroexpand).

  Each macro below rewrites one definition form into the plain Clojure the linter knows, for the linter only: the
  language itself never reads this file. A view or a command becomes a defn whose body is the code its templates work
  out (what each ~ and ~@ holds), so the code is linted and the template's data is not. A definition whose body is
  data or a template for the host (defscreen, defstate, defstyle...) becomes a def of its name, so its body is not
  linted at all."
  (:require [clojure.string :as string]))

(def ^:private quote-heads
  "The heads a quoted or backquoted form has, as the linter hands it over."
  '#{quote syntax-quote clojure.core/syntax-quote})

(def ^:private unquote-heads
  "The heads of ~ and ~@."
  '#{unquote unquote-splicing clojure.core/unquote clojure.core/unquote-splicing})

(def ^:private binding-heads
  "The forms that bind names, whose names a dotted word inside them may start with."
  '#{fn fn* let loop for doseq dotimes when-let if-let when-some if-some when-first letfn})

(defn- head
  "The first item of a list, or nil for anything else."
  [form]
  (when (seq? form) (first form)))

(defn- binding-names
  "The names a binding pattern binds: [a b & more :as all], {:keys [a b] :or {a 1}}."
  [pattern]
  (cond
    (symbol? pattern) (if (= '& pattern) [] [pattern])
    (map? pattern) (mapcat binding-names (concat (keys pattern) (vals pattern)))
    (coll? pattern) (mapcat binding-names pattern)
    :else []))

(defn- bound-by
  "The names a binding form binds: its first vector, and the first vector of each of its arities."
  [form]
  (let [items (rest form)
        vector-of (fn [items] (first (filter vector? items)))
        arities (filter #(and (seq? %) (vector? (first %))) items)]
    (concat (binding-names (vector-of items)) (mapcat (comp binding-names first) arities))))

(defn- path-to-local
  "A dotted word that reads a local's member (task.done, with task a local) as the local itself, so the local counts
  as used; any other word as it is. A path of the app (state.count) stays, and config.edn tells the linter it is known."
  [word locals]
  (let [text (name word)
        dot (string/index-of text ".")]
    (if (and (nil? (namespace word)) dot (pos? dot) (contains? locals (symbol (subs text 0 dot))))
      (symbol (subs text 0 dot))
      word)))

(declare as-code)

(defn- worked-out
  "The forms a template works out, as code: what each ~ and ~@ in it holds. The rest of a template is data."
  [template locals]
  (cond
    (contains? unquote-heads (head template)) [(as-code (second template) locals)]
    (map? template) (mapcat #(worked-out % locals) (concat (keys template) (vals template)))
    (coll? template) (mapcat #(worked-out % locals) template)
    :else []))

(defn- repeat-as-let
  "A view's repeat, (repeat [card cards :index i :key form] body...), as the let it reads as: card and i named."
  [[_ [item collection & options] & body] locals]
  (let [{index :index key-form :key} (apply hash-map options)
        names (cond-> [item] index (conj index))
        inner (into locals (mapcat binding-names names))]
    (list 'let (cond-> [item (as-code collection locals)] index (conj index 0))
          (vec (map #(as-code % inner) (cond->> body key-form (cons key-form)))))))

(defn- list-as-code
  "A list as code: a template inside it as what it works out, a view's repeat as a let, a handler as the function and
  its arguments, and every other list with its items as code."
  [form locals]
  (let [first-item (head form)]
    (cond
      (contains? quote-heads first-item) (vec (worked-out (second form) locals))
      (contains? unquote-heads first-item) (as-code (second form) locals)
      (and (= 'repeat first-item) (vector? (second form))) (repeat-as-let form locals)
      (= 'handler first-item) (vec (map #(as-code % locals) (rest form)))
      :else (let [inner (if (contains? binding-heads first-item) (into locals (bound-by form)) locals)]
              (apply list (map #(as-code % inner) form))))))

(defn- as-code
  "A form of a view's or a command's code, rewritten for the linter (list-as-code(), path-to-local())."
  [form locals]
  (cond
    (symbol? form) (path-to-local form locals)
    (seq? form) (list-as-code form locals)
    (map? form) (into {} (map (fn [[k v]] [(as-code k locals) (as-code v locals)]) form))
    (vector? form) (mapv #(as-code % locals) form)
    (set? form) (set (map #(as-code % locals) form))
    :else form))

(defn- split-definition
  "A view's or a command's parts: its name, its docstring if any, the options before its parameters, its parameters
  and its body."
  [[definition-name & more]]
  (let [docstring (when (string? (first more)) (first more))
        more (if docstring (rest more) more)
        options (take-while (complement vector?) more)
        [parameters & body] (drop-while (complement vector?) more)]
    {:definition-name definition-name :docstring docstring :options options :parameters (or parameters []) :body body}))

(defn- as-function
  "A view or a command as the defn the linter reads: the code its options and its body work out."
  [arguments]
  (let [{:keys [definition-name docstring options parameters body]} (split-definition arguments)
        locals (set (binding-names parameters))
        option-code (vec (worked-out options locals))
        body-code (mapv #(as-code % locals) body)]
    (concat (list 'defn definition-name)
            (when docstring [docstring])
            [parameters]
            (if (seq option-code)
              [(list 'let ['_ option-code] body-code)]
              [body-code]))))

(defmacro defview
  "(defview name \"doc\" [params] body...): a function whose templates are linted for the code they hold."
  [& arguments]
  (as-function arguments))

(defmacro defcommand
  "(defcommand name \"doc\" :key value ... [params] body...): a function, with the code of its keys' templates."
  [& arguments]
  (as-function arguments))

(defmacro definition
  "A definition whose body is data or a template for the host (defscreen, defstate, defstyle...): its name only."
  [definition-name & _]
  (list 'def definition-name nil))

(defmacro defkey
  "(defkey \"chord\" \"doc\"? what): the code its template works out; a word in it is an action's name, not code."
  [& arguments]
  (vec (worked-out arguments #{})))
