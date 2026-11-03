;;; state.lisp --- TODO

(defstate today "2026-10-15" :type (string))

(defstate nextId 1)

(defstate selected meditate)

(defstate page list :type {:enum [list detail]})

(defstate sort manual :type {:enum [manual name color score status]})

(defstate hideCompleted false :type (boolean))

(defstate showArchived false :type (boolean))

(defstate theme light :type {:enum [light dark system]})

(defstate pureBlack false :type (boolean))

(defstate firstWeekday 1 :type (number))

(defstate reverseDays false :type (boolean))

(defstate skipDays true :type (boolean))

(defstate reminders true :type (boolean))

(defstate locale en :type {:enum [en es fr]})

(defstate scorePeriod week :type {:enum [day week month quarter year]})

(defstate historyPeriod week :type {:enum [day week month quarter year]})

(defstate popup {:open false :h 0 :date "" :x 0 :y 0 :value "" :note ""})

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
   :error ""})

(defstate colorOpen false)

(defstate confirmDelete false)

(defstate picked [])

(defstate unpicked [])

(defstate settingsOpen false)

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
   :filterSub {}})

(defstate toasts [])

(defstate quickPick {:open false :query "" :index 0 :top 0 :recent []})

(defstate sync {:revisions 0 :copy nil :error "" :commits 0})

(defstate backupFile nil)

(defstate restoredCount 0)

(defstate skippedCount 0)

