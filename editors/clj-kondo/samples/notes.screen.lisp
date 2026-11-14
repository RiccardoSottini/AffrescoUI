;;; notes.screen.lisp --- a sample for the linter's configuration: a list of notes, in a file with no ns
;;;
;;; Not an example app: check.sh lints it and expects exactly the two mistakes marked "planted" below.

(include "tally.lisp")

;;; State and look

(defstate notes [{:id 1 :text "Buy bread" :pinned false}] "The notes, newest last.")
(defstate draft "" "What is typed in the field.")
(defcomputed pinned "How many notes are pinned." (count (filter (fn [note] note.pinned) state.notes)))
(defcustom show-pinned true "Shows how many notes are pinned." :type boolean :title ~(t "settings.pinned"))
(deftokens colours "The sample's colours." {:color.note "#FFF7C2" :color.pinned "#F2C14E"})
(defstyle card "A note's card." {:background @color.note :radius 6 :padding 8})
(defmenu note "What a note's menu holds." {:clear {:run clear-notes :title ~(t "menu.clear")}})

;;; Logic

(defn trimmed-draft "The draft without spaces at its ends." []
  (trim state.draft))

(defn add-note "Adds the typed note, unless the field is empty." []
  (when (not= (trimmed-draft) "")
    (push "state.notes" {:id (inc (count state.notes)) :text (trimmed-draft) :pinned false})
    (set "state.draft" "")))

(defn pin "Pins a note, or unpins it." [i]
  (set (str "state.notes." i ".pinned") (not (:pinned (get state.notes i)))))

(defn next-id "The id the next note gets." []
  (inc (count state.notes) 1)) ; planted: inc takes one argument

(defcommand clear-notes "Removes every note." :title ~(t "command.clear") :category "Notes" []
  (set "state.notes" []))

(defkey "mod+shift+k" "Removes every note." clear-notes)

;;; The window

(defview note-row "One note: its text and a button that pins it." [note i]
  `(row#note :style card :gap 6
     (label#text :text ~note.text :grow 1)
     (button#pin :text ~(if note.pinned (t "note.unpin") (t "note.pin")) :onClick ~(handler pin i))))

(defview note-list "The notes, one row each." []
  `(column#notes :gap 4
     ~(repeat [note state.notes :index i :key note.id] (note-row note i))))

(defscreen main :window {:title ~(t "window.title") :width 480 :height 640}
  (column#page :gap 8 :padding 12
    (textarea#draft :bind state.draft :placeholder ~(t "draft.hint") :onSubmit add-note)
    ~(note-list)
    (label#count :visible ~settings.show-pinned :text ~(t "pinned" {:n computed.pinned}))))

(defn summary "A line about the notes." []
  (let [total (count state.notes)
        unused (count (filter (fn [note] (:pinned note)) state.notes))] ; planted: unused is never read
    (str total " notes")))
