;;; search.lisp --- searching as you type (C-s, C-r) and replacing with a question at each match (M-%)
;;;
;;; While a search or a replace is on, the selected window passes every key to search-key (window.lisp): the search
;;; takes typed characters, C-s and C-r go to the next and the previous match, RET stops at the match and C-g goes
;;; back; the replace takes y, n, ! and q. The editor finds the matches (computed.findRequest): each change of
;;; state.is.req or state.qr.req asks it to move.

;;; Keys as characters

(def key-chars "The characters typed by the keys whose names aren't their characters."
  {"space" " " "minus" "-" "period" "." "comma" "," "slash" "/" "semicolon" ";" "apostrophe" "'" "equal" "="
   "leftbracket" "[" "rightbracket" "]" "backslash" "\\"
   "shift+9" "(" "shift+0" ")" "shift+1" "!" "shift+2" "@" "shift+3" "#" "shift+4" "$" "shift+5" "%" "shift+6" "^"
   "shift+7" "&" "shift+8" "*" "shift+minus" "_" "shift+equal" "+" "shift+semicolon" ":" "shift+apostrophe" "\""
   "shift+comma" "<" "shift+period" ">" "shift+slash" "?"})

(defn starts-with? "Whether a text starts with another." [s start]
  (and (>= (count s) (count start)) (= (subs s 0 (count start)) start)))

(defn typed-char "The character a key types, or nil: a letter, a shifted letter, or a sign." [k]
  (cond (= (count k) 1) k
        (and (= (count k) 7) (starts-with? k "shift+") (not= (upper-case (subs k 6)) (subs k 6)))
        (upper-case (subs k 6))
        :else (get key-chars k)))

(defn search-key "A key the selected window passes on while a search or a replace is on." [event]
  (if state.is.on
    (isearch-key event.key)
    (query-replace-key event.key)))

;;; Searching as you type

(defn isearch-start "Starts a search from point, forward or back." [back]
  (when computed.text
    (set "state.is" (merge state.is {:on true :text "" :back back :step "first" :req (inc state.is.req) :start (point)
                                     :wrapped false}))
    (set "state.echo" "")))

(defcommand isearch-forward
  "Searches forward as you type: C-s again goes to the next match, RET stops at it, C-g goes back."
  :title ~(t "command.isearch-forward") :category ~(t "category.search")
  :description ~(t "command.isearch-forward.about")
  []
  (isearch-start false))

(defcommand isearch-backward "Searches backward as you type: C-r again goes to the previous match."
  :title ~(t "command.isearch-backward") :category ~(t "category.search")
  :description ~(t "command.isearch-backward.about")
  []
  (isearch-start true))

(defn isearch-step "C-s or C-r while searching: the next or the previous match, or the last search again." [back]
  (set "state.is.back" back)
  (when (= state.is.text "") (set "state.is.text" (or state.is.last "")))
  (set "state.is.step" (if back "previous" "next"))
  (increment "state.is.req"))

(defn isearch-end "Stops the search where it is, keeps its text for the next, and leaves the mark where it started." []
  (when (not= state.is.text "") (set "state.is.last" state.is.text))
  (set "state.is.on" false)
  (set "state.mark" {:on false :line state.is.start.line :column state.is.start.column})
  (after 0 isearch-unselect))

(defn isearch-unselect "Lets go of the match the search selected, leaving point at its end." []
  (when (and computed.text (> (or (:selected (caret)) 0) 0))
    (press (if state.is.back "left" "right") :in computed.ed)))

(defn isearch-key "A key while searching: a character searches further, Backspace less; RET stops, C-g goes back." [k]
  (let [c (typed-char k)
        bound (find (fn [b] (= (:key b) k)) computed.keyList)]
    (cond (or (= k "ctrl+s") (= k "ctrl+r"))
          (isearch-step (= k "ctrl+r"))
          (= k "backspace")
          (do (set "state.is.text" (subs state.is.text 0 (max 0 (dec (count state.is.text)))))
              (set "state.is.step" "first")
              (increment "state.is.req"))
          (= k "ctrl+g")
          (do (isearch-end)
              (goto-point computed.ed state.is.start)
              (set "state.echo" (t "echo.quit")))
          (or (= k "enter") (= k "escape"))
          (do (isearch-end) (message (t "echo.markSaved")))
          (some? c)
          (do (set "state.is.text" (str state.is.text c))
              (set "state.is.step" (if state.is.back "previous" "first"))
              (increment "state.is.req"))
          :else
          (do (isearch-end)
              (when (some? bound) (run (:command bound)))))))

;;; Replacing

(defcommand query-replace
  "Replaces a text with another, asking at each match: y or SPC replaces, n skips, ! replaces the rest, q stops."
  :title ~(t "command.query-replace") :category ~(t "category.search")
  :description ~(t "command.query-replace.about")
  []
  (let [from state.qr.from]
    (minibuffer-read "replace-from"
                     (if (and (some? from) (not= from ""))
                       (t "prompt.queryReplaceDefault" {:from from :to state.qr.to})
                       (t "prompt.queryReplace"))
                     (or from "")
                     "")))

(defn ask-replace-to "Asks what to replace a text with." [value]
  (minibuffer-read "replace-to" (t "prompt.queryReplaceWith" {:from value}) "" ""))

(defn mb-replace-from
  "The text to replace: the last one again if nothing was typed, else ask for its replacement." [value]
  (cond (= value "") nil
        (and (= value state.qr.from) (= state.mb.text "")) (query-replace-start state.qr.to)
        :else (do (set "state.qr.from" value)
                  (after 0 ask-replace-to value))))

(defn mb-replace-to "The replacement: the replace starts." [value]
  (query-replace-start value))

(defn query-replace-start "Starts asking at each match, from point; with no match, it ends at once." [to]
  (set "state.qr" (merge state.qr {:on true :to to :step "first" :req (inc state.qr.req) :done 0}))
  (after 60 end-if-no-match))

(defn end-if-no-match "Ends the replace if the editor found no match." []
  (when (and state.qr.on (= (or (:matches (caret)) 0) 0))
    (query-replace-end)))

(defn query-replace-mark "Remembers where the match being answered is." []
  (set "state.qr.at" (point)))

(defn query-replace-wrapped "Ends the replace once the editor has gone round past the first match answered." []
  (after 60 end-if-wrapped))

(defn end-if-wrapped "Ends the replace if no match is left, or the next one is back before the last answered." []
  (let [at (caret) last state.qr.at]
    (when (and state.qr.on
               (or (= (or (:matches at) 0) 0)
                   (< (:line at) (:line last))
                   (and (= (:line at) (:line last)) (<= (:column at) (:column last)))))
      (goto-point computed.ed last)
      (query-replace-end))))

(defn replace-this "y, SPC or .: replaces this match; . stops there." [k]
  (query-replace-mark)
  (set "state.qr.step" "replace")
  (set "state.qr.replace" state.qr.to)
  (increment "state.qr.req")
  (increment "state.qr.done")
  (if (= k "period")
    (query-replace-end)
    (query-replace-wrapped)))

(defn skip-this "n: leaves this match as it is, and goes to the next." []
  (query-replace-mark)
  (set "state.qr.step" "next")
  (increment "state.qr.req")
  (query-replace-wrapped))

(defn replace-the-rest "!: replaces this match and every one after it." []
  (set "state.qr.done" (+ state.qr.done (or (:matches (caret)) 0)))
  (set "state.qr.step" "replaceAll")
  (set "state.qr.replace" state.qr.to)
  (increment "state.qr.req")
  (after 60 query-replace-end))

(defn query-replace-key
  "A key while replacing: y, SPC or . replaces, n skips, ! replaces the rest, C-g quits, anything else stops." [k]
  (cond (contains ["y" "space" "period"] k) (replace-this k)
        (contains ["n" "delete" "backspace"] k) (skip-this)
        (= k "shift+1") (replace-the-rest)
        (= k "ctrl+g") (do (set "state.qr.on" false) (set "state.echo" (t "echo.quit")))
        :else (query-replace-end)))

(defn query-replace-unselect "Lets go of the match the replace left selected." []
  (when (and computed.text (> (or (:selected (caret)) 0) 0))
    (press "right" :in computed.ed)))

(defn query-replace-end "Ends the replace, saying how many it replaced." []
  (set "state.qr.on" false)
  (message (t "echo.replaced" {:n state.qr.done}))
  (after 0 query-replace-unselect))
