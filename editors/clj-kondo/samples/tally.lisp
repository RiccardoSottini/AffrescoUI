;;; tally.lisp --- a sample for the linter's configuration: a counter with hooks, in a file with an ns
;;;
;;; Not an example app: check.sh lints it and expects exactly the two mistakes marked "planted" below.
;;; The ns is for the linter, which reads a file with one and one without: the language has no ns yet, so
;;; affrescoui check stops at it (without that line, notes.screen.lisp and this file build).

(ns app.tally)

;;; Definitions

(def step-size "How much a press adds." 1)

(defstate total 0 "The tally.")

(defhook after-count "Runs after the tally changes, with the new total.")

(defmacro twice "Runs a form two times." [form]
  `(do ~form ~form))

(defpart tally "What the tally adds to the screen."
  :watch {:state.total announce}
  :onLoad [~(handler reset-tally)])

(defdefaults tally-defaults "Defaults an app may replace." :settings {:tally.step {:type number :default 1}})

(defcomponent counter "A number with a button that adds to it." {:label "Count"}
  `(row#counter :gap 4 (label#value :text ~state.total) (button#add :text ~label :onClick bump)))

;;; Logic

(defn bump "Adds a step to the tally, then runs the hook." []
  (set "state.total" (+ state.total step-size))
  (run-hooks after-count state.total))

(defn bump-twice "Adds two steps." []
  (twice (bump)))

(defn reset-tally [] "Sets the tally to zero." ; planted: the docstring comes after the parameters
  (set "state.total" 0))

(defn announce "Says the new count in the log." [& given]
  (log (str "Count: " state.total " after " (count given) " changes")))

(defn halve "Half the tally, rounded down." []
  (quot state.total totl)) ; planted: totl is defined nowhere

(defn checked "Runs a function after making sure the tally isn't negative." [f & arguments]
  (when (>= state.total 0) (apply f arguments)))

(add-hook after-count announce)
(remove-hook after-count announce)
(advice bump :around checked)

(defcommand add-step "Adds a step." :title ~(t "command.add") []
  (bump))

(defview tally-label "The tally as text, red when it is over a limit." [limit]
  `(label#tally :text ~(str state.total) :color ~(if (> state.total limit) @color.danger @color.text)))
