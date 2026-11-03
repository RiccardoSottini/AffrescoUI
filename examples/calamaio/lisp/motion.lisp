;;; motion.lisp --- moving point, the mark, a numeric argument, and C-g
;;;
;;; A command that moves presses a key of the selected window's editor (computed.ed), the numeric argument's count of
;;; times (computed.n): with the mark active it extends the region. C-u and M-<digit> give that argument; C-g cancels
;;; what was begun.

;;; Pressing the editor's keys

(defn caret "The selected window's caret: its line, column, how much is selected and how many matches there are." []
  (:caret (get state.win state.sel)))

(defn move
  "Presses a key of the selected window's editor, the argument's count of times; with the mark, the region grows." [k]
  (when (some? state.arg) (set "state.echo" ""))
  (when computed.text
    (press (str (if state.mark.on "shift+" "") k) :in computed.ed :times computed.n))
  (set "state.arg" nil))

;;; Motion

(defcommand forward-char "Moves point a character forward (by the argument's count)."
  :title ~(t "command.forward-char") :category ~(t "category.motion")
  :description ~(t "command.forward-char.about")
  []
  (move "right"))

(defcommand backward-char "Moves point a character back."
  :title ~(t "command.backward-char") :category ~(t "category.motion")
  :description ~(t "command.backward-char.about")
  []
  (move "left"))

(defcommand next-line "Moves point down a line, keeping its column."
  :title ~(t "command.next-line") :category ~(t "category.motion")
  :description ~(t "command.next-line.about")
  []
  (move "down"))

(defcommand previous-line "Moves point up a line, keeping its column."
  :title ~(t "command.previous-line") :category ~(t "category.motion")
  :description ~(t "command.previous-line.about")
  []
  (move "up"))

(defcommand forward-word "Moves point past the end of the next word."
  :title ~(t "command.forward-word") :category ~(t "category.motion")
  :description ~(t "command.forward-word.about")
  []
  (move "alt+right"))

(defcommand backward-word "Moves point to the start of the word before it."
  :title ~(t "command.backward-word") :category ~(t "category.motion")
  :description ~(t "command.backward-word.about")
  []
  (move "alt+left"))

(defcommand move-beginning-of-line "Moves point to the start of its line."
  :title ~(t "command.move-beginning-of-line") :category ~(t "category.motion")
  :description ~(t "command.move-beginning-of-line.about")
  []
  (move "cmd+left"))

(defcommand move-end-of-line "Moves point to the end of its line."
  :title ~(t "command.move-end-of-line") :category ~(t "category.motion")
  :description ~(t "command.move-end-of-line.about")
  []
  (move "cmd+right"))

(defcommand beginning-of-buffer "Moves point to the start of the buffer, leaving the mark where it was."
  :title ~(t "command.beginning-of-buffer") :category ~(t "category.motion")
  :description ~(t "command.beginning-of-buffer.about")
  []
  (when (not state.mark.on) (message (t "echo.markSet")))
  (set "state.arg" nil)
  (move "cmd+up"))

(defcommand end-of-buffer "Moves point to the end of the buffer, leaving the mark where it was."
  :title ~(t "command.end-of-buffer") :category ~(t "category.motion")
  :description ~(t "command.end-of-buffer.about")
  []
  (when (not state.mark.on) (message (t "echo.markSet")))
  (set "state.arg" nil)
  (move "cmd+down"))

(defcommand scroll-up-command "Shows the next screenful of text."
  :title ~(t "command.scroll-up-command") :category ~(t "category.motion")
  :description ~(t "command.scroll-up-command.about")
  []
  (move "pagedown"))

(defcommand scroll-down-command "Shows the previous screenful of text."
  :title ~(t "command.scroll-down-command") :category ~(t "category.motion")
  :description ~(t "command.scroll-down-command.about")
  []
  (move "pageup"))

;;; The mark

(defcommand set-mark-command "Sets the mark where point is and activates the region."
  :title ~(t "command.set-mark-command") :category ~(t "category.editing")
  :description ~(t "command.set-mark-command.about")
  []
  (when (and computed.text (> (or (:selected (caret)) 0) 0))
    (press "right" :in computed.ed))
  (set "state.mark" (assoc (point) :on true))
  (message (t "echo.markSet")))

(defn deactivate-mark "Lets go of the region, point staying at the end of it the mark isn't at." []
  (let [at (caret) mark state.mark]
    (when (and (:on mark) computed.text (> (or (:selected at) 0) 0))
      (press (if (or (> (:line at) (:line mark)) (and (= (:line at) (:line mark)) (>= (:column at) (:column mark))))
               "right"
               "left")
             :in computed.ed)))
  (set "state.mark.on" false))

(defcommand keyboard-quit "Cancels the command being typed, the minibuffer and the region."
  :title ~(t "command.keyboard-quit") :category ~(t "category.editing")
  :description ~(t "command.keyboard-quit.about")
  []
  (deactivate-mark)
  (set "state.arg" nil)
  (when state.mb.open (minibuffer-close))
  (set "state.echo" (t "echo.quit")))

;;; A numeric argument

(defcommand universal-argument
  "Starts a numeric argument for the next command: 4, and each further C-u multiplies it by 4."
  :title ~(t "command.universal-argument") :category ~(t "category.editing")
  :description ~(t "command.universal-argument.about")
  []
  (let [fresh (or (nil? state.arg) state.argDigits)]
    (set "state.argU" (if fresh 1 (inc (or state.argU 0))))
    (set "state.arg" (if fresh 4 (* state.arg 4)))
    (set "state.argDigits" false)
    (set "state.echo" (str (join " " (map (fn [_] "C-u") (range state.argU))) "-"))))

(defcommand digit-argument "Adds a digit to the numeric argument of the next command."
  :title ~(t "command.digit-argument") :category ~(t "category.editing")
  :description ~(t "command.digit-argument.about")
  [d]
  (set "state.arg" (if (and state.argDigits (some? state.arg)) (+ (* state.arg 10) d) d))
  (set "state.argDigits" true)
  (set "state.echo" (str "C-u " state.arg "-")))
