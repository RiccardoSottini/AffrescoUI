;;; panels.lisp --- Forge's panel: Problems, Output, the notification centre and progress
;;;
;;; Problems lists the TODO and FIXME lines of the open files and the language server's diagnostics as a tree, each
;;; file with its problems under it, errors first, and opening one goes to the line; Output is a log every feature writes to with notify and log-line; the notification centre over the
;;; status bar's bell keeps the toasts' history; a long task shows a toast with a bar. The panel's tabs and the
;;; status bar's counts come from this state.

(ns forge.features.panels.panels)

;;; Problems

(defstate problems [])

(defcomputed errors (count (filter #(= (:severity %) "error") state.problems)))

(defcomputed warnings (count (filter #(= (:severity %) "warning") state.problems)))

(defstate problemsFilter "")

(defstate problemsFolded [] "The files whose problems are folded away in Problems, by their rows' ids.")

(defn severity-rank "Where a severity sorts: errors first, then warnings, then the rest." [severity]
  (case severity :error 0 :warning 1 2))

(defn problem-matches "Whether a problem's message or file has the filter's text (`query`, lower case)." [query it]
  (let [text (str/lower-case (str it.message " " it.file))]
    (or (empty? query) (str/includes? text query))))

(defn problem-row "A problem's row (`it`, its place in the list as `index`): its severity's icon, its message and line." [it]
  {:id it.index
   :icon (case it.severity :error "circle-x" :warning "alert-triangle" "info")
   :iconColor (case it.severity :error "#f85149" :warning "#cca700" "#3794ff")
   :label (t "problems.row" {:message it.message :line it.line})})

(defn problem-file-row "A file's row in Problems, its name and how many problems it has, those problems under it." [rows]
  (let [{:keys [file label]} (first rows)]
    {:id (str "file:" file)
     :icon "file-code"
     :label (t "problems.fileRow" {:name label :path file :n (count rows)})
     :children (map problem-row rows)}))

(defn problems-kept "The problems (`problems`) the filter (`filter-text`) keeps, numbered, errors first." [problems filter-text]
  (let [query (str/lower-case (str/trim filter-text))
        numbered (map-indexed (fn [index it] (assoc it :index index)) problems)]
    (sort-by #(severity-rank (:severity %)) (filter #(problem-matches query %) numbered))))

(defn problem-rows "The problems kept (`kept`), as a row per file with its problems under it, the files by path." [kept]
  (let [by-file (group-by :file kept)]
    (map #(problem-file-row (get by-file %)) (sort (keys by-file)))))

(defcomputed problemsShown (problems-kept state.problems state.problemsFilter))

(defcomputed problemRows (problem-rows computed.problemsShown))

(defcomputed problemsOpen "The files open in Problems: every one but those folded."
  (remove #(contains state.problemsFolded %) (map :id computed.problemRows)))

(defn fold-problems "A file's row in Problems opened or closed: kept folded or not." []
  (set "state.problemsFolded" (if event.expanded
                                (remove #(= % event.id) state.problemsFolded)
                                (conj state.problemsFolded event.id))))

(defn go-to-problem "A problem's row of Problems, opened: its file shown at its line (a file's row folds instead)." []
  (when-not (str/starts-with? (str event.id) "file:")
    (let [{:keys [file label line]} (get state.problems (number event.id))]
      (showTab file label)
      (go-to-line line))))

(defcommand showProblems "Focus Problems"
  :title ~(t "command.showProblems")
  :description ~(t "command.showProblems") :category ~(t "category.view") :icon "alert-triangle" []
  (set "state.layout.panel" true)
  (set "state.panelTab" :problems))

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
  (debounce 4000 hide-toast))

(defstate toast {:open false :message "" :progress nil})

(defn hide-toast "Hides the toast in the corner." []
  (set "state.toast.open" false))

(defcommand showOutput "Focus Output"
  :title ~(t "command.showOutput")
  :description ~(t "command.showOutput") :category ~(t "category.view") :icon "terminal" []
  (set "state.layout.panel" true)
  (set "state.panelTab" :output))

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

(defn open-terminal-with-panel "Opens a terminal when the Terminal tab is shown with none." []
  (when (and (= state.panelTab :terminal) (empty? state.terminals.list) (true? state.layout.panel)) (terminalNew)))

(defpart terminal-on-show "The Terminal tab, shown with none open, opens one."
  :watch {:state.panelTab open-terminal-with-panel})

;;; Indexing the folder again

(defcommand reindex "Read the folder again, showing the progress."
  :title ~(t "command.reindex") :category ~(t "category.files") :icon "folder"
  :description ~(t "command.reindex.about") []
  (set "state.toast" {:open true :message (t "toast.indexing") :progress 0.2})
  (explorerRefresh)
  (invoke "search.files" :with {:exclude state.folder.skip} :onError reindex-failed :onDone reindex-done))

(defn reindex-done "The folder's files indexed for Go to File; Output says how many." [result]
  (indexed result.files)
  (set "state.toast.progress" 1)
  (log-line (t (if result.truncated "log.indexedTruncated" "log.indexed") {:n (count result.files)}))
  (debounce 1500 hide-toast))

(defn reindex-failed "Says why the folder couldn't be indexed." [error]
  (notify "error" (t "notify.indexingFailed" {:error error.message})))
