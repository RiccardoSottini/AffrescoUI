;;; editing.lisp --- changing text: deleting, undoing, the region's case and lines, comments, transposing, the region
;;;
;;; A change presses the selected window's editor's keys or asks it for an edit, and only in a buffer that isn't
;;; read-only: there it says so.

;;; Changing what is at point

(defn read-only "Says that the selected window's buffer can't be changed." []
  (message (t "echo.readOnly" {:name computed.buf.name})))

(defn edit-key
  "Presses an editing key of the selected window's editor, the argument's count of times, if its buffer can change." [k]
  (when (some? state.arg) (set "state.echo" ""))
  (if (and computed.text (not computed.buf.readOnly))
    (press k :in computed.ed :times computed.n)
    (read-only))
  (set "state.arg" nil))

(defcommand delete-char "Deletes the character after point."
  :title ~(t "command.delete-char") :category ~(t "category.editing")
  :description ~(t "command.delete-char.about")
  []
  (edit-key "delete"))

(defcommand undo "Undoes the last change; repeat it to undo more."
  :title ~(t "command.undo") :category ~(t "category.editing")
  :description ~(t "command.undo.about")
  []
  (edit-key "ctrl+z")
  (message (t "echo.undo")))

(defcommand undo-redo "Redoes what the last undo undid."
  :title ~(t "command.undo-redo") :category ~(t "category.editing")
  :description ~(t "command.undo-redo.about")
  []
  (edit-key "ctrl+shift+z")
  (message (t "echo.redo")))

(defcommand open-line "Puts a line break after point, leaving point where it is."
  :title ~(t "command.open-line") :category ~(t "category.editing")
  :description ~(t "command.open-line.about")
  []
  (cond (not computed.text) nil
        computed.buf.readOnly (read-only)
        :else (do (edit "type" :text "\n" :in computed.ed)
                  (press "left" :in computed.ed))))

;;; The region, or the line

(defn region-command
  "Asks the editor for an edit of the region (or of the line, unless need), with the mode's comment marker." [c need]
  (cond (not computed.text) nil
        computed.buf.readOnly (read-only)
        (and need (= (or (:selected (caret)) 0) 0)) (message (t "echo.regionEmpty"))
        :else (do (edit c :in computed.ed :prefix (or (get-in state.modes [computed.buf.mode :comment]) "#"))
                  (set "state.mark.on" false))))

(defcommand upcase-region "Turns the region's letters into capitals."
  :title ~(t "command.upcase-region") :category ~(t "category.editing")
  :description ~(t "command.upcase-region.about")
  []
  (region-command "upperCase" true))

(defcommand downcase-region "Turns the region's letters into small letters."
  :title ~(t "command.downcase-region") :category ~(t "category.editing")
  :description ~(t "command.downcase-region.about")
  []
  (region-command "lowerCase" true))

(defcommand capitalize-region "Starts each word of the region with a capital."
  :title ~(t "command.capitalize-region") :category ~(t "category.editing")
  :description ~(t "command.capitalize-region.about")
  []
  (region-command "titleCase" true))

(defcommand sort-lines "Sorts the region's lines alphabetically."
  :title ~(t "command.sort-lines") :category ~(t "category.editing")
  :description ~(t "command.sort-lines.about")
  []
  (region-command "sortLines" true))

(defcommand comment-line "Comments or uncomments the line, or the region's lines, with the mode's comment marker."
  :title ~(t "command.comment-line") :category ~(t "category.editing")
  :description ~(t "command.comment-line.about")
  []
  (region-command "toggleComment" false))

(defcommand transpose-chars "Swaps the characters before and after point."
  :title ~(t "command.transpose-chars") :category ~(t "category.editing")
  :description ~(t "command.transpose-chars.about")
  []
  (region-command "transpose" false))

;;; Point and the mark

(defn end-of-text "Where a text ends: its last line, and the column after its last character." [text]
  (let [lines (split text "\n")]
    {:line (count lines) :column (inc (count (last lines)))}))

(defcommand mark-whole-buffer
  "Puts point at the start of the buffer and the mark at its end, so the region is all of it."
  :title ~(t "command.mark-whole-buffer") :category ~(t "category.editing")
  :description ~(t "command.mark-whole-buffer.about")
  []
  (when computed.text
    (press "cmd+down" :in computed.ed)
    (press "shift+cmd+up" :in computed.ed)
    (set "state.mark" (assoc (end-of-text (or computed.buf.text "")) :on true))
    (message (t "echo.markSet"))))

(defcommand exchange-point-and-mark
  "Puts point where the mark is and the mark where point was, and activates the region."
  :title ~(t "command.exchange-point-and-mark") :category ~(t "category.editing")
  :description ~(t "command.exchange-point-and-mark.about")
  []
  (when computed.text
    (let [m state.mark p (point)]
      (edit "select" :in computed.ed :line (:line p) :column (:column p) :toLine (:line m) :toColumn (:column m))
      (set "state.mark" (assoc p :on true)))))
