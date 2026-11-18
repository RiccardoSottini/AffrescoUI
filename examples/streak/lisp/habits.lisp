;;; habits.lisp --- what Streak does: its functions and commands
;;;
;;; The screen (index.screen.lisp) says what is shown; this file says what happens. A function reads the running app
;;; through paths (`state.today`, `data.ordered`, `computed.h`) and changes it only through steps (`set`, `push`,
;;; `update-record`...). A handler runs one by name: {:run "cellClick" :with {:h 1 :d "2026-10-15"}} gives its parameters.

(ns streak.habits)

;;; The habits as records

(defn habit "The habit on a row of the ordered list." [h] (get data.ordered h))

(defn measurable? "Whether a habit counts a number rather than yes or no." [habit]
  (= (:kind habit) :measurable))

(defn separator "What stands between a habit's entries." [habit]
  (if (measurable? habit) " " ""))

(defn blank "The entry of a day with nothing in it." [habit]
  (if (measurable? habit) "0" "-"))

(defn cells "A habit's entries, one per day from its first." [habit]
  (if (= (:entries habit) "") [] (str/split (:entries habit) (separator habit))))

(defn dayIndex "Which entry of a habit a day is." [habit date]
  (daysBetween (:from habit) date))

(defn entryOn "A yes-or-no habit's entry on a day: y, s or -." [habit date]
  (or (get (cells habit) (dayIndex habit date)) "-"))

(defn toast "Shows a message." [message]
  (push "state.toasts" {:message message}))

;;; Marking days

(defn select "Shows a habit in the detail." [h]
  (set "state.selected" (:id (habit h)))
  (when-not (true? computed.wide) (set "state.page" :detail)))

(defn extendBack "Gives a habit days from an earlier first one." [habit date]
  (let [days (map (constantly (blank habit)) (range (daysBetween date (:from habit))))]
    (update-record "habits" :id (:id habit)
      :fields {:from date
               :entries (->> [(:entries habit)] (concat days) (str/join (separator habit)) str/trim)})))

(defn putEntry "Writes one day's entry: a character for yes-or-no habits, a number's text for measurable ones." [h date value]
  (when (pos? (daysBetween date (:from (habit h)))) (extendBack (habit h) date))
  (let [current (habit h)
        at (dayIndex current date)
        list (cells current)
        padded (->> (- (inc at) (count list)) (max 0) range (map (constantly (blank current))) (concat list))]
    (update-record "habits" :id (:id current)
      :fields {:entries (str/join (separator current) (assoc (vec padded) at value))})))

(defn openPopup "Opens the day's popup, with what is written on it." [h d x y]
  (let [current (habit h)
        amount (get (get computed.vals h) (dayIndex current d) 0)]
    (set "state.popup" {:open true :h h :date d :x x :y y
                        :note (get (:notes current) d "")
                        :value (if (and (measurable? current) (pos? amount)) (text amount) "")})))

(defn cellClick "A yes-or-no box goes yes, skipped (if allowed), no; a measurable one asks for the number." [h d x y]
  (if (measurable? (habit h))
    (openPopup h d x y)
    (putEntry h d (case (entryOn (habit h) d)
                    "y" (if (true? state.skipDays) "s" "-")
                    "s" "-"
                    "y"))))

(defn popupNoteOnly "Keeps the popup's note and closes it." []
  (let [current (habit state.popup.h)]
    (update-record "habits" :id (:id current)
      :fields {:notes (assoc (:notes current) state.popup.date (str/trim state.popup.note))})
    (set "state.popup.open" false)))

(defn popupSet "From the popup's buttons: y, s or -." [value]
  (putEntry state.popup.h state.popup.date value)
  (popupNoteOnly))

(defn popupSave "Keeps the number the popup holds." []
  (putEntry state.popup.h state.popup.date (-> state.popup.value number (or 0) (max 0) text))
  (popupNoteOnly))

(defn checkToday-at "Marks today on a row." [h]
  (cellClick h state.today (/ window.width 2) (/ window.height 3)))

;;; Order, filters and settings

(defn moveHabit "A row dragged onto another, in manual order: a position between its new neighbours." [from to]
  (when (and (= state.sort :manual) (not= from to))
    (let [ordered data.ordered
          others (concat (take from ordered) (drop (inc from) ordered))
          position #(get-in others [% :position])
          last-row (dec (count ordered))]
      (update-record "habits" :id (get-in ordered [from :id])
        :fields {:position (cond (<= to 0) (dec (or (position 0) 0))
                                 (>= to last-row) (inc (or (position (dec last-row)) 0))
                                 :else (/ (+ (position (dec to)) (position to)) 2))}))))

(defn setSort "" [by] (set "state.sort" by))
(defn setPeriod "" [key value] (set (str "state." key) value))
(defn setTheme "" [value] (set "state.theme" value))
(defn setFirstDay "" [value] (set "state.firstWeekday" value))
(defn setLocale "" [value] (set "state.locale" value))

(defn leaveEscape "What Escape leaves: the picked rows, or the detail page." []
  (if (true? computed.picking) (set "state.picked" []) (set "state.page" :list)))

;;; The edit dialog

(def everyDay "A reminder's days at first." [true true true true true true true])

(defn newHabit "Opens the dialog on a new habit." [kind]
  (set "state.edit" {:open true :id "" :kind kind :name "" :question ""
                     :color (-> (count data.ordered) (* 3) (+ 11) (mod 20))
                     :freqMode :daily :n 3 :unit ""
                     :target (if (= kind :measurable) "10" "")
                     :targetType "atLeast" :reminderOn false :time "08:00" :days everyDay
                     :description "" :error ""}))

(defcommand addYesNo "A new yes-or-no habit."
  :title ~(t "action.addYesNo.title") :description ~(t "action.addYesNo.about")
  :category ~(t "action.addYesNo.category") :icon "check-circle" []
  (newHabit "yesNo"))

(defcommand addMeasurable "A new measurable habit."
  :title ~(t "action.addMeasurable.title") :description ~(t "action.addMeasurable.about")
  :category ~(t "action.addMeasurable.category") :icon "ruler" []
  (newHabit "measurable"))

(defn freqModeOf "Which frequency choice a habit's frequency is." [habit]
  (let [{:keys [days times]} (:frequency habit)]
    (cond (= 1 days times) :daily
          (measurable? habit) (if (= days 7) :week :month)
          (and (= times 1) (not (#{7 30} days))) :everyN
          (= days 7) :week
          :else :month)))

(defcommand editHabit "Opens the dialog on the selected habit."
  :title ~(t "action.editHabit.title") :description ~(t "action.editHabit.about")
  :category ~(t "action.editHabit.category") :icon "edit" :enablement ~(>= computed.sel 0) []
  (let [{:keys [id kind name question color unit target targetType reminder description] :as h} computed.h
        {:keys [days times]} (:frequency h)]
    (set "state.edit" {:open true :id id :kind kind :name name :question question
                       :color color :freqMode (freqModeOf h)
                       :n (if (= times 1) days times)
                       :unit unit :target (text target) :targetType targetType
                       :reminderOn (some? reminder) :time (or (:time reminder) "08:00")
                       :days (or (:days reminder) everyDay)
                       :description description :error ""})))

(defn insertHabit "A new habit of a kind, at the end of the list, from today, and selected." [kind fields]
  (let [id (str "habit" state.nextId)]
    (insert "habits"
      :value (merge {:id id :kind kind :archived false :from state.today :entries "" :notes {}
                     :position (inc (reduce max -1 (map :position data.ordered)))}
                    fields))
    (set "state.selected" id)
    (increment "state.nextId")))

(defn storeHabit "Inserts the dialog's habit, or updates the selected one." [fields]
  (if (= state.edit.id "")
    (insertHabit state.edit.kind fields)
    (update-record "habits" :id computed.h.id :fields fields)))

(defn frequencyOf "What the dialog's frequency choice means." [edit]
  (let [{:keys [freqMode kind n]} edit
        period (if (= freqMode :week) 7 30)]
    (cond (= freqMode :daily) {:times 1 :days 1}
          (= kind :measurable) {:times 1 :days period}
          (= freqMode :everyN) {:times 1 :days n}
          :else {:times n :days period})))

(defn saveHabit "Keeps the dialog's habit, if it has a name." []
  (let [{:keys [name question color description unit target targetType] :as edit} state.edit]
    (if (str/blank? name)
      (do (set "state.edit.error" (t "edit.nameNeeded"))
          (focus "#nameField")
          (set "state.edit.open" true))
      ; style: keep, question, description and unit are optional fields: the host's trim gives "" for nil
      (storeHabit {:name (str/trim name) :question (trim question) :color color
                   :description (trim description) :unit (trim unit)
                   :target (or (number target) 0) :targetType targetType
                   :reminder (when (:reminderOn edit) {:time (:time edit) :days (:days edit)})
                   :frequency (frequencyOf edit)}))))

;;; By names: what the commands an agent runs (habitAdd, habitDone, habitRename) do. A habit that isn't there, or a
;;; day that can't be marked, changes nothing, and a toast says so.

(defn habitRow "The row of the habit with that name, in any case, or -1." [name]
  (indexOf (map #(str/lower-case (:name %)) data.ordered) (-> name str/trim str/lower-case)))

(defn addHabitNamed "A new habit with that name, every day: yes or no, or measurable with a target of 10." [name kind]
  (if (str/blank? name)
    (toast (t "edit.nameNeeded"))
    (insertHabit kind {:name (str/trim name) :question "" :color (-> (count data.ordered) (* 3) (+ 11) (mod 20))
                       :description "" :unit "" :target (if (= kind :measurable) 10 0) :targetType "atLeast"
                       :reminder nil :frequency {:times 1 :days 1}})))

(defn markDone "Marks a habit done, by its name, on a day up to today (today when nil): yes, or for a measurable one
  an amount (its target when nil)." [name date amount]
  (let [h (habitRow name)
        day (or date state.today)]
    (cond (neg? h) (toast (t "toast.noHabit" {:name name}))
          (or (not (isDate day)) (neg? (daysBetween day state.today)))
          (toast (t "toast.badDay" {:date day :today state.today}))
          :else (putEntry h day (if (measurable? (habit h)) (text (or amount (:target (habit h)))) "y")))))

(defn renameHabit "Gives a habit, by its name, a new name." [name to]
  (let [h (habitRow name)]
    (cond (neg? h) (toast (t "toast.noHabit" {:name name}))
          (str/blank? to) (toast (t "edit.nameNeeded"))
          :else (update-record "habits" :id (:id (habit h)) :fields {:name (str/trim to)}))))

(defcommand habitAdd "Adds a habit by its name."
  :title ~(t "action.habitAdd.title") :description ~(t "action.habitAdd.about")
  :category ~(t "action.habitAdd.category") :icon "plus"
  :arguments {:name {:type string :description "The habit's name, like Stretch"}
              :kind {:enum [yesNo measurable] :default yesNo
                     :description "yesNo: done or not each day; measurable: an amount each day"}}
  [name kind]
  (addHabitNamed name kind))

(defcommand habitDone "Marks a habit done, by its name, today or on another day."
  :title ~(t "action.habitDone.title") :description ~(t "action.habitDone.about")
  :category ~(t "action.habitDone.category") :icon "check"
  :arguments {:habit {:type string :description "The habit's name, like Meditate"}
              :date {:type [string null] :description "The day, like 2026-10-14; today when left out"}
              :amount {:type [number null] :minimum 0
                       :description "For a measurable habit, the amount; its target when left out"}}
  [habit date amount]
  (markDone habit date amount))

(defcommand habitRename "Gives a habit, by its name, a new name."
  :title ~(t "action.habitRename.title") :description ~(t "action.habitRename.about")
  :category ~(t "action.habitRename.category") :icon "edit"
  :arguments {:habit {:type string :description "The habit's name now"}
              :name {:type string :description "Its new name"}}
  [habit name]
  (renameHabit habit name))

;;; Archiving, picking and deleting

(defcommand archiveHabit "Archives the selected habit, or brings it back."
  :title ~(t "action.archiveHabit.title") :description ~(t "action.archiveHabit.about")
  :category ~(t "action.archiveHabit.category") :icon "archive" :enablement ~(>= computed.sel 0) []
  (update-record "habits" :id computed.h.id :fields {:archived (not (true? computed.h.archived))})
  (push "state.toasts" {:message (t (if (true? computed.h.archived) "toast.archived" "toast.unarchived") {:name computed.h.name})
                        :actions [{:title (t "undo") :run "archiveHabit"}]}))

(defcommand confirmDelete "Asks before deleting."
  :title ~(t "action.confirmDelete.title") :description ~(t "action.confirmDelete.about")
  :category ~(t "action.confirmDelete.category") :icon "trash-2" :enablement ~(>= computed.sel 0) []
  (set "state.confirmDelete" true))

(defn togglePick "Picks a row, or lets it go." [id]
  (if (contains state.picked id)
    (set "state.picked" (remove #{id} state.picked))
    (push "state.picked" id)))

(defn archivePicked "Archives the picked rows, or brings them back." []
  (set "state.unpicked" state.picked)
  (update-record "habits" :id state.picked :fields {:archived (not (true? computed.pickedArchived))})
  (push "state.toasts" {:message (t (if-not (true? computed.pickedArchived) "toast.unarchivedN" "toast.archivedN") {:n (count state.picked)})
                        :actions [{:title (t "undo") :run "unarchivePicked"}]})
  (set "state.picked" []))

(defn unarchivePicked "Undoes the last archive of picked rows." []
  (doseq [id state.unpicked]
    (update-record "habits" :id id :fields {:archived (not (:archived (find #(= (:id %) id) data.ordered)))})))

(defn deleteOne "Deletes the selected habit." []
  (toast (t "toast.deleted" {:name computed.h.name}))
  (delete "habits" :id computed.h.id)
  (set "state.selected" (or (get-in data.ordered [(or (first computed.order) 0) :id]) ""))
  (set "state.page" :list))

(defn deleteHabit "Deletes the picked rows, or the selected habit." []
  (if (true? computed.picking)
    (do (toast (t "toast.deletedN" {:n (count state.picked)}))
        (delete "habits" :id state.picked)
        (set "state.selected" (or (:id (first data.ordered)) ""))
        (set "state.picked" []))
    (deleteOne)))

;;; Export and backup

(defn exportedValue "A day's value in a habit's CSV." [habit value]
  (if (measurable? habit)
    (text value)
    (case value 1 "YES" -1 "SKIP" "NO")))

(defcommand exportHabit "Copies the selected habit as CSV."
  :title ~(t "action.exportHabit.title") :description ~(t "action.exportHabit.about")
  :category ~(t "action.exportHabit.category") :icon "file-down" :enablement ~(>= computed.sel 0) []
  (let [h computed.h
        lines (map-indexed (fn [index value]
                             (let [date (addDays (:from h) index)]
                               (str date "," (exportedValue h value) "," (get (:notes h) date ""))))
                           computed.v)]
    (copy (str "Date,Value,Note\n" (->> lines reverse (str/join "\n"))))
    (toast (t "toast.copied" {:n 1}))))

(defcommand exportAll "Copies every habit as CSV."
  :title ~(t "action.exportAll.title") :description ~(t "action.exportAll.about")
  :category ~(t "action.exportAll.category") :icon "download" []
  (copy (str "Habit,Date,Value\n" (->> computed.csvRows (map #(str/join "\n" (map :line %))) (str/join "\n"))))
  (toast (t "toast.copied" {:n (count data.ordered)})))

(defn backedUp "A backup was written." [result]
  (toast (t "toast.backedUp" {:n (count data.ordered) :name (:name result)})))

(defn notBackedUp "A backup failed." [error]
  (toast (t "toast.notBackedUp" {:message (:message error)})))

(defcommand backupAll "Saves every habit to a file the person chooses."
  :title ~(t "action.backupAll.title") :description ~(t "action.backupAll.about")
  :category ~(t "action.backupAll.category") :icon "hard-drive-download" []
  (saveFile {:format "streak-backup" :version 1 :habits data.ordered}
    :name (str "streak-backup-" state.today ".json")
    :onDone backedUp
    :onError notBackedUp))

(defn countRestored "" [] (increment "state.restoredCount"))
(defn countSkipped "" [] (increment "state.skippedCount"))

(defn restoreOne "One habit of a backup: updates the one of its id, or adds it." [id fields]
  (if (contains (map :id data.ordered) id)
    (do (update-record "habits" :id id :fields fields)
        (countRestored))
    (insert "habits" :value (merge fields {:id id}) :then countRestored :onError countSkipped)))

(def backedUpFields "What a backup may set of a habit."
  ["name" "question" "color" "kind" "frequency" "unit" "target" "targetType" "reminder" "description" "archived" "from"
   "entries" "notes" "position"])

(defn restoreEntry "One item of a backup's habits, if it is a habit." [item]
  (if (and (map? item) (string? (:id item)) (not= (:id item) ""))
    (restoreOne (:id item) (select-keys item backedUpFields))
    (countSkipped)))

(defn restoreHabits "What the file the person chose holds." []
  (if-not (and (= state.backupFile.format "streak-backup") (vector? state.backupFile.habits))
    (toast (t "toast.badBackup"))
    (do (set "state.restoredCount" 0)
        (set "state.skippedCount" 0)
        (doseq [item state.backupFile.habits] (restoreEntry item))
        (toast (if (pos? state.skippedCount)
                 (t "toast.restoredSkipped" {:n state.restoredCount :skipped state.skippedCount})
                 (t "toast.restored" {:n state.restoredCount})))))
  (set "state.backupFile" nil))

(defn notRestored "A restore failed." [error]
  (toast (t "toast.notRestored" {:message (:message error)})))

(defcommand restoreAll "Restores habits from a file the person chooses."
  :title ~(t "action.restoreAll.title") :description ~(t "action.restoreAll.about")
  :category ~(t "action.restoreAll.category") :icon "hard-drive-upload" []
  (openFile "state.backupFile" :extensions ["json"] :onDone restoreHabits :onError notRestored))

;;; The window's commands

(defcommand openSettings "Opens the settings."
  :title ~(t "action.openSettings.title") :description ~(t "action.openSettings.about")
  :category "Streak" :icon "settings" []
  (set "state.settingsOpen" true))

(defcommand toggleTheme "Between light and dark."
  :title ~(t "action.toggleTheme.title") :description ~(t "action.toggleTheme.about")
  :category "Streak" :icon "moon" []
  (set "state.theme" (if (= computed.theme :light) :dark :light)))

(defcommand toggleHideCompleted "Shows or hides what is done today."
  :title ~(t "action.toggleHideCompleted.title") :description ~(t "action.toggleHideCompleted.about")
  :category "Streak" :icon "eye" []
  (toggle "state.hideCompleted"))

(defcommand toggleShowArchived "Shows or hides what is archived."
  :title ~(t "action.toggleShowArchived.title") :description ~(t "action.toggleShowArchived.about")
  :category "Streak" :icon "archive" []
  (toggle "state.showArchived"))

(defcommand nextHabit "Selects the habit below."
  :title ~(t "action.nextHabit.title") :description ~(t "action.nextHabit.about")
  :category ~(t "action.nextHabit.category") []
  (let [order computed.order
        row (get order (min (dec (count order)) (inc (indexOf order computed.sel))))]
    (set "state.selected" (or (get-in data.ordered [row :id]) state.selected))))

(defcommand prevHabit "Selects the habit above."
  :title ~(t "action.prevHabit.title") :description ~(t "action.prevHabit.about")
  :category ~(t "action.prevHabit.category") []
  (let [order computed.order
        row (get order (max 0 (dec (indexOf order computed.sel))))]
    (set "state.selected" (or (get-in data.ordered [row :id]) state.selected))))

(defcommand checkToday "Marks today on the selected habit."
  :title ~(t "action.checkToday.title") :description ~(t "action.checkToday.about")
  :category ~(t "action.checkToday.category") :icon "check" :enablement ~(>= computed.sel 0) []
  (checkToday-at computed.sel))

(defcommand commandPalette "Opens the list of commands."
  :title ~(t "action.commandPalette.title") :description ~(t "action.commandPalette.about")
  :category "Streak" :icon "search" []
  (set "state.quickPick.open" true)
  (set "state.quickPick.query" ""))

;;; The clock and reminders

(defn remindNow "A habit's reminder, if this is its minute." [row item]
  (when-some [reminder (:reminder item)]
    (when (and (not (:archived item))
               (= (:time reminder) (formatTime (now) "HH:mm"))
               (get (:days reminder) (weekday state.today)))
      (push "state.toasts" {:message (t "toast.reminder" {:name (:name item)})
                            :actions [{:title (t "toast.done") :run "cellClick"
                                       :with {:h row :d state.today :x 0 :y 0}}]}))))

(defn tick "Every minute: today's date, and the reminders due." []
  (set "state.today" (today))
  (when (true? state.reminders)
    (doseq [[row item] (map-indexed vector data.ordered)] (remindNow row item))))

;;; Syncing with the document server

(defn syncReceive "The habits the server holds." [incoming]
  (when-some [habits (:habits incoming)]
    (delete "habits" :where {})
    (doseq [[position item] (map-indexed vector habits)] (insert "habits" :value (assoc item :position position)))
    (set "state.sync.copy" computed.syncData)
    (toast (t "sync.received"))))

(defn syncSend "Sends the habits, if they changed since the last time." []
  (when (and (true? computed.online) (not= (json computed.syncData) (json state.sync.copy)))
    (increment "state.sync.revisions")
    (set "state.sync.copy" computed.syncData)
    (request "streakServer.revision" :with {:id (str "streak-" (text state.sync.revisions)) :data computed.syncData})))

(defcommand syncNow "Syncs now, or says why it can't."
  :title ~(t "action.syncNow.title") :description ~(t "action.syncNow.about")
  :category ~(t "action.syncNow.category") :icon "refresh-cw" []
  (if (true? computed.online)
    (syncSend)
    (toast (t (str "sync." computed.syncState) {:message state.sync.error}))))

(defn signedIn "" [] (toast (t "signedIn" {:account (or auth.streakServer.account (t "you"))})))
(defn notSignedIn "" [error] (toast (t "notSignedIn" {:message (:message error)})))

(defcommand signInToServer "Signs in to the document server."
  :title ~(t "action.signIn.title") :description ~(t "action.signInToServer.about")
  :category ~(t "action.signIn.category") :icon "log-in" :enablement ~(not computed.signedIn) []
  (signIn "streakServer" :onDone signedIn :onError notSignedIn))

(defcommand signOutOfServer "Signs out of the document server."
  :title ~(t "action.signOut.title") :description ~(t "action.signOutOfServer.about")
  :category ~(t "action.signOut.category") :icon "log-out" :enablement ~computed.signedIn []
  (signOut "streakServer")
  (set "state.sync" {:revisions state.sync.revisions :copy nil :error "" :commits 0}))

(defn serverOpened "" []
  (set "state.sync.error" "")
  (request "streakServer.open"))

(defn serverHas "" [event] (syncReceive (:state event)))

(defn serverCommitted "" []
  (increment "state.sync.commits")
  (request "streakServer.getDocument"))

(defn serverFailed "" [event]
  (set "state.sync.error" (or (:message event) (:error event) "error")))

(defn serverClosed "" [] (set "state.sync.copy" nil))

;;; Keys

(defkey "mod+n" addYesNo)
(defkey "mod+shift+n" addMeasurable)
(defkey "mod+e" editHabit)
(defkey "mod+backspace" confirmDelete)
(defkey "mod+shift+h" archiveHabit)
(defkey "mod+comma" openSettings)
(defkey "mod+j" toggleTheme)
(defkey "mod+k" commandPalette)
(defkey "mod+shift+p" commandPalette)
(defkey "mod+s" syncNow)
(defkey "mod+down" nextHabit)
(defkey "mod+up" prevHabit)
(defkey "mod+enter" checkToday)

(defkey "escape"
  {:when ~(or computed.picking (and (not computed.wide) (= state.page "detail"))) :do [{:lisp leaveEscape}]})

;;; The views' handlers

(defn clickRow "A row clicked: picked or let go while picking, else shown in the detail." [h id]
  (if (truthy? computed.picking) (togglePick id) (select h)))

(defn stepTimes "The frequency's times, one less or one more: at least 1, at most 7 a week or 31 a month." [by]
  (set "state.edit.n" (if (neg? by)
                        (max 1 (dec state.edit.n))
                        (min (if (= state.edit.freqMode :week) 7 31) (inc state.edit.n)))))

(defn pickColor "A colour of the palette chosen for the habit edited; the palette closes." [colour]
  (set "state.edit.color" colour)
  (set "state.colorOpen" false))

(defn openPeriodMenu "Opens a card's period menu: the score's or the history's." [key]
  (set (str "state.menus." (case key :scorePeriod "score" "history")) true))
