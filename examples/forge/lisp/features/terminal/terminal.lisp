;;; terminal.lisp --- Forge's terminals, and the tasks they run
;;;
;;; Up to 8 shells in the panel's Terminal tab, each a tab of its own: the terminal node, its shell on the terminal
;;; service of the pty provider. The node keeps Tab for the shell. Run Task (and Cmd+Shift+B, the build task) runs a
;;; task of .forge/tasks.json, read through files.read and parsed with parseJsonc, in a new terminal, through
;;; runInTerminal.

;;; Terminals

(defstate terminals {:list [] :active "" :next 1})

(defpart terminal-service "The shells: the pty provider's, in the open folder."
  :services {:terminal {:provider pty :process shell :root workspace}})

(defcommand terminalNew "New Terminal"
  :title ~(t "command.terminalNew") :category ~(t "category.terminal") :icon "terminal"
  :enablement ~(< (count state.terminals.list) 8) []
  (push "state.terminals.list"
        {:id (str "t" state.terminals.next) :label (t "terminal.label" {:n state.terminals.next}) :closable true})
  (set "state.terminals.active" (str "t" state.terminals.next))
  (increment "state.terminals.next" :by 1)
  (set "state.layout.panel" true)
  (set "state.panelTab" "terminal")
  (after 0 focusTerminal))

(defn focusTerminal "Focuses the terminal shown, once it is." []
  (focus "#terminal"))

(defn terminalClose "Closes a terminal and its shell; the Output tab shows when the last one goes." [id]
  (closeTerminal id)
  (set "state.terminals.list" (filter (fn [it] (not= it.id id)) state.terminals.list))
  (when (= state.terminals.active id)
    (set "state.terminals.active" (if (> (count state.terminals.list) 0) state.terminals.list.0.id "")))
  (when (and (= (count state.terminals.list) 0) (= state.panelTab "terminal")) (set "state.panelTab" "output")))

(defcommand terminalKill "Kill the Active Terminal"
  :title ~(t "command.terminalKill") :category ~(t "category.terminal") :icon "trash-2"
  :enablement ~(not (empty? state.terminals.active)) []
  (terminalClose state.terminals.active))

(defcommand showTerminal "Focus Terminal"
  :title ~(t "command.showTerminal") :category ~(t "category.view") :icon "terminal" []
  (if (= (count state.terminals.list) 0)
    (terminalNew)
    (do (set "state.layout.panel" true) (set "state.panelTab" "terminal") (after 0 focusTerminal))))

(defcommand terminalClear "Clear Terminal"
  :title ~(t "command.terminalClear") :category ~(t "category.terminal") :icon "eraser"
  :enablement ~(not (empty? state.terminals.active)) []
  (clearTerminal state.terminals.active))

(defn terminalKeepTab "The terminal keeps Tab for its shell: nothing else runs." []
  nil)

(defkey "mod+alt+t" terminalNew)

;;; Tasks

(defstate tasks {:items [] :skipped 0 :error "" :last ""})

(defcommand runTask "Pick a task of the open folder's .forge/tasks.json and run it in a new terminal."
  :title ~(t "command.runTask") :category ~(t "category.terminal") :icon "play"
  :description ~(t "command.runTask.about") []
  (tasksLoad "pick"))

(defcommand runBuildTask "Run the build task of .forge/tasks.json, or pick one if there are several."
  :title ~(t "command.runBuildTask") :category ~(t "category.terminal") :icon "play"
  :description ~(t "command.runBuildTask.about") []
  (tasksLoad "build"))

(defcommand rerunLastTask "Rerun Last Task"
  :title ~(t "command.rerunLastTask") :category ~(t "category.terminal") :icon "refresh-cw"
  :enablement ~(not= state.tasks.last "") []
  (taskStart (find (fn [it] (= it.title state.tasks.last)) state.tasks.items)))

(defkey "mod+shift+b" runBuildTask)

(defn tasksLoad "Reads .forge/tasks.json, then picks a task (`then` pick), runs the build task (build), or only lists them ('')."
  [then]
  (if (empty? state.folder.root)
    (tasksApply {:tasks [] :skipped 0} then)
    (invoke "files.read"
            :with {:path ".forge/tasks.json"}
            :onError [tasksLoadFailed then]
            :onDone [tasksLoadDone then])))

(defn tasksLoadDone "The tasks file read, as JSON with comments." [then result]
  (tasksReadFile (parseJsonc result.text) then))

(defn tasksLoadFailed "No tasks file is no tasks; another failure is said." [then error]
  (if (= error.code "notFound") (tasksApply {:tasks [] :skipped 0} then) (tasksFail error.message)))

(defn tasksFail "Says why the tasks file couldn't be read." [reason]
  (set "state.tasks.error" (t "tasks.cannotRead" {:reason reason}))
  (notify "error" state.tasks.error))

(defn tasksApplyFile "Plays a tasks file given in the event (`event.file`), as the scenarios do." []
  (tasksReadFile event.file (or-else event.then "")))

(defn tasksReadFile "The tasks file's tasks, if it has some." [file then]
  (if (nil? file)
    (tasksFail (t "tasks.notJson"))
    (tasksRead then (if (and (= (type file) "object") (= (type file.tasks) "list")) file.tasks []))))

(defn tasksRead "The tasks Forge runs: type shell (its command a line for the shell) or process (one program), with plain text args, each quoted by shellLine(); ${workspaceFolder} becomes '.' (the terminal starts in the folder), and a task naming another variable is skipped. The first 100 are kept, the others counted as skipped."
  [then listed]
  (tasksQuote then
              (count listed)
              (map (fn [it]
                     {:t it
                      :shell (not= it.type "process")
                      :group (if (= (type it.group) "object") it.group.kind it.group)
                      :command (if (= (type it.command) "string")
                                 (join "." (split it.command "${workspaceFolder}"))
                                 nil)
                      :args (cond
                              (nil? it.args) []
                              (and (= (type it.args) "list") (<= (count it.args) 1000))
                                (map (fn [it]
                                       (if (= (type it) "string") (join "." (split it "${workspaceFolder}")) nil))
                                     it.args)
                              :else nil)})
                   (filter (fn [it]
                             (and (= (type it) "object")
                                  (some? it.command)
                                  (or (nil? it.type) (= it.type "shell") (= it.type "process"))))
                           listed))))

(defn tasksQuote "Each task's command and arguments quoted for the shell; a task that can't be is left out."
  [then total known]
  (tasksLine then
             total
             (filter (fn [it] (and (some? it.quotedCommand) (some? it.quotedArgs)))
                     (map (fn [it]
                            (merge it {:quotedCommand (shellLine [it.command]) :quotedArgs (shellLine it.args)}))
                          (filter (fn [it]
                                    (and (some? it.command)
                                         (not= it.command "")
                                         (some? it.args)
                                         (not (contains it.command "${"))
                                         (every? (fn [it] (or (nil? it) (not (contains it "${")))) it.args)))
                                  known)))))

(defn tasksLine "Each task's line, as the shell runs it, its label and its group." [then total quoted]
  (tasksKeep then
             total
             (filter (fn [it] (<= (count it.line) 60000))
                     (map (fn [it]
                            {:label (if (and (= (type it.t.label) "string")
                                             (not= it.t.label "")
                                             (some? (shellLine [it.t.label])))
                                      it.t.label
                                      it.command)
                             :line (str (if it.shell it.command it.quotedCommand)
                                        (if (= it.quotedArgs "") "" (str " " it.quotedArgs)))
                             :group (if (or (= it.group "build") (= it.group "test")) it.group "")})
                          quoted))))

(defn tasksKeep "The first 100 tasks; the rest are counted as skipped." [then total kept]
  (tasksApply {:tasks (slice kept 0 100) :skipped (- total (min (count kept) 100))} then))

(defn tasksApply "The tasks found: the build task runs, or the palette lists them to pick one." [found then]
  (set "state.tasks.items"
       (map (fn [it] {:title it.label :detail it.line :line it.line :group it.group}) (or-else found.tasks [])))
  (set "state.tasks.skipped" (or-else found.skipped 0))
  (set "state.tasks.error" "")
  (cond
    (and (= then "build") (= (count (filter (fn [it] (= it.group "build")) state.tasks.items)) 1))
      (taskStart (find (fn [it] (= it.group "build")) state.tasks.items))
    (and (not= then "") (empty? state.tasks.items)) (notify "info" (t "tasks.none"))
    (not= then "") (do (set "state.quickPick.query" "task ") (set "state.quickPick.open" true))))

(defn taskRunEvent "A task picked, run." [event]
  (taskStart event))

(defn taskStart "Runs a task in a new terminal, named after it; at 8 terminals, says to close one." [item]
  (if (>= (count state.terminals.list) 8)
    (notify "warning" (t "tasks.tooMany"))
    (do
      (runInTerminal (str "t" state.terminals.next) :text item.line)
      (set "state.tasks.last" item.title)
      (terminalNew)
      (set "state.terminals.list"
           (map (fn [it] (if (= it.id state.terminals.active) (merge it {:label (t "terminal.task" {:title item.title})}) it))
                state.terminals.list)))))
