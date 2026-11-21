;;; macros.lisp --- the keys Calamaio reads itself: a character typed after C-u, and keyboard macros (F3, F4)
;;;
;;; While a numeric argument waits or a macro is being recorded, the selected window's editor gives every key to
;;; editor-key (window.lisp) before the bindings and the text see it. A key that types a character types it the
;;; argument's count of times; a binding runs its command, a prefix (C-x) waits for the rest; any other key goes to
;;; the editor. A macro keeps the keys typed in a window from F3 to F4, and each answer given in the minibuffer, and
;;; F4 types them again.

(ns calamaio.macros)

;;; What it keeps

(defstate keyseq "" "The keys of a binding typed so far, while Calamaio reads them itself.")

(defstate kmacro {:on false :keys [] :last []}
  "The keyboard macro: whether it is being recorded, the keys recorded so far, and the last one recorded.")

(defcomputed readsKeys "Whether the selected window's editor gives its keys to Calamaio: an argument, a prefix, a macro."
  (and (or (some? state.arg) state.kmacro.on (not= state.keyseq "")) (not state.is.on) (not state.qr.on)))

;;; Running a key

(defn screen-binding "The document's own binding of a key sequence, or nil." [s]
  (find #(and (= (:key %) s) (= (:scope %) "screen")) computed.keyList))

(defn prefix? "Whether a key sequence begins a longer binding." [s]
  (some #(str/starts-with? (:key %) (str s " ")) computed.keyList))

(defn self-insert "Types a character, the argument's count of times, in a buffer that can change." [c]
  (if (and computed.text (not computed.buf.readOnly))
    (edit "type" :in computed.ed :text (str/join (for [_ (range computed.n)] c)))
    (read-only))
  (set "state.echo" "")
  (set "state.arg" nil))

(defn run-binding "Runs a binding's command; an argument's digit (M-5) gives its digit." [b k]
  (if (= (:command b) "digit-argument")
    (let [digit (subs k (dec (count k)))]
      (digit-argument (parse-long digit)))
    (run (:command b))))

(def listing-keys "The keys of a listing (a folder's, the buffer list), and the commands they run."
  {"enter" "listing-visit" "g" "listing-revert"})

(def dired-keys "The keys only a folder's listing has, and the commands they run."
  {"d" "dired-flag-file-deletion" "u" "dired-unmark" "x" "dired-do-flagged-delete" "shift+r" "dired-do-rename"
   "shift+c" "dired-do-copy" "shift+equal" "dired-create-directory"})

(defn window-command
  "The command a key runs in the selected window's own keys (a listing's, q in a buffer that only shows), or nil."
  [k]
  (let [mode computed.buf.mode]
    (cond (= mode "dired") (get (merge listing-keys dired-keys) k)
          (= mode "buffer-menu") (get listing-keys k)
          (and (= k "q") computed.buf.readOnly (not= computed.buf.kind "splash") (not= mode "messages")) "quit-window"
          :else nil)))

(def macro-answer-mark "What starts a keyboard macro's step that answers the minibuffer, the answer after it."
  "minibuffer:")

(defn electric? "Whether electric-pair-mode types this character itself." [c]
  (and (get settings "electric-pair-mode") (contains? #{"(" ")" "[" "]" "{" "}" "\""} c)))

(defn dispatch-key
  "A key Calamaio reads itself: a digit of the argument, a binding, the start of one, a character, or the editor's."
  [k]
  (let [s (str/trim (str state.keyseq " " k))
        b (screen-binding s)
        c (typed-char k)
        wc (when (= state.keyseq "") (window-command k))]
    (cond (= k "ctrl+g") (do (set "state.keyseq" "") (keyboard-quit))
          (str/starts-with? k macro-answer-mark) (when state.mb.open
                                                   (set "state.mb.text" (subs k (count macro-answer-mark)))
                                                   (minibuffer-exit))
          (and (= state.keyseq "") (some? state.arg) (some? (parse-long k))) (digit-argument (parse-long k))
          (some? b) (do (set "state.keyseq" "") (set "state.echo" "") (run-binding b k))
          (prefix? s) (do (set "state.keyseq" s)
                          (set "state.echo" (str (keyLabel s "letters") "-")))
          (not= state.keyseq "") (do (set "state.keyseq" "")
                                     (message (t "echo.noBinding" {:keys (keyLabel s "letters")})))
          (some? wc) (do (set "state.arg" nil) (run wc))
          (and (some? c) (electric? c)) (electric-pair c)
          (and (= k "space") (get settings "auto-fill-mode")) (auto-fill-space)
          (some? c) (self-insert c)
          (and computed.text (not computed.buf.readOnly)) (do (press k :in computed.ed :times computed.n)
                                                              (set "state.arg" nil))
          :else (do (press k :in computed.ed) (set "state.arg" nil)))))

(defn editor-key "A key the selected window's editor gives Calamaio: recorded in a macro, then run." [event]
  (when (and state.kmacro.on (not (#{"f3" "f4"} event.key)))
    (set "state.kmacro.keys" (conj state.kmacro.keys event.key)))
  (dispatch-key event.key))

;;; Keyboard macros

(defcommand kmacro-start-macro "Starts recording the keys typed in a window as a keyboard macro."
  :title ~(t "command.kmacro-start-macro") :category ~(t "category.editing")
  :description ~(t "command.kmacro-start-macro.about")
  []
  (if state.kmacro.on
    (message (t "echo.macroAlready"))
    (do (set "state.kmacro" (merge state.kmacro {:on true :keys []}))
        (message (t "echo.macroStart")))))

(defn kmacro-play "Types the last keyboard macro's keys again, a count of times." [times]
  (let [ks state.kmacro.last]
    (set "state.arg" nil)
    (dotimes [_ times]
      (doseq [k ks] (dispatch-key k)))))

(defcommand kmacro-end-or-call-macro
  "Ends the keyboard macro being recorded; else types the last one again (with an argument, that many times)."
  :title ~(t "command.kmacro-end-or-call-macro") :category ~(t "category.editing")
  :description ~(t "command.kmacro-end-or-call-macro.about")
  []
  (cond state.kmacro.on (do (set "state.kmacro" {:on false :keys [] :last state.kmacro.keys})
                            (message (t "echo.macroEnd" {:n (count state.kmacro.last)})))
        (empty? state.kmacro.last) (message (t "echo.macroNone"))
        :else (kmacro-play computed.n)))
