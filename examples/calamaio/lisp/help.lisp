;;; help.lisp --- C-h: what a key, a command, a variable or a mode does, in a *Help* buffer beside the selected window
;;;
;;; Help is worked out from the document itself: its commands (the `commands` variable), its keys (`keybindings`) and
;;; its settings (`settingsList`), so what it says is always what Calamaio does now.

;;; What help reads

(defcomputed keyList
  "Every key binding: its key, as it is shown, the command it runs and where it is bound." keybindings)

(defcomputed commandList "Every command: its name, title, category and description." commands)

(defcomputed variables "Every variable: its key, value, default, type and description." settingsList)

;;; The *Help* buffer

(defn help-in "Shows the *Help* buffer in a window, from its first line." [x]
  (set (str "state.win." x ".buf") "bh")
  (set (str "state.win." x ".pt") {:line 1 :column 1})
  (set (str "state.win." x ".caret") {:line 1 :column 1})
  (after 40 help-shown x))

(defn help-shown "Puts point at the start of the help window, and the focus back on the selected one." [x]
  (goto-point (str "#ed" x) {:line 1 :column 1})
  (focus computed.ed))

(defn help-show
  "Shows a text in the *Help* buffer, in the window after the selected one (split in two if it is alone)." [text]
  (set "state.bufs.bh" {:name "*Help*" :mode "help" :readOnly true :text text})
  (when (not (contains state.order "bh"))
    (set "state.order" (concat state.order ["bh"])))
  (when (= (count computed.wins) 1)
    (split-window true))
  (let [wins computed.wins]
    (help-in (get wins (mod (inc (indexOf wins state.sel)) (count wins)))))
  (message (t "echo.helpWindow")))

;;; Commands

(defn command-named "The command of a name, as the commands variable describes it." [f]
  (find (fn [c] (= (:name c) f)) computed.commandList))

(defn keys-of "The keys that run a command, as they are shown." [f]
  (map (fn [b] (:keyLabel b)) (filter (fn [b] (= (:command b) f)) computed.keyList)))

(defn ask-function "C-h f: asks for a command's name, to describe it." []
  (minibuffer-read "function" (t "prompt.describeFunction") "" ""))

(defn mb-function "Describes the command named." [value]
  (if (contains computed.commandNames value)
    (help-function value)
    (message (t "echo.noCommand" {:name value}))))

(defn help-function "Describes a command: its category, its keys, what it does and its title." [f]
  (let [c (command-named f)
        found (keys-of f)]
    (help-show (t "help.function"
                  {:name f
                   :category (:category c)
                   :keys (if (empty? found) (t "help.noKey" {:name f}) (t "help.keys" {:keys (join ", " found)}))
                   :description (:description c)
                   :title (:title c)}))))

;;; Variables

(defn ask-variable "C-h v: asks for a variable's name, to describe it." []
  (minibuffer-read "variable" (t "prompt.describeVariable") "" ""))

(defn mb-variable "Describes the variable named." [value]
  (let [s (find (fn [it] (= (:key it) value)) computed.variables)]
    (if (some? s)
      (help-variable s)
      (message (t "echo.noVariable" {:name value})))))

(defn help-variable "Describes a variable: its value, its default, what it changes and what it can be." [s]
  (help-show (t "help.variable"
                {:name (:key s)
                 :value (json (:value s))
                 :default (if (:modified s) (t "help.changedFrom" {:default (json (:default s))}) (t "help.isDefault"))
                 :description (:description s)
                 :type (str (:type s)
                            (if (:enum s) (t "help.oneOf" {:values (join ", " (:enum s))}) "")
                            (if (some? (:minimum s))
                              (t "help.range" {:from (str (:minimum s)) :to (str (:maximum s))})
                              ""))})))

;;; Keys

(defn ask-key "C-h k: reads the next key, to describe it." []
  (set "state.keyread" {:on true :seq ""})
  (set "state.echo" "")
  (after 0 focus-key-reader))

(defn focus-key-reader "Gives the key reader the focus, so the next key goes to it." []
  (focus "#keyReader"))

(defn key-read-end "Stops reading a key, and gives the selected window back its focus." []
  (set "state.keyread.on" false)
  (focus computed.ed))

(defn key-read
  "A key while C-h k reads: a whole binding is described, a prefix waits for more, anything else has none." [event]
  (let [sequence (trim (str state.keyread.seq " " event.key))
        bound (find (fn [b] (= (:key b) sequence)) computed.keyList)]
    (set "state.keyread.seq" sequence)
    (cond (or (= sequence "ctrl+g") (= sequence "escape"))
          (do (key-read-end) (set "state.echo" (t "echo.quit")))
          (some? bound)
          (do (key-read-end) (help-key bound))
          (some (fn [b] (starts-with? (:key b) (str sequence " "))) computed.keyList)
          nil
          :else
          (do (key-read-end) (message (t "echo.noBinding" {:keys (keyLabel sequence "letters")}))))))

(defn help-key "Describes a key: the command it runs and where it is bound." [b]
  (help-show (t "help.key"
                {:keys (:keyLabel b)
                 :name (:command b)
                 :where (if (= (:scope b) "screen") (t "help.documentKeys") (t "help.scopeKeys" {:scope (:scope b)}))
                 :description (or (:description (command-named (:command b))) "")})))

(defcommand describe-bindings "Lists every key and the command it runs."
  :title ~(t "command.describe-bindings") :category ~(t "category.help")
  :description ~(t "command.describe-bindings.about")
  []
  (help-show (str (t "help.bindings")
                  (join "\n" (map (fn [b] (str (pad (:keyLabel b) 20 " ") (:command b)))
                                  (filter (fn [b] (= (:scope b) "screen")) computed.keyList)))
                  "\n")))

;;; The describe commands
;;
;; describe-function, describe-variable and describe-key are also the language's own functions (loop.lisp asks
;; them what is defined), which a definition of the same name would replace: a program has one name for one thing.
;; So these three commands are actions, each running a function of another name.

(defpart describe-commands "C-h f, C-h v and C-h k: the commands that ask what to describe."
  :actions {:describe-function {:title ~(t "command.describe-function")
                                :category ~(t "category.help")
                                :description ~(t "command.describe-function.about")
                                :do [{:lisp ask-function}]}
            :describe-variable {:title ~(t "command.describe-variable")
                                :category ~(t "category.help")
                                :description ~(t "command.describe-variable.about")
                                :do [{:lisp ask-variable}]}
            :describe-key {:title ~(t "command.describe-key")
                           :category ~(t "category.help")
                           :description ~(t "command.describe-key.about")
                           :do [{:lisp ask-key}]}})

;;; Modes and messages

(defcommand describe-mode "Describes the selected buffer's major mode and the minor modes that are on."
  :title ~(t "command.describe-mode") :category ~(t "category.help")
  :description ~(t "command.describe-mode.about")
  []
  (let [mode (get state.modes computed.buf.mode)]
    (help-show (t "help.mode" {:mode (:title mode) :about (or (:about mode) (t "help.noModeKeys"))}))))

(defcommand show-messages "Shows the messages buffer: everything the echo area said."
  :title ~(t "command.show-messages") :category ~(t "category.help")
  :description ~(t "command.show-messages.about")
  []
  (show "b2"))

(defcommand view-echo-area-messages "Shows the messages the echo area has shown so far."
  :title ~(t "command.view-echo-area-messages") :category ~(t "category.help")
  :description ~(t "command.view-echo-area-messages.about")
  []
  (show-messages))
