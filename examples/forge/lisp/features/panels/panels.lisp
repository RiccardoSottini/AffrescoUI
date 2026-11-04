;;; panels.lisp --- Forge's panel: Problems, Output, the notification centre and progress
;;;
;;; Problems lists the TODO and FIXME lines of the open files and the language server's diagnostics, and a click goes
;;; to the line; Output is a log every feature writes to with notify and log-line; the notification centre over the
;;; status bar's bell keeps the toasts' history; a long task shows a toast with a bar. The panel's tabs and the
;;; status bar's counts come from this state.

;;; Problems

(defstate problems [])

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

(defn goToProblem "A row of Problems, opened: its file shown at its line." []
  (let [p (get state.problems (number event.id))]
    (showTab p.file p.label)
    (goToLine p.line)))

(defcommand showProblems "Focus Problems"
  :title ~(t "command.showProblems")
  :description ~(t "command.showProblems") :category ~(t "category.view") :icon "alert-triangle" []
  (set "state.layout.panel" true)
  (set "state.panelTab" "problems"))

(defkey "mod+shift+m" showProblems)

;;; Output and notices

(defstate output [])

(defcomputed outputRows
  (map-indexed (fn [index it] {:id index :label (str (time it.time "HH:mm:ss") "  " it.text)}) state.output))

(defn log-line "A line in Output." [text]
  (push "state.output" {:time (now) :text text}))

(defn notify "A notice: in Output with its severity, in the notification centre, and as a toast for a few seconds."
  [severity message]
  (push "state.output" {:time (now) :text (str "[" severity "] " message)})
  (push "state.notifications" {:time (now) :severity severity :message message})
  (set "state.toast" {:open true :message message :progress nil})
  (debounce 4000 hideToast))

(defstate toast {:open false :message "" :progress nil})

(defn hideToast "Hides the toast in the corner." []
  (set "state.toast.open" false))

(defcommand showOutput "Focus Output"
  :title ~(t "command.showOutput")
  :description ~(t "command.showOutput") :category ~(t "category.view") :icon "terminal" []
  (set "state.layout.panel" true)
  (set "state.panelTab" "output"))

(defcommand clearOutput "Clear Output"
  :title ~(t "command.clearOutput") :description ~(t "command.clearOutput") :category ~(t "category.output") []
  (set "state.output" []))

(defkey "mod+shift+u" showOutput)

;;; The notification centre

(defstate notifications [])

(defcomputed notificationRows
  (map-indexed (fn [index it] {:id index :label it.message :icon (if (= it.severity "error") "circle-x" "bell")})
                 (reverse state.notifications)))

(defcommand showNotifications "Show Notifications"
  :title ~(t "command.showNotifications")
  :description ~(t "command.showNotifications") :category ~(t "category.view") :icon "bell" []
  (toggle "state.notificationCentre"))

(defcommand clearNotifications "Clear All Notifications"
  :title ~(t "command.clearNotifications")
  :description ~(t "command.clearNotifications") :category ~(t "category.notifications") []
  (set "state.notifications" [])
  (set "state.toast.open" false)
  (set "state.notificationCentre" false))

;;; The panel's tabs

(defstate panelTab problems)

(defn openTerminalWithPanel "Opens a terminal when the Terminal tab is shown with none." []
  (when (and (= state.panelTab "terminal") (= (count state.terminals.list) 0) state.layout.panel) (terminalNew)))

(defpart terminal-on-show "The Terminal tab, shown with none open, opens one."
  :watch {:state.panelTab openTerminalWithPanel})

;;; Indexing the folder again

(defcommand reindex "Read the folder again, showing the progress."
  :title ~(t "command.reindex") :category ~(t "category.files") :icon "folder"
  :description ~(t "command.reindex.about") []
  (set "state.toast" {:open true :message (t "toast.indexing") :progress 0.2})
  (explorerRefresh)
  (invoke "search.files" :with {:exclude state.folder.skip} :onError reindexFailed :onDone reindexDone))

(defn reindexDone "The folder's files indexed for Go to File; Output says how many." [result]
  (indexed result.files)
  (set "state.toast.progress" 1)
  (log-line (t (if result.truncated "log.indexedTruncated" "log.indexed") {:n (count result.files)}))
  (debounce 1500 hideToast))

(defn reindexFailed "Says why the folder couldn't be indexed." [error]
  (notify "error" (t "notify.indexingFailed" {:error error.message})))
