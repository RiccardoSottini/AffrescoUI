;;; killring.lisp --- the kill ring: what C-k, M-d, C-w and M-w keep, and what C-y and M-y give back
;;;
;;; What is killed goes in state.kills, the newest first, and on the clipboard. Successive kills from the same place
;;; join, as they do in the editor Calamaio follows. A function reads the running app through paths (`computed.text`,
;;; `state.kills`) and changes it only through steps (`set`, `edit`, `copy`). Evaluate any of these in place (C-M-x) and
;;; the next key uses it.

;;; What every kill starts from

(defn editable? "Whether this buffer takes a change now: says so if it doesn't." []
  (cond (not computed.text) false
        (:readOnly computed.buf) (do (read-only) false)
        :else true))

;;; Killing

(defn kill-push "Keeps a text in the ring, or joins it to the newest." [text join]
  (set "state.kills"
       (if (and join (> (count state.kills) 0))
         (vec (cons (str (first state.kills) text) (rest state.kills)))
         (vec (take settings.kill-ring-max (cons text state.kills)))))
  (copy (first state.kills)))

(defn after-a-kill? "Whether point hasn't moved since the last kill." []
  (let [here (caret) last state.killAt]
    (and (some? last)
         (= (:buf last) computed.cur)
         (= (:line last) (:line here))
         (= (:column last) (:column here)))))

(defn kill "Cuts a span of text, some times over, into the ring." [span times]
  (set "state.arg" nil)
  (when (editable?)
    (deactivate-mark)
    (set "state.killTmp" "")
    (map (fn [i] (edit "cut" :span span :in computed.ed :into "state.killTmp" :append true)) (range times))
    (if (= state.killTmp "")
      (message (t "echo.endOfBuffer"))
      (do (kill-push state.killTmp (and (not= span "line") (after-a-kill?)))
          (let [here (caret)]
            (set "state.killAt" {:buf computed.cur :line (:line here) :column (:column here)}))))))

(defn kill-selection "Cuts or copies the region into the ring." [keep]
  (cond
    (not computed.text) nil
    (= (or (:selected (caret)) 0) 0) (message (t "echo.regionEmpty"))
    (and (not keep) (:readOnly computed.buf)) (read-only)
    :else
    (do (set "state.killTmp" "")
        (edit "cut" :span "selection" :in computed.ed :into "state.killTmp" :keep keep)
        (kill-push state.killTmp false)
        (set "state.killAt" nil)
        (if keep
          (do (deactivate-mark) (message (t "echo.copied")))
          (set "state.mark.on" false)))))

(defcommand kill-line
  "Kills the rest of the line, or its line break when point is at the end; with an argument, that many times."
  :title ~(t "command.kill-line") :category ~(t "category.killing")
  :description ~(t "command.kill-line.about")
  []
  (kill "lineEnd" computed.n))

(defcommand kill-word "Kills to the end of the word after point."
  :title ~(t "command.kill-word") :category ~(t "category.killing")
  :description ~(t "command.kill-word.about")
  []
  (kill "word" computed.n))

(defcommand backward-kill-word "Kills back to the start of the word before point."
  :title ~(t "command.backward-kill-word") :category ~(t "category.killing")
  :description ~(t "command.backward-kill-word.about")
  []
  (when (editable?)
    (deactivate-mark)
    (press "shift+alt+left" :in computed.ed)
    (kill-selection false)))

(defcommand kill-whole-line "Kills the line point is on, with its line break."
  :title ~(t "command.kill-whole-line") :category ~(t "category.killing")
  :description ~(t "command.kill-whole-line.about")
  []
  (kill "line" computed.n))

(defcommand kill-region "Kills the region, the text between point and the mark, into the kill ring."
  :title ~(t "command.kill-region") :category ~(t "category.killing")
  :description ~(t "command.kill-region.about")
  []
  (kill-selection false))

(defcommand kill-ring-save "Copies the region into the kill ring, leaving the text where it is."
  :title ~(t "command.kill-ring-save") :category ~(t "category.killing")
  :description ~(t "command.kill-ring-save.about")
  []
  (kill-selection true))

;;; Yanking

(defn end-of "Where a text ends once it is inserted at a place." [text at]
  (let [lines (split text "\n")]
    {:line (+ (:line at) (count lines) -1)
     :column (if (contains text "\n")
               (+ (length (last lines)) 1)
               (+ (:column at) (length text)))}))

(defn yank-at "Inserts a kill of the ring at a place, and remembers it for M-y." [i at]
  (let [text (nth state.kills i)]
    (set "state.mark" {:on false :line (:line at) :column (:column at)})
    (edit "type" :text text :in computed.ed)
    (set "state.yank" {:buf computed.cur :i i :start at :end (end-of text at)})))

(defcommand yank "Inserts the last killed text at point, and sets the mark at its start."
  :title ~(t "command.yank") :category ~(t "category.killing")
  :description ~(t "command.yank.about")
  []
  (when (editable?)
    (if (= (count state.kills) 0)
      (message (t "echo.killRingEmpty"))
      (let [here (caret)]
        (deactivate-mark)
        (set "state.arg" nil)
        (yank-at 0 {:line (:line here) :column (:column here)})))))

(defn yanked-here? "Whether the last command was a yank, right here." []
  (let [last state.yank here (caret)]
    (and (some? last)
         (= (:buf last) computed.cur)
         (= (:line (:end last)) (:line here))
         (= (:column (:end last)) (:column here)))))

(defcommand yank-pop "Right after a yank, replaces the yanked text with the kill before it in the ring."
  :title ~(t "command.yank-pop") :category ~(t "category.killing")
  :description ~(t "command.yank-pop.about")
  []
  (if (not (yanked-here?))
    (message (t "echo.notAYank"))
    (let [last state.yank start (:start last) end (:end last)]
      (edit "select" :in computed.ed :line (:line start) :column (:column start)
            :toLine (:line end) :toColumn (:column end))
      (after 0 yank-at (mod (inc (:i last)) (count state.kills)) start))))
