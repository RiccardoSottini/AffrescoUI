;;; board.lisp --- Flow's board, included by index.screen.lisp: its settings and state, the work-in-progress counts,
;;; the timeline's watch, presence from the host's channel, and the keys for undo and moves
;;;
;;; What they run is in flow.lisp.

(ns flow.board)

;;; State

(defstate query "")

(defstate labels [urgent frontend backend docs])

(defstate swimlanes false)

(defstate undoStack [])

(defstate today "2026-03-16")

(defstate sprintStart "2026-03-09")

(defstate burndown [34 33 30 29 27 24 23])

(defstate activity
  [{:who "MK" :key moved :title "New colour palette for labels" :column "In progress"}
   {:who "JD" :key commented :title "Invite teammates by email" :column ""}
   {:who "AL" :key added :title "Document the board API" :column ""}])

(defstate presence {})

(defstate draft {:editing false :notes ""})

(defstate dueOpen false)

(defstate settingsOpen false)

(defstate newItem "")

(defstate newComment "")

(defstate newAttachment "")

(defstate timelineTracks [])

(defstate settings {})

;;; What it works out

; style: keep, sum is the expression's own function: the function it takes names its item it
(defcomputed remaining (sum (filter #(not= (:c %) computed.lastColumn) computed.allCards) (fn [it] it.points)))

(defcomputed burndownValues (concat state.burndown [computed.remaining]))

(defcomputed checklistDone
  (if-some [card computed.card] (count (filter :done (or-else card.checklist []))) 0))

(defcomputed dueDays
  (if (or (nil? computed.card) (nil? computed.card.due)) nil (daysBetween state.today computed.card.due)))

(defcomputed present (filter #(some? (get state.presence %)) (keys state.presence)))

;;; Keys

(defkey "mod+z" undoChange)

(defkey "mod+l" changeLanguage)

(defkey "mod+f" focusSearch)

(defkey "mod+comma" openSettings)

(defkey "alt+up" {:when ~(not context.textInputFocus) :do moveUp})

(defkey "alt+down" {:when ~(not context.textInputFocus) :do moveDown})

(defkey "alt+left" {:when ~(not context.textInputFocus) :do moveLeft})

(defkey "alt+right" {:when ~(not context.textInputFocus) :do moveRight})

;;; Settings

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "settings.language") :description ~(t "settings.language.about"))

(defcustom board.wipWarnings
  true "Show a notice when a move puts a column over its work-in-progress limit."
  :type boolean :title ~(t "settings.wipWarnings") :description ~(t "settings.wipWarnings.about"))

(defcustom window.statusBar
  false "Show the revision on screen and the window's size in a bar at the bottom of the window: for developers."
  :type boolean :title ~(t "settings.developerStatusBar") :description ~(t "settings.developerStatusBar.about"))

(defcustom board.weekStart
  1 "0 for Sunday, 1 for Monday, up to 6 for Saturday, in the date picker."
  :type number :minimum 0 :maximum 6 :title ~(t "settings.weekStart") :description ~(t "settings.weekStart.about"))

;;; What it adds to the screen

(defpart board "What board adds."
  :watch {:computed.columns build-timeline :state.members build-timeline}
  :onLoad build-timeline
  :channels {:team {:socket studio.sock :reconnect {:after 2000}}}
  :actions {"team:committed" [~(handler team-committed)]
            "team:event" [~(handler team-event)]
            "team:closed" [~(handler team-closed)]})
