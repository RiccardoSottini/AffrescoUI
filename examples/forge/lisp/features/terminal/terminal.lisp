;;; terminal.lisp --- Forge's terminal: up to 8 shells in the panel's Terminal tab, each a tab of its own (the terminal node, its shell on the terminal service of the pty provider)
;;;
;;; The Terminal tab shows only while a terminal is open. The node keeps Tab for the shell. Run Task (and mod+shift+b,
;;; the build task) runs a task of .forge/tasks.json (read through files.read, parsed with parseJsonc) in a new
;;; terminal, through runInTerminal.

;;; State and definitions

(defstate terminals {:list [] :active "" :next 1})

(defstate tasks {:items [] :skipped 0 :error "" :last ""})

(defkey "mod+alt+t" terminalNew)

(defkey "mod+shift+b" runBuildTask)

(defpart terminal "What terminal adds." :services {:terminal {:provider pty :process shell :root workspace}})

;;; Functions and commands

(defcommand terminalNew "New Terminal"
  :title ~(t "command.terminalNew") :category ~(t "category.terminal") :icon "terminal"
  :enablement ~(< (count state.terminals.list) 8) []
  (push "state.terminals.list"
        {:id (str "t" state.terminals.next) :label (str "Terminal " state.terminals.next) :closable true})
  (set "state.terminals.active" (str "t" state.terminals.next))
  (increment "state.terminals.next" :by 1)
  (set "state.layout.panel" true)
  (set "state.panelTab" "terminal")
  (after 0 terminalNewLater))

(defn terminalClose "TODO" [id]
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
    (do (set "state.layout.panel" true) (set "state.panelTab" "terminal") (after 0 showTerminalLater))))

(defcommand terminalClear "Clear Terminal"
  :title ~(t "command.terminalClear") :category ~(t "category.terminal") :icon "eraser"
  :enablement ~(not (empty? state.terminals.active)) []
  (clearTerminal state.terminals.active))

(defn terminalKeepTab "TODO" []
  nil)

(defcommand runTask "Pick a task of the open folder's .forge/tasks.json and run it in a new terminal."
  :title ~(t "command.runTask") :category ~(t "category.terminal") :icon "play"
  :description ~(t "command.runTask.about") []
  (tasksLoad "pick"))

(defcommand runBuildTask "Run the build task of .forge/tasks.json, or pick one if there are several."
  :title ~(t "command.runBuildTask") :category ~(t "category.terminal") :icon "play"
  :description ~(t "command.runBuildTask.about") []
  (tasksLoad "build"))

(defn tasksLoad "TODO" [then]
  (if (empty? state.folder.root)
    (tasksApply {:tasks [] :skipped 0} then)
    (invoke "files.read"
            :with {:path ".forge/tasks.json"}
            :onError [tasksLoadFailed then]
            :onDone [tasksLoadDone then])))

(defn tasksFail "TODO" [reason]
  (set "state.tasks.error" (str "Cannot read .forge/tasks.json: " reason))
  (notify "error" state.tasks.error))

(defn tasksReadFile "TODO" [file then]
  (if (nil? file)
    (tasksFail "it isn't JSON")
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

(defn tasksQuote "TODO" [then total known]
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

(defn tasksLine "TODO" [then total quoted]
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

(defn tasksKeep "TODO" [then total kept]
  (tasksApply {:tasks (slice kept 0 100) :skipped (- total (min (count kept) 100))} then))

(defn tasksApply "TODO" [found then]
  (set "state.tasks.items"
       (map (fn [it] {:title it.label :detail it.line :line it.line :group it.group}) (or-else found.tasks [])))
  (set "state.tasks.skipped" (or-else found.skipped 0))
  (set "state.tasks.error" "")
  (cond
    (and (= then "build") (= (count (filter (fn [it] (= it.group "build")) state.tasks.items)) 1))
      (taskStart (find (fn [it] (= it.group "build")) state.tasks.items))
    (and (not= then "") (empty? state.tasks.items)) (notify "info" "No tasks in .forge/tasks.json")
    :else (do (when (not= then "") (set "state.quickPick.query" "task ") (set "state.quickPick.open" true)))))

(defn tasksApplyFile "TODO" []
  (tasksReadFile event.file (or-else event.then "")))

(defn taskRunEvent "TODO" [event]
  (taskStart event))

(defn taskStart "TODO" [item]
  (if (>= (count state.terminals.list) 8)
    (notify "warning" "Close a terminal to run a task")
    (do
      (runInTerminal (str "t" state.terminals.next) :text item.line)
      (set "state.tasks.last" item.title)
      (terminalNew)
      (set "state.terminals.list"
           (map (fn [it] (if (= it.id state.terminals.active) (merge it {:label (str "Task - " item.title)}) it))
                state.terminals.list)))))

(defcommand rerunLastTask "Rerun Last Task"
  :title ~(t "command.rerunLastTask") :category ~(t "category.terminal") :icon "refresh-cw"
  :enablement ~(not= state.tasks.last "") []
  (taskStart (find (fn [it] (= it.title state.tasks.last)) state.tasks.items)))

(defn terminalNewLater "What terminalNew does when its time comes." []
  (focus "#terminal"))

(defn showTerminalLater "What showTerminal does when its time comes." []
  (focus "#terminal"))

(defn tasksLoadFailed "What tasksLoad does when it fails." [then error]
  (if (= error.code "notFound") (tasksApply {:tasks [] :skipped 0} then) (tasksFail error.message)))

(defn tasksLoadDone "What tasksLoad does when it is done." [then result]
  (tasksReadFile (parseJsonc result.text) then))
