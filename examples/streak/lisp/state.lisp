;;; state.lisp --- what Streak keeps while it runs, and where each value starts
;;;
;;; The habits, their entries and notes are records (models.lisp, seed/); the state is the app's own.

;;; State
(defstate today "2026-10-15" "Today, as a date." :type (string))

(defstate nextId 1 "The number of the next habit added.")

(defstate selected meditate "The habit shown in the detail, by its id.")

(defstate page list "The page of a narrow window: list or detail." :type {:enum [list detail]})

(defstate sort manual "The list's sort: manual, name, color, score or status." :type {:enum [manual name color score status]})

(defstate hideCompleted false "Whether habits done today are hidden." :type (boolean))

(defstate showArchived false "Whether archived habits are shown." :type (boolean))

(defstate theme light "light, dark or system." :type {:enum [light dark system]})

(defstate pureBlack false "Whether the dark theme is pure black." :type (boolean))

(defstate firstWeekday 1 "The week's first day: 0 for Sunday." :type (number))

(defstate reverseDays false "Whether the rows' days run from today." :type (boolean))

(defstate skipDays true "Whether a day can be skipped." :type (boolean))

(defstate reminders true "Whether reminders are on." :type (boolean))

(defstate locale en "The language: en, es or fr." :type {:enum [en es fr]})

(defstate scorePeriod week "The score card's period." :type {:enum [day week month quarter year]})

(defstate historyPeriod week "The history card's period." :type {:enum [day week month quarter year]})

(defstate popup {:open false :h 0 :date "" :x 0 :y 0 :value "" :note ""}
  "The day's popup: its habit, day, value, note and place.")

(defstate edit
  {:open false
   :id ""
   :kind yesNo
   :name ""
   :question ""
   :color 11
   :freqMode daily
   :n 3
   :unit ""
   :target ""
   :targetType atLeast
   :reminderOn false
   :time "08:00"
   :days [true true true true true true true]
   :description ""
   :error ""}
  "The dialog that adds or edits a habit, and its fields.")

(defstate colorOpen false "Whether the dialog's palette is open.")

(defstate confirmDelete false "Whether the dialog that deletes is shown.")

(defstate picked [] "The rows picked, by id.")

(defstate unpicked [] "The rows just let go.")

(defstate settingsOpen false "Whether the settings are shown.")

(defstate menus
  {:add false
   :filter false
   :more false
   :detail false
   :score false
   :history false
   :theme false
   :firstDay false
   :language false
   :filterSub {}}
  "Which menus are open.")

(defstate toasts [] "The notices in the corner.")

(defstate quickPick {:open false :query "" :index 0 :top 0 :recent []}
  "The command palette: open, and what is typed.")

(defstate sync {:revisions 0 :copy nil :error "" :commits 0} "The sync: commits sent, and the last error.")

(defstate backupFile nil "A backup file read, before it is restored.")

(defstate restoredCount 0 "How many habits a restore brought back.")

(defstate skippedCount 0 "How many a restore skipped.")

