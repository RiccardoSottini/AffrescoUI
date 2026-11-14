; The conformance run's shim (README.md): what the suite's files ask of clojure.test and of their own portability
; namespace, written in the language. The runner loads it into each file's interpreter and registers both namespaces
; as bare, so `t/is`, `p/thrown?` and `is` are the names below. Each assertion reports once, through the runner's
; primitive `%report`: (%report line status) for a pass, (%report line "fail" expected actual), (%report line "error"
; message), (%report line "out" reason). The token pass (prepare.cpp) has written each assertion's line into it, so
; `(is x)` arrives as `(cf--is 12 x)`, and has put `(%out "reason")` where the language has no value.

(defn cf--out-reason "The reason a quoted form is out of scope: the first (%out reason) inside it, or nil." [form]
  (cond
    (and (list? form) (= (first form) '%out)) (second form)
    (or (list? form) (vector? form)) (some cf--out-reason form)
    (map? form) (some cf--out-reason (concat (keys form) (vals form)))
    :else nil))

(defn cf--one-of "Whether a text is one of several." [text texts]
  (some (fn [each] (= each text)) texts))

(defn cf--word "A form's word as text, or nil when it isn't a word." [form]
  (when (symbol? form) (str form)))

(defn cf--thrown-kind "How a form expects an error: :class for (thrown? Class body...) and (thrown-with-msg? Class
  message body...), :any for the portability namespace's (p/thrown? body...), else nil." [form]
  (let [word (when (list? form) (cf--word (first form)))]
    (cond
      (nil? word) nil
      (cf--one-of word ["p/thrown?" "clojure.core-test.portability/thrown?"]) :any
      (cf--one-of word ["thrown?" "t/thrown?" "clojure.test/thrown?"]) :class
      (cf--one-of word ["thrown-with-msg?" "t/thrown-with-msg?" "clojure.test/thrown-with-msg?"]) :message
      :else nil)))

(defn cf--equality? "Whether a form is (= a b ...), whose values a failure shows." [form]
  (and (list? form) (> (count form) 2) (cf--one-of (cf--word (first form)) ["=" "clojure.core/="])))

(defn cf--message "The text of a caught condition." [condition]
  (cond
    (string? condition) condition
    (map? condition) (str (or (:message condition) (pr-str condition)))
    :else (pr-str condition)))

(defn cf--out-raised "The reason in the message of a condition (%out reason) raised, or nil." [message]
  (let [at (str/index-of message "out of scope: ")]
    (when at (subs message (+ at 14)))))

(defn cf--undefined-name? "Whether a condition's message says a name isn't defined." [message]
  (str/includes? message "is not defined"))

(defn cf--fail-or-out "Reports a caught condition: out of scope when (%out reason) raised it, else as an error."
  [line condition]
  (let [message (cf--message condition)]
    (if (cf--out-raised message)
      (%report line "out" (cf--out-raised message))
      (%report line "error" message))))

(defn cf--check "Runs an assertion's form and reports whether it held." [line quoted thunk]
  (try
    (if (thunk)
      (%report line "pass")
      (%report line "fail" (pr-str quoted) "false"))
    (catch condition (cf--fail-or-out line condition))))

(defn cf--check-equal "Runs (= a b ...) and reports whether it held, with the values when it doesn't." [line quoted thunk]
  (try
    (let [values (thunk)]
      (if (apply = values)
        (%report line "pass")
        (%report line "fail" (pr-str quoted) (pr-str (cons 'not (list (cons '= values)))))))
    (catch condition (cf--fail-or-out line condition))))

(defn cf--check-thrown "Runs a body that should raise an error, and reports whether it did. A name that isn't defined is
  an error, not the error the assertion means." [line quoted thunk]
  (let [outcome (try [:value (thunk)] (catch condition [:raised (cf--message condition)]))]
    (cond
      (= (first outcome) :value) (%report line "fail" (pr-str quoted) (pr-str (second outcome)))
      (cf--out-raised (second outcome)) (%report line "out" (cf--out-raised (second outcome)))
      (cf--undefined-name? (second outcome)) (%report line "error" (second outcome))
      :else (%report line "pass"))))

(defmacro cf--is "One assertion, at a line: out of scope without running when the form holds (%out reason)." [line form
                                                                                                          & message]
  (let [reason (cf--out-reason form)
        kind (cf--thrown-kind form)]
    (cond
      reason `(%report ~line "out" ~reason)
      (= kind :any) `(cf--check-thrown ~line '~form (fn [] ~@(rest form)))
      (= kind :class) `(cf--check-thrown ~line '~form (fn [] ~@(drop 2 form)))
      (= kind :message) `(cf--check-thrown ~line '~form (fn [] ~@(drop 3 form)))
      (cf--equality? form) `(cf--check-equal ~line '~form (fn [] [~@(rest form)]))
      :else `(cf--check ~line '~form (fn [] ~form)))))

(defmacro is "An assertion the token pass didn't see: its line is unknown." [form & message]
  `(cf--is 0 ~form ~@message))

(defn cf--substitute "A template with each parameter's word replaced by its value, as clojure.template does." [form
                                                                                                           values]
  (cond
    (and (symbol? form) (contains? values (str form))) (get values (str form))
    (list? form) (apply list (map (fn [item] (cf--substitute item values)) form))
    (vector? form) (mapv (fn [item] (cf--substitute item values)) form)
    (map? form) (zipmap (map (fn [item] (cf--substitute item values)) (keys form))
                        (map (fn [item] (cf--substitute item values)) (vals form)))
    :else form))

(defmacro cf--are "One assertion per row: (are [x y] expr row...), each row's values put in the template." [line
                                                                                                        parameters
                                                                                                        template
                                                                                                        & rows]
  (let [width (count parameters)]
    (if (or (= width 0) (not= 0 (rem (count rows) width)))
      `(%report ~line "error" "are: the rows don't fill the parameters")
      `(do ~@(map (fn [row] `(cf--is ~line ~(cf--substitute template (zipmap (map str parameters) row))))
                  (partition width rows))))))

(defmacro are "Rows the token pass didn't see: their line is unknown." [parameters template & rows]
  `(cf--are 0 ~parameters ~template ~@rows))

(defmacro testing "Assertions under a text, which their reports carry." [text & body]
  `(do (%enter ~text)
       (try (do ~@body) (finally (%leave nil)))))

(defmacro deftest "A test: a function of no arguments the runner calls after the file's forms." [name & body]
  `(defn ~(symbol (str "test--" name)) [] ~@body))

(defmacro when-var-exists "Its body when the name is defined, else a report that the function isn't there." [name
                                                                                                             & body]
  (if (%defined? name) `(do ~@body) `(%absent '~name)))

(defn big-int? "Whether a number is the dialect's integer of any size: as for the suite's dialects with one kind of
  integer (cljs, lpy, phel), any whole number." [n]
  (integer? n))

(defn sleep "Waits: the language has no clock, so it doesn't." [ms] nil)

(defn lazy-seq? "Whether a value is a lazy sequence: nothing is lazy." [x] false)

(defn string-encoding "How text is counted: in characters of UTF-8." [] :utf8)
