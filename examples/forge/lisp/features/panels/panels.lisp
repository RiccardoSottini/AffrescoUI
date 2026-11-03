;;; panels.lisp --- Forge's panels: Problems (TODO and FIXME lines of the open files, a click goes to the line), Output (a log every feature writes to with the notify action), the notification centre over the status bar's bell (the toast's history) and progress (a toast with a bar while a long task runs)
;;;
;;; The panel's tabs and the status bar counts come from this state.

;;; State and definitions

(defstate problems [])

(defstate output [])

(defstate notifications [])

(defstate toast {:open false :message "" :progress nil})

(defstate panelTab problems)

(defcomputed errors (count (filter (fn [it] (= it.severity "error")) state.problems)))

(defcomputed warnings (count (filter (fn [it] (= it.severity "warning")) state.problems)))

(defcomputed problemRows
  (map-indexed (fn [index it]
                   {:id index
                    :icon (cond (= it.severity "error") "circle-x"
                                (= it.severity "warning") "alert-triangle"
                                :else "info")
                    :iconColor (cond (= it.severity "error") "#f85149"
                                     (= it.severity "warning") "#cca700"
                                     :else "#3794ff")
                    :cells [it.message it.label it.line]})
                 state.problems))

(defcomputed outputRows
  (map-indexed (fn [index it] {:id index :label (str (time it.time "HH:mm:ss") "  " it.text)}) state.output))

(defcomputed notificationRows
  (map-indexed (fn [index it] {:id index :label it.message :icon (if (= it.severity "error") "circle-x" "bell")})
                 (reverse state.notifications)))

(defkey "mod+shift+m" showProblems)

(defkey "mod+shift+u" showOutput)

(defpart panels "What panels adds." :watch {:state.panelTab openTerminalWithPanel})

;;; Functions and commands

(defn notify "TODO" [severity message]
  (push "state.output" {:time (now) :text (str "[" severity "] " message)})
  (push "state.notifications" {:time (now) :severity severity :message message})
  (set "state.toast" {:open true :message message :progress nil})
  (debounce 4000 notifyLater))

(defn log "TODO" [text]
  (push "state.output" {:time (now) :text text}))

(defn goToProblem "TODO" []
  (doseq [p [(get state.problems (number event.id))]] (showTab p.file p.label) (goToLine p.line)))

(defcommand showProblems "Focus Problems"
  :title ~(t "command.showProblems") :category ~(t "category.view") :icon "alert-triangle" []
  (set "state.layout.panel" true)
  (set "state.panelTab" "problems"))

(defcommand showOutput "Focus Output"
  :title ~(t "command.showOutput") :category ~(t "category.view") :icon "terminal" []
  (set "state.layout.panel" true)
  (set "state.panelTab" "output"))

(defcommand showNotifications "Show Notifications"
  :title ~(t "command.showNotifications") :category ~(t "category.view") :icon "bell" []
  (toggle "state.notificationCentre"))

(defcommand clearOutput "Clear Output"
  :title ~(t "command.clearOutput") :category ~(t "category.output") []
  (set "state.output" []))

(defcommand clearNotifications "Clear All Notifications"
  :title ~(t "command.clearNotifications") :category ~(t "category.notifications") []
  (set "state.notifications" [])
  (set "state.toast.open" false)
  (set "state.notificationCentre" false))

(defcommand reindex "Read the folder again, showing the progress."
  :title ~(t "command.reindex") :category ~(t "category.files") :icon "folder"
  :description ~(t "command.reindex.about") []
  (set "state.toast" {:open true :message "Indexing the folder" :progress 0.2})
  (explorerRefresh)
  (invoke "search.files" :with {:exclude state.folder.skip} :onError reindexFailed :onDone reindexDone))

(defn openTerminalWithPanel "TODO" []
  (when (and (= state.panelTab "terminal") (= (count state.terminals.list) 0) state.layout.panel) (terminalNew)))

(defn notifyLater "What notify does when its time comes." []
  (set "state.toast.open" false))

(defn reindexFailed "What reindex does when it fails." [error]
  (notify "error" (str "Indexing failed: " error.message)))

(defn reindexDone "What reindex does when it is done." [result]
  (indexed result.files)
  (set "state.toast.progress" 1)
  (log (str "Indexed " (plural (count result.files) "file") (if result.truncated " (truncated)" "")))
  (debounce 1500 reindexDoneLater))

(defn reindexDoneLater "What reindexDone does when its time comes." []
  (set "state.toast.open" false))
