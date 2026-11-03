;;; debug.lisp --- Forge's debugger: launch configurations, a session, breakpoints, the stack, variables, watches
;;;
;;; Launch configurations are read from .forge/launch.json through the files service (comments allowed:
;;; parseJsonc); a session runs lldb-dap or debugpy through the dap service the host granted. Every request goes
;;; through debugRequest, which names the service by state.debug.type and runs the answer's function with `body` and
;;; `context`; the adapter's events go to debugEvent. The functions that take the adapter's messages as events
;;; (debugApplySession, debugApplyEvent, debugApplyResponse) let a scenario or an agent drive the views without an
;;; adapter. What an adapter sends is bounded in the dap service; here the console keeps 2000 lines, the history 100
;;; entries and the variables 5000 rows. The views are in views.lisp.

;;; The view and the configurations

(defstate debug
  (node :type ""
    :status inactive
    :configurations []
    :selected "0"
    :skipped 0
    :error ""
    :capabilities {}
    :exceptionFilters []
    :filterChoices {}
    :configured false
    :restartAfter false
    :threadId nil
    :reason ""
    :exception ""
    :threads []
    :frames []
    :frameId nil
    :selectedStack nil
    :current {:path "" :line 0 :inside false}
    :variables []
    :selectedVariable nil
    :watches []
    :watchNext 1
    :watchDraft ""
    :selectedWatch nil
    :console []
    :consoleNext 1
    :input ""
    :history []
    :historyAt 0
    :completions []
    :breakpoints []
    :breakpointNext 1
    :selectedBreakpoint nil
    :collapsed []
    :edit {:open false :id "" :condition "" :hitCondition "" :logMessage ""}
    :setValue {:open false :path "" :name "" :value ""}
    :outside {:open false :frame nil}
    :hover {:path "" :word "" :line 0 :value ""}
    :terminalSent false))

(defcomputed debugConfiguration (or-else (get state.debug.configurations (number state.debug.selected)) nil))

(defcomputed debugConfigurationRows
  (map-indexed (fn [index it] {:id (text index) :label it.name :icon (if (= it.type "python") "file-code" "cpu")})
                 state.debug.configurations))

(defpart debug-services "The debug adapters, lldb-dap and debugpy, and the terminal a debugged program runs on."
  :services {:debugLldb {:provider dap :process lldb-dap :root workspace :restart {:after 0 :times 100}}
             :debugPython {:provider dap :process debugpy-adapter :root workspace :restart {:after 0 :times 100}}
             :debugTerminal {:provider pty :process sh :root workspace}})

(defcommand showDebug "Show Debugger"
  :title ~(t "command.showDebug") :description ~(t "command.showDebug") :category ~(t "category.view") :icon "bug" []
  (forgeShowView "debug")
  (when (and (= state.sideView "debug") (= state.debug.status "inactive")) (debugLoadConfigurations)))

(defcommand debugLoadConfigurations "Read the open folder's .forge/launch.json again."
  :title ~(t "command.debugLoadConfigurations") :category ~(t "category.debug") :icon "refresh-cw"
  :description ~(t "command.debugLoadConfigurations.about") []
  (if (empty? state.folder.root)
    (debugApplyConfigurations {:configurations [] :skipped 0})
    (invoke "files.read"
            :with {:path ".forge/launch.json"}
            :onError debugLoadConfigurationsFailed
            :onDone debugLoadConfigurationsDone)))

(defn debugLoadConfigurationsDone "launch.json, read." [result]
  (debugReadLaunchFile (parseJsonc result.text)))

(defn debugLoadConfigurationsFailed "No launch.json is no configuration; another failure is said." [error]
  (if (= error.code "notFound")
    (debugApplyConfigurations {:configurations [] :skipped 0})
    (set "state.debug.error" (t "debug.cannotRead" {:reason error.message}))))

(defn debugReadLaunchFile "launch.json's text, read as JSON with comments: its configurations, else why not." [launch]
  (if (nil? launch)
    (set "state.debug.error" (t "debug.cannotRead" {:reason (t "tasks.notJson")}))
    (debugReadConfigurations
      (if (and (= (type launch) "object") (= (type launch.configurations) "list")) launch.configurations []))))

(defn debugReadConfigurations
  "The configurations Forge runs (lldb or python, launch or attach), each made plain; the others are skipped."
  [listed]
  (debugMakeConfigurations
    (count listed)
    (filter (fn [it] (and (some? it.adapter) (or (= it.request "launch") (= it.request "attach"))))
            (map (fn [it]
                   {:c it
                    :request it.request
                    :adapter (cond
                               (or (= it.type "lldb") (= it.type "lldb-dap")) "lldb"
                               (or (= it.type "python") (= it.type "debugpy")) "python"
                               :else nil)
                    :port (or-else it.port (if (= (type it.connect) "object") it.connect.port nil))
                    :terminal (or (= it.console "integratedTerminal") (= it.console "externalTerminal"))})
                 (filter (fn [it] (= (type it) "object")) listed)))))

(defn debugMakeConfigurations
  "Each configuration as its adapter takes it, from what it says; one that can't run is left out."
  [total known]
  (debugKeepConfigurations total
                           (filter some? (map (fn [config] (when (debugRunnable? config) (debugConfiguration config)))
                                              known))))

(defn debugRunnable? "Whether a configuration can run: a program to launch, a port or a process to attach to." [config]
  (cond
    (= config.request "launch") (and (= (type config.c.program) "string") (<= (count config.c.program) 4096))
    (= config.adapter "python")
      (and (= (type config.port) "number")
           (= (round config.port) config.port)
           (>= config.port 1)
           (<= config.port 65535))
    :else (and (= (type config.c.pid) "number") (= (round config.c.pid) config.c.pid) (> config.c.pid 0))))

(defn debugConfiguration "A configuration as the debug view lists it: its name, adapter, request and arguments."
  [config]
  {:name (if (and (= (type config.c.name) "string") (not= config.c.name "") (<= (count config.c.name) 200))
           config.c.name
           (str config.adapter " " config.request))
   :type config.adapter
   :request config.request
   :arguments (merge (debugLaunchArguments config.c) (debugConsoleArguments config) (debugAttachArguments config))})

(defn debugLaunchArguments
  "What a configuration gives to launch: its program, cwd, stopOnEntry, args and env, each bounded."
  [c]
  (merge (if (and (= (type c.program) "string") (<= (count c.program) 4096)) {:program c.program} {})
         (if (and (= (type c.cwd) "string") (<= (count c.cwd) 4096)) {:cwd c.cwd} {})
         (if (= (type c.stopOnEntry) "boolean") {:stopOnEntry c.stopOnEntry} {})
         (if (= (type c.args) "list")
           {:args (slice (filter (fn [it] (and (= (type it) "string") (<= (count it) 4096))) c.args) 0 1000)}
           {})
         (if (= (type c.env) "object")
           {:env (fromEntries (slice (filter (fn [it] (and (= (type it.value) "string") (<= (count it.value) 4096)))
                                             (entries c.env))
                                     0
                                     1000))}
           {})))

(defn debugConsoleArguments "Where the program's output goes: debugpy's console, or lldb-dap's terminal." [config]
  (cond
    (not= (type config.c.console) "string") {}
    (= config.adapter "python") {:console (if config.terminal "integratedTerminal" "internalConsole")}
    config.terminal {:runInTerminal true}
    :else {}))

(defn debugAttachArguments "What attaching needs: debugpy's host and port, or lldb-dap's process." [config]
  (cond
    (not= config.request "attach") {}
    (= config.adapter "python") {:connect {:host "127.0.0.1" :port config.port}}
    :else {:pid config.c.pid}))

(defn debugKeepConfigurations "The first 100 configurations; the rest are counted as skipped." [total kept]
  (debugApplyConfigurations {:configurations (slice kept 0 100) :skipped (- total (min (count kept) 100))}))

(defn debugApplyConfigurations "The configurations found, listed; the one chosen stays if it is still there." [found]
  (set "state.debug.configurations" (or-else found.configurations []))
  (set "state.debug.skipped" (or-else found.skipped 0))
  (set "state.debug.error" "")
  (when (>= (number state.debug.selected) (count state.debug.configurations)) (set "state.debug.selected" "0")))

(defn debugApplyLaunchFile "Plays a launch.json given in the event, as the scenarios do." [event]
  (debugReadLaunchFile event))

(defkey "mod+shift+d" showDebug)

;;; A session

(defcommand debugContinueOrStart "Start Debugging or Continue"
  :title ~(t "command.debugContinueOrStart")
  :description ~(t "command.debugContinueOrStart") :category ~(t "category.debug") :icon "play" []
  (cond (= state.debug.status "stopped") (debugContinue) (= state.debug.status "inactive") (debugStart)))

(defcommand debugStart "Start Debugging"
  :title ~(t "command.debugStart") :description ~(t "command.debugStart") :category ~(t "category.debug") :icon "play"
  :enablement ~(= state.debug.status "inactive") []
  (if (nil? computed.debugConfiguration)
    (notify "info" (t "debug.noConfiguration"))
    (do
      (debugBegin computed.debugConfiguration)
      (debugSubscribe)
      (debugRequest "initialize"
                    "debugOnInitialize"
                    {}
                    ""
                    {:clientID "forge"
                     :clientName "Forge"
                     :adapterID (if (= state.debug.type "python") "debugpy" "lldb-dap")
                     :locale "en"
                     :linesStartAt1 true
                     :columnsStartAt1 true
                     :pathFormat "path"
                     :supportsVariableType true
                     :supportsVariablePaging true
                     :supportsRunInTerminalRequest true}))))

(defn debugBegin
  "Starts a session with a configuration: the adapter's events listened to, initialize asked, the console shown."
  [configuration]
  (set "state.debug.type" configuration.type)
  (set "state.debug.status" "starting")
  (set "state.debug.configured" false)
  (set "state.debug.restartAfter" false)
  (set "state.debug.capabilities" {})
  (set "state.debug.terminalSent" false)
  (set "state.debug.threads" [])
  (set "state.debug.threadId" nil)
  (debugResumed)
  (set "state.debug.breakpoints"
       (map (fn [it] (merge it {:verified false :message "" :adapterId nil})) state.debug.breakpoints))
  (debugPrint (t "debug.starting" {:name configuration.name}) "console")
  (set "state.layout.panel" true)
  (set "state.panelTab" "debugConsole"))

(defn debugApplySession
  "Plays a session's start given in the event (its configuration and capabilities), as the scenarios do."
  []
  (set "state.debug.configurations" [event.configuration])
  (set "state.debug.selected" "0")
  (debugBegin event.configuration)
  (set "state.debug.capabilities" (or-else event.capabilities {}))
  (debugApplyFilters (or-else event.capabilities.exceptionBreakpointFilters []))
  (set "state.debug.status" "running"))

(defn debugSubscribe "Listens to the adapter's events." []
  (if (= state.debug.type "python")
    (subscribe "debugPython.events" :id "debug-events" :do debugSubscribeEach :onError debugSubscribeFailed)
    (subscribe "debugLldb.events" :id "debug-events" :do debugSubscribeEach :onError debugSubscribeFailed)))

(defn debugSubscribeEach "An event of the adapter, as it comes." [event]
  (debugEvent event))

(defn debugSubscribeFailed "The adapter's events stopped: the session ends, saying why." [error]
  (debugEnd error.message))

(defn debugOnInitialize
  "The adapter initialized: its capabilities and exception filters kept, then the program launched or attached."
  [body]
  (set "state.debug.capabilities" body)
  (debugApplyFilters (or-else body.exceptionBreakpointFilters []))
  (debugRequest computed.debugConfiguration.request "debugOnLaunch" {} "" computed.debugConfiguration.arguments))

(defn debugApplyFilters "The adapter's exception filters, each on as the person last chose, or as its default."
  [filters]
  (set "state.debug.exceptionFilters"
       (map (fn [it]
              {:filter (text it.filter)
               :label (text (or-else it.label it.filter))
               :enabled (or-else (get state.debug.filterChoices (text it.filter)) it.default false)})
            (slice filters 0 20))))

(defn debugOnLaunch "The program launched: the session runs." []
  (when (= state.debug.status "starting") (set "state.debug.status" "running")))

(defn debugConfigure
  "The adapter is ready: every file's breakpoints and the exception filters sent, then configurationDone."
  []
  (set "state.debug.configured" true)
  (doseq [path (distinct (map (fn [it] it.path) state.debug.breakpoints))]
    (debugSendBreakpoints path))
  (when (> (count state.debug.exceptionFilters) 0) (debugSendFilters))
  (when (or-else state.debug.capabilities.supportsConfigurationDoneRequest false)
    (debugRequest "configurationDone" "" {} "" {})))

(defn debugEnd "The session ends, saying why (`message`); a Restart starts it again." [message]
  (when (not= state.debug.status "inactive")
    (set "state.debug.status" "inactive")
    (cancel "debug-events")
    (debugResumed)
    (set "state.debug.threads" [])
    (set "state.debug.threadId" nil)
    (set "state.debug.breakpoints"
         (map (fn [it] (merge it {:verified false :message "" :adapterId nil})) state.debug.breakpoints))
    (if (not (empty? message))
      (do (debugPrint message "error") (notify "error" message))
      (debugPrint (t "debug.ended") "console"))
    (when state.debug.restartAfter (set "state.debug.restartAfter" false) (debugStart))))

(defkey "f5" debugContinueOrStart)

;;; Requests and their answers

(defn debugRequest
  "A request to the adapter: `answer` and `refused` name the functions its answer and refusal run, with `context`."
  [command answer context refused arguments]
  (if (= state.debug.type "python")
    (debugRequestPython command arguments answer context refused)
    (debugRequestLldb command arguments answer context refused)))

(defn debugRequestPython "A request to the Python adapter (debugpy), each a step that names its method written out."
  [command arguments answer context refused]
  (let [done [debugAnswered answer context] failed [debugRefused command refused context]]
    (cond
      (= command "initialize") (invoke "debugPython.initialize" :with arguments :onDone done :onError failed)
      (= command "launch") (invoke "debugPython.launch" :with arguments :timeout 60000 :onDone done :onError failed)
      (= command "attach") (invoke "debugPython.attach" :with arguments :timeout 60000 :onDone done :onError failed)
      (= command "configurationDone")
        (invoke "debugPython.configurationDone" :with arguments :onDone done :onError failed)
      (= command "setBreakpoints") (invoke "debugPython.setBreakpoints" :with arguments :onDone done :onError failed)
      (= command "setExceptionBreakpoints")
        (invoke "debugPython.setExceptionBreakpoints" :with arguments :onDone done :onError failed)
      (= command "threads") (invoke "debugPython.threads" :with arguments :onDone done :onError failed)
      (= command "stackTrace") (invoke "debugPython.stackTrace" :with arguments :onDone done :onError failed)
      (= command "scopes") (invoke "debugPython.scopes" :with arguments :onDone done :onError failed)
      (= command "variables") (invoke "debugPython.variables" :with arguments :onDone done :onError failed)
      (= command "source") (invoke "debugPython.source" :with arguments :onDone done :onError failed)
      (= command "evaluate") (invoke "debugPython.evaluate" :with arguments :onDone done :onError failed)
      (= command "setVariable") (invoke "debugPython.setVariable" :with arguments :onDone done :onError failed)
      (= command "continue") (invoke "debugPython.continue" :with arguments :onDone done :onError failed)
      (= command "next") (invoke "debugPython.next" :with arguments :onDone done :onError failed)
      (= command "stepIn") (invoke "debugPython.stepIn" :with arguments :onDone done :onError failed)
      (= command "stepOut") (invoke "debugPython.stepOut" :with arguments :onDone done :onError failed)
      (= command "pause") (invoke "debugPython.pause" :with arguments :onDone done :onError failed)
      (= command "restart") (invoke "debugPython.restart" :with arguments :onDone done :onError failed)
      (= command "disconnect") (invoke "debugPython.disconnect" :with arguments :onDone done :onError failed)
      (= command "completions") (invoke "debugPython.completions" :with arguments :onDone done :onError failed)
      (= command "exceptionInfo") (invoke "debugPython.exceptionInfo" :with arguments :onDone done :onError failed))))

(defn debugRequestLldb "A request to lldb-dap, each a step that names its method written out."
  [command arguments answer context refused]
  (let [done [debugAnswered answer context] failed [debugRefused command refused context]]
    (cond
      (= command "initialize") (invoke "debugLldb.initialize" :with arguments :onDone done :onError failed)
      (= command "launch") (invoke "debugLldb.launch" :with arguments :timeout 60000 :onDone done :onError failed)
      (= command "attach") (invoke "debugLldb.attach" :with arguments :timeout 60000 :onDone done :onError failed)
      (= command "configurationDone")
        (invoke "debugLldb.configurationDone" :with arguments :onDone done :onError failed)
      (= command "setBreakpoints") (invoke "debugLldb.setBreakpoints" :with arguments :onDone done :onError failed)
      (= command "setExceptionBreakpoints")
        (invoke "debugLldb.setExceptionBreakpoints" :with arguments :onDone done :onError failed)
      (= command "threads") (invoke "debugLldb.threads" :with arguments :onDone done :onError failed)
      (= command "stackTrace") (invoke "debugLldb.stackTrace" :with arguments :onDone done :onError failed)
      (= command "scopes") (invoke "debugLldb.scopes" :with arguments :onDone done :onError failed)
      (= command "variables") (invoke "debugLldb.variables" :with arguments :onDone done :onError failed)
      (= command "source") (invoke "debugLldb.source" :with arguments :onDone done :onError failed)
      (= command "evaluate") (invoke "debugLldb.evaluate" :with arguments :onDone done :onError failed)
      (= command "setVariable") (invoke "debugLldb.setVariable" :with arguments :onDone done :onError failed)
      (= command "continue") (invoke "debugLldb.continue" :with arguments :onDone done :onError failed)
      (= command "next") (invoke "debugLldb.next" :with arguments :onDone done :onError failed)
      (= command "stepIn") (invoke "debugLldb.stepIn" :with arguments :onDone done :onError failed)
      (= command "stepOut") (invoke "debugLldb.stepOut" :with arguments :onDone done :onError failed)
      (= command "pause") (invoke "debugLldb.pause" :with arguments :onDone done :onError failed)
      (= command "restart") (invoke "debugLldb.restart" :with arguments :onDone done :onError failed)
      (= command "disconnect") (invoke "debugLldb.disconnect" :with arguments :onDone done :onError failed)
      (= command "completions") (invoke "debugLldb.completions" :with arguments :onDone done :onError failed)
      (= command "exceptionInfo") (invoke "debugLldb.exceptionInfo" :with arguments :onDone done :onError failed))))

(defn debugAnswered
  "An adapter's answer: the function it names runs with the answer's `body` and the request's `context`."
  [answer context result]
  (when (not (empty? answer)) (run answer :with {:body (or-else result {}) :context context})))

(defn debugRefused
  "An adapter's refusal: the request's function runs, the session ends if it couldn't start, or the console says why."
  [command refused context error]
  (when (not (= error.code "ended"))
    (cond
      (not (empty? refused)) (run refused :with {:message error.message :context context})
      (or (= command "initialize") (= command "launch") (= command "attach"))
        (debugEnd (t "debug.couldNot" {:command command :error error.message}))
      (= command "disconnect") (debugEnd "")
      :else (debugPrint (str command ": " error.message) "error"))))

(defn debugApplyResponse "Plays an adapter's answer given in the event (answer, body, context), as the scenarios do."
  []
  (run event.answer :with {:body (or-else event.body {}) :context (or-else event.context {})}))

(defn debugApplyRefusal "Plays an adapter's refusal given in the event, as the scenarios do." []
  (debugRefused event.command (or-else event.refused "") (or-else event.context {}) event.error))

(defn debugIgnore "An answer nothing waits for." []
  nil)

;;; The adapter's events

(defn debugEvent
  "An event of the adapter (`message`): initialized, stopped, continued, output, breakpoint, exited or terminated."
  [message]
  (when (= message.event "initialized") (debugConfigure))
  (when (= message.event "stopped") (debugStopped (or-else message.body {})))
  (when (= message.event "continued") (set "state.debug.status" "running") (debugResumed))
  (when (= message.event "output") (debugOutput (or-else message.body {})))
  (when (= message.event "breakpoint")
    (set "state.debug.breakpoints"
         (map (fn [it]
                (if (and (some? it.adapterId) (= it.adapterId message.body.breakpoint.id))
                  (merge it
                         {:verified (or-else message.body.breakpoint.verified it.verified)
                          :message (text (or-else message.body.breakpoint.message ""))})
                  it))
              state.debug.breakpoints)))
  (when (= message.event "exited")
    (debugPrint (t "debug.exited" {:code (or-else message.body.exitCode 0)}) "console"))
  (when (= message.event "terminated") (debugEnd "")))

(defn debugApplyEvent "Plays an adapter's event given in the event, as the scenarios do." [event]
  (debugEvent event))

(defn debugStopped "The program stopped (`body`: reason, threadId, text): the threads and the stack are read." [body]
  (set "state.debug.status" "stopped")
  (set "state.debug.reason" (or-else body.reason "pause"))
  (set "state.debug.threadId" (or-else body.threadId state.debug.threadId (or-else state.debug.threads.0.id 1)))
  (set "state.debug.exception"
       (if (= body.reason "exception") (text (or-else body.text body.description "Exception")) ""))
  (debugRequest "threads" "debugOnThreads" {} "" {})
  (debugLoadStack)
  (when (and (= body.reason "exception") (or-else state.debug.capabilities.supportsExceptionInfoRequest false))
    (debugRequest "exceptionInfo" "debugOnExceptionInfo" {} "debugIgnore" {:threadId state.debug.threadId})))

(defn debugResumed "The program runs again: what was shown of where it stopped goes." []
  (set "state.debug.frames" [])
  (set "state.debug.frameId" nil)
  (set "state.debug.variables" [])
  (set "state.debug.exception" "")
  (set "state.debug.current" {:path "" :line 0 :inside false})
  (set "state.debug.hover" {:path "" :word "" :line 0 :value ""}))

(defn debugOutput "The program's output, line by line in the console; telemetry is left out." [body]
  (when (and (not= body.category "telemetry") (not (empty? body.output)))
    (debugPrintLines (split body.output "\n") (or-else body.category "console"))))

;;; Running, stepping and stopping

(defcommand debugContinue "Continue"
  :title ~(t "command.debugContinue")
  :description ~(t "command.debugContinue") :category ~(t "category.debug") :icon "play"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "continue"))

(defcommand debugStepOver "Step Over"
  :title ~(t "command.debugStepOver")
  :description ~(t "command.debugStepOver") :category ~(t "category.debug") :icon "redo-dot"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "next"))

(defcommand debugStepInto "Step Into"
  :title ~(t "command.debugStepInto")
  :description ~(t "command.debugStepInto") :category ~(t "category.debug") :icon "arrow-down-to-dot"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "stepIn"))

(defcommand debugStepOut "Step Out"
  :title ~(t "command.debugStepOut")
  :description ~(t "command.debugStepOut") :category ~(t "category.debug") :icon "arrow-up-from-dot"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "stepOut"))

(defn debugStep "Steps the program (next, stepIn or stepOut) from where it stopped." [command]
  (when (= state.debug.status "stopped")
    (set "state.debug.status" "running")
    (debugResumed)
    (debugRequest command "" {} "" {:threadId state.debug.threadId})))

(defcommand debugPause "Pause"
  :title ~(t "command.debugPause") :description ~(t "command.debugPause") :category ~(t "category.debug") :icon "pause"
  :enablement ~(= state.debug.status "running") []
  (debugRequest "threads" "debugOnThreadsPause" {} "" {}))

(defn debugOnThreadsPause "Pauses the program, once its threads are known." [body]
  (debugOnThreads body)
  (when (> (count state.debug.threads) 0) (debugRequest "pause" "" {} "" {:threadId state.debug.threads.0.id})))

(defcommand debugStop "Stop"
  :title ~(t "command.debugStop") :description ~(t "command.debugStop") :category ~(t "category.debug") :icon "square"
  :enablement ~(not= state.debug.status "inactive") []
  (when (not= state.debug.status "inactive")
    (debugRequest "disconnect" "debugOnDisconnect" {} "" {:terminateDebuggee true})))

(defn debugOnDisconnect "The adapter let the program go: the session ends." []
  (debugEnd ""))

(defcommand debugRestart "Restart"
  :title ~(t "command.debugRestart")
  :description ~(t "command.debugRestart") :category ~(t "category.debug") :icon "rotate-ccw"
  :enablement ~(not= state.debug.status "inactive") []
  (if (or-else state.debug.capabilities.supportsRestartRequest false)
    (do
      (set "state.debug.status" "running")
      (debugResumed)
      (debugPrint "Restarting" "console")
      (debugRequest "restart" "" {} "" {:arguments computed.debugConfiguration.arguments}))
    (do (set "state.debug.restartAfter" true) (debugStop))))

(defkey "shift+f5" debugStop)

(defkey "mod+shift+f5" debugRestart)

(defkey "f6" debugPause)

(defkey "f10" debugStepOver)

(defkey "f11" debugStepInto)

(defkey "shift+f11" debugStepOut)

;;; The call stack

(defcomputed debugStackRows
  (concat (map (fn [it]
                   {:id (str "t:" it.id)
                    :label (+ it.name
                              (if (and (= it.id state.debug.threadId) (= state.debug.status "stopped"))
                                  (str "  paused on " state.debug.reason)
                                  ""))
                    :icon "list-tree"})
                 (slice state.debug.threads
                        0
                        (+ (indexOf (map (fn [it] it.id) state.debug.threads) state.debug.threadId) 1)))
            (map (fn [it]
                   {:id (str "f:" it.id)
                    :label (str "    "
                                it.name
                                "  "
                                (if (empty? it.path) (t "debug.unknownSource") (get (reverse (split it.path "/")) 0))
                                ":"
                                it.line)
                    :icon (if (= it.id state.debug.frameId) "arrow-right" "")
                    :iconColor (if it.inside "#e5c07b" "#848484")})
                 state.debug.frames)
            (map (fn [it] {:id (str "t:" it.id) :label it.name :icon "list-tree"})
                 (slice state.debug.threads
                        (+ (indexOf (map (fn [it] it.id) state.debug.threads) state.debug.threadId) 1)))))

(defn debugLoadStack "Reads the stopped thread's stack." []
  (debugRequest "stackTrace"
                "debugOnStackTrace"
                {:threadId state.debug.threadId}
                ""
                {:threadId state.debug.threadId :startFrame 0 :levels 50}))

(defn debugOnThreads "The program's threads." [body]
  (set "state.debug.threads"
       (map (fn [it] {:id it.id :name (text (or-else it.name it.id))}) (or-else body.threads []))))

(defn debugOnExceptionInfo "The exception the program stopped on: its name and description." [body]
  (set "state.debug.exception"
       (str (text (or-else body.exceptionId "Exception"))
            (if (empty? body.description) "" (str ": " body.description)))))

(defn debugOnStackTrace "The stopped thread's frames: the first is chosen." [context body]
  (when (and (= context.threadId state.debug.threadId) (= state.debug.status "stopped"))
    (set "state.debug.frames"
         (map (fn [it]
                {:id it.id
                 :name (text (or-else it.name ""))
                 :line (or-else it.line 0)
                 :path (text (or-else it.source.path it.source.name ""))
                 :relative (text (or-else it.source.relative ""))
                 :inside (or-else it.source.inside false)
                 :reference (or-else it.source.sourceReference 0)})
              (or-else body.stackFrames [])))
    (when (> (count state.debug.frames) 0) (debugSelectFrame state.debug.frames.0 false))))

(defn debugSelectStack "A row of the call stack, chosen: a thread's frames are read, a frame shown." [id]
  (cond
    (= (slice id 0 2) "t:")
      (do
        (when (and (= state.debug.status "stopped") (not= (slice id 2) (text state.debug.threadId)))
          (set "state.debug.threadId" (:id (find (fn [it] (= (text it.id) (slice id 2))) state.debug.threads)))
          (debugLoadStack)))
    (some? (find (fn [it] (= (str "f:" it.id) id)) state.debug.frames))
      (debugSelectFrame (find (fn [it] (= (str "f:" it.id) id)) state.debug.frames) true)))

(defn debugSelectFrame
  "Shows a frame: its scopes read, its source opened at its line, or asked about (`ask`) when outside the folder."
  [frame ask]
  (set "state.debug.frameId" frame.id)
  (set "state.debug.selectedStack" (str "f:" frame.id))
  (set "state.debug.current"
       {:path (if frame.inside
                frame.relative
                (str "debug-source:" (if (empty? frame.path) (str "ref:" frame.reference) frame.path)))
        :line frame.line
        :inside frame.inside})
  (set "state.debug.variables" [])
  (debugRequest "scopes" "debugOnScopes" {:frameId frame.id} "" {:frameId frame.id})
  (debugRefreshWatches)
  (cond
    (and frame.inside (not (empty? frame.relative)))
      (do (openPath frame.relative (baseName frame.relative)) (goToLine frame.line))
    (contains computed.docIds state.debug.current.path)
      (showTab state.debug.current.path (:label (find (fn [it] (= it.id state.debug.current.path)) state.docs)))
    :else
      (do
        (when (and ask (or (> frame.reference 0) (not (empty? frame.path))))
          (set "state.debug.outside" {:open true :frame frame})))))

(defcomputed debugMarks
  (concat (map (fn [it]
                   {:path it.path
                    :mark {:line (- it.line 1)
                           :kind "gutter"
                           :text (cond (not it.enabled) "○" (not (empty? it.logMessage)) "◆" :else "●")
                           :color (if (or (not it.enabled)
                                          (and (not= state.debug.status "inactive") (not it.verified)))
                                      "#848484"
                                      "#e51400")}})
                 state.debug.breakpoints)
            (if (and (= state.debug.status "stopped") (> state.debug.current.line 0))
                [{:path state.debug.current.path
                  :mark {:line (- state.debug.current.line 1) :kind "background" :color "#ffcc0040"}}]
                [])
            (if (and (= state.debug.status "stopped")
                     (not (empty? state.debug.exception))
                     (> state.debug.current.line 0))
                [{:path state.debug.current.path
                  :mark {:line (- state.debug.current.line 1)
                         :kind "inline"
                         :color "#f85149"
                         :text state.debug.exception}}]
                [])
            (if (and (= state.debug.status "stopped") (not (empty? state.debug.hover.value)))
                [{:path state.debug.hover.path
                  :mark {:line state.debug.hover.line
                         :kind "inline"
                         :color "#3794ff"
                         :text (str state.debug.hover.word " = " state.debug.hover.value)}}]
                [])))

;;; Variables

(defcomputed debugVariableRows
  (map (fn [it]
           {:id it.path
            :label (+ (pad "" (* it.depth 2))
                      (if (= it.kind "more")
                          (str "  … " it.value)
                          (+ (if (> it.ref 0) (if it.expanded "▾ " "▸ ") "  ")
                             it.name
                             (if (= it.kind "scope") "" (str " = " it.value)))))})
         state.debug.variables))

(defn debugOnScopes "A frame's scopes: the first is expanded unless it is expensive." [context body]
  (when (= context.frameId state.debug.frameId)
    (set "state.debug.variables"
         (map-indexed (fn [index it]
                        {:path (str "s" index)
                         :depth 0
                         :name (text (or-else it.name ""))
                         :value ""
                         :ref (or-else it.variablesReference 0)
                         :indexed (or-else it.indexedVariables 0)
                         :expanded false
                         :kind "scope"
                         :expensive (or-else it.expensive false)})
                      (slice (or-else body.scopes []) 0 20)))
    (when (and (> (count state.debug.variables) 0) (not state.debug.variables.0.expensive)) (debugExpand "s0"))))

(defn debugExpand "Expands or collapses a row of the variables, by its path." [path]
  (debugExpandRow (find (fn [it] (= it.path path)) state.debug.variables)))

(defn debugExpandRow
  "Expands a row (`row`): its variables read; collapses it if it is open; a 'more' row reads the next hundred."
  [row]
  (cond
    (and (some? row) (= row.kind "more"))
      (debugLoadVariables row.parent row.ref row.depth row.start row.indexed row.path)
    (and (some? row) (> row.ref 0) row.expanded)
      (set "state.debug.variables"
           (map (fn [it] (if (= it.path row.path) (merge it {:expanded false}) it))
                (filter (fn [it] (not= (slice it.path 0 (+ (count row.path) 1)) (str row.path "/")))
                        state.debug.variables)))
    :else
      (do
        (when (and (some? row) (> row.ref 0))
          (set "state.debug.variables"
               (map (fn [it] (if (= it.path row.path) (merge it {:expanded true}) it)) state.debug.variables))
          (debugLoadVariables row.path row.ref (+ row.depth 1) 0 row.indexed "")))))

(defn debugLoadVariables "Reads a row's variables, a hundred at a time from `start`."
  [parent ref depth start indexed replace]
  (debugRequest "variables"
                "debugOnVariables"
                {:parent parent
                 :ref ref
                 :depth depth
                 :start start
                 :indexed indexed
                 :replace replace
                 :frameId state.debug.frameId}
                ""
                (merge {:variablesReference ref}
                       (if (> indexed 0) {:filter "indexed" :start start :count (min 100 (- indexed start))} {}))))

(defn debugOnVariables
  "A row's variables, inserted under it (the next hundred replace its 'more' row); the rows are bounded."
  [context body]
  (when (and (= context.frameId state.debug.frameId)
             (>= (indexOf (map (fn [it] it.path) state.debug.variables)
                          (if (empty? context.replace) context.parent context.replace))
                 0))
    (debugInsertRows
      (+ (indexOf (map (fn [it] it.path) state.debug.variables)
                  (if (empty? context.replace) context.parent context.replace))
         (if (empty? context.replace) 1 0))
      (if (empty? context.replace) 0 1)
      (concat (map-indexed (fn [index it]
                             {:path (str context.parent "/" (+ context.start index))
                              :depth context.depth
                              :name (text (or-else it.name ""))
                              :value (text (or-else it.value ""))
                              :type (text (or-else it.type ""))
                              :ref (or-else it.variablesReference 0)
                              :indexed (or-else it.indexedVariables 0)
                              :expanded false
                              :kind "variable"
                              :container context.ref})
                           (slice (or-else body.variables []) 0 (max 0 (- 5000 (count state.debug.variables)))))
              (if (and (> context.indexed 0) (< (+ context.start 100) context.indexed))
                [{:path (str context.parent "/more" (+ context.start 100))
                  :depth context.depth
                  :name ""
                  :value (str (text (- (- context.indexed context.start) 100)) " more")
                  :ref context.ref
                  :indexed context.indexed
                  :expanded false
                  :kind "more"
                  :parent context.parent
                  :start (+ context.start 100)}]
                [])))))

(defn debugInsertRows "Rows inserted in the variables at `at`, replacing `skip` rows there." [at skip rows]
  (set "state.debug.variables"
       (concat (slice state.debug.variables 0 at) rows (slice state.debug.variables (+ at skip)))))

(defcommand debugEditValue "Set Value"
  :title ~(t "command.debugEditValue")
  :description ~(t "command.debugEditValue") :category ~(t "category.debug") :icon "pencil"
  :enablement ~(= state.debug.status "stopped") []
  (debugEditValueOf (find (fn [it] (= it.path state.debug.selectedVariable)) state.debug.variables)))

(defn debugEditValueOf "Asks for a variable's new value, if the adapter can set it." [row]
  (if (and (some? row) (= row.kind "variable") (or-else state.debug.capabilities.supportsSetVariable false))
    (set "state.debug.setValue" {:open true :path row.path :name row.name :value row.value})
    (notify "info" (t "debug.variablesOnly"))))

(defn debugSetValue "Sets the variable's new value." []
  (set "state.debug.setValue.open" false)
  (debugRequest "setVariable"
                "debugOnSetVariable"
                {:path state.debug.setValue.path}
                ""
                {:variablesReference (:container (find (fn [it] (= it.path state.debug.setValue.path))
                                                       state.debug.variables))
                 :name state.debug.setValue.name
                 :value state.debug.setValue.value}))

(defn debugOnSetVariable "The variable's value as the adapter set it." [context body]
  (set "state.debug.variables"
       (map (fn [it]
              (if (= it.path context.path)
                (merge it {:value (text (or-else body.value it.value)) :type (text (or-else body.type it.type))})
                it))
            state.debug.variables))
  (debugRefreshWatches))

(defn debugToggleSection
  "Collapses a section of the view (variables, watch, stack or breakpoints), or expands it again."
  [section]
  (set "state.debug.collapsed"
       (if (contains state.debug.collapsed section)
         (filter (fn [it] (not= it section)) state.debug.collapsed)
         (concat state.debug.collapsed [section]))))

;;; Watches

(defcomputed debugWatchRows
  (map (fn [it]
           {:id it.id
            :label (str it.expression
                        " = "
                        (if (empty? it.error) (if (= state.debug.status "stopped") it.value (t "debug.notAvailable")) it.error))
            :icon "eye"
            :iconColor (if (empty? it.error) "#848484" "#f85149")})
         state.debug.watches))

(defn debugAddWatch "Watches the expression typed (100 at most)." []
  (when (and (not (empty? (trim state.debug.watchDraft))) (< (count state.debug.watches) 100))
    (push "state.debug.watches"
          {:id (str "w" state.debug.watchNext) :expression (trim state.debug.watchDraft) :value "" :error ""})
    (increment "state.debug.watchNext" :by 1)
    (set "state.debug.watchDraft" "")
    (debugRefreshWatches)))

(defn debugRemoveWatch "Stops watching the expression chosen." []
  (set "state.debug.watches" (filter (fn [it] (not= it.id state.debug.selectedWatch)) state.debug.watches)))

(defn debugRefreshWatches "Evaluates every watch in the frame shown." []
  (when (and (= state.debug.status "stopped") (some? state.debug.frameId))
    (doseq [watch state.debug.watches]
      (debugRequest "evaluate"
                    "debugOnWatch"
                    {:id watch.id}
                    "debugOnWatchError"
                    {:expression watch.expression :frameId state.debug.frameId :context "watch"}))))

(defn debugOnWatch "A watch's value." [context body]
  (set "state.debug.watches"
       (map (fn [it] (if (= it.id context.id) (merge it {:value (text (or-else body.result "")) :error ""}) it))
            state.debug.watches)))

(defn debugOnWatchError "Why a watch has no value." [context message]
  (set "state.debug.watches"
       (map (fn [it] (if (= it.id context.id) (merge it {:value "" :error message}) it)) state.debug.watches)))

;;; The debug console

(defcomputed debugConsoleRows
  (map (fn [it]
           {:id it.id
            :label it.text
            :icon (cond (= it.kind "input") "chevron-right"
                        (or (= it.kind "error") (= it.kind "stderr")) "circle-x"
                        :else "")
            :iconColor (if (= it.kind "input") "#3794ff" "#f85149")})
         state.debug.console))

(defn debugPrintLines
  "Lines in the debug console, of a kind (console, stdout, stderr, input, result, error); the last 2000 are kept."
  [lines kind]
  (set "state.debug.console"
       (slice (concat state.debug.console
                      (map-indexed (fn [index it]
                                     {:id (str "c" (+ state.debug.consoleNext index)) :text it :kind kind})
                                   (slice lines 0 (- (count lines) (if (empty? (last lines)) 1 0)))))
              (- 2000)))
  (increment "state.debug.consoleNext" :by (count lines)))

(defn debugPrint "A line in the debug console." [text kind]
  (debugPrintLines [text] kind))

(defn debugEvaluate "Evaluates the console's input in the frame shown, and keeps it in the history." []
  (when (not (empty? (trim state.debug.input)))
    (debugPrint state.debug.input "input")
    (set "state.debug.history"
         (slice (concat (filter (fn [it] (not= it state.debug.input)) state.debug.history) [state.debug.input])
                (- 100)))
    (set "state.debug.historyAt" (count state.debug.history))
    (set "state.debug.completions" [])
    (if (= state.debug.status "inactive")
      (debugPrint (t "debug.noSession") "error")
      (debugRequest "evaluate"
                    "debugOnEvaluate"
                    {}
                    "debugOnEvaluateError"
                    (merge {:expression state.debug.input :context "repl"}
                           (if (nil? state.debug.frameId) {} {:frameId state.debug.frameId}))))
    (set "state.debug.input" "")))

(defn debugOnEvaluate "What the input evaluated to, in the console." [body]
  (debugPrint (text (or-else body.result "")) "result"))

(defn debugOnEvaluateError "Why the input couldn't be evaluated, in the console." [message]
  (debugPrint message "error"))

(defn debugHistoryBack "The console's input before, from the history." []
  (when (> state.debug.historyAt 0)
    (increment "state.debug.historyAt" :by -1)
    (set "state.debug.input" (get state.debug.history state.debug.historyAt))))

(defn debugHistoryForward "The console's input after, from the history." []
  (when (< state.debug.historyAt (count state.debug.history)) (increment "state.debug.historyAt" :by 1))
  (set "state.debug.input" (or-else (get state.debug.history state.debug.historyAt) "")))

(defn debugComplete "Asks the adapter to complete the console's input." []
  (when (and (not= state.debug.status "inactive") (or-else state.debug.capabilities.supportsCompletionsRequest false))
    (debugRequest "completions"
                  "debugOnCompletions"
                  {:input state.debug.input}
                  "debugIgnore"
                  (merge {:text state.debug.input :column (+ (count state.debug.input) 1)}
                         (if (nil? state.debug.frameId) {} {:frameId state.debug.frameId})))))

(defn debugOnCompletions "The completions of the input: one is inserted, several listed." [context body]
  (when (= context.input state.debug.input)
    (set "state.debug.completions"
         (slice (map (fn [it] (text (or-else it.text it.label))) (or-else body.targets [])) 0 50))
    (when (= (count state.debug.completions) 1)
      (set "state.debug.input"
           (str (slice state.debug.input
                       0
                       (- (count state.debug.input) (count (get (reverse (split state.debug.input " ")) 0))))
                state.debug.completions.0))
      (set "state.debug.completions" []))))

(defcommand debugClearConsole "Clear Console"
  :title ~(t "command.debugClearConsole")
  :description ~(t "command.debugClearConsole") :category ~(t "category.debug") :icon "eraser" []
  (set "state.debug.console" []))

(defcommand showDebugConsole "Show Debug Console"
  :title ~(t "command.showDebugConsole")
  :description ~(t "command.showDebugConsole") :category ~(t "category.view") :icon "terminal" []
  (set "state.layout.panel" true)
  (set "state.panelTab" "debugConsole"))

(defn debugHover "A word hovered while stopped: its value asked for." [path]
  (cond
    (or (not= state.debug.status "stopped") (empty? event.word) (nil? event.line))
      (set "state.debug.hover" {:path "" :word "" :line 0 :value ""})
    (or (not= event.word state.debug.hover.word) (not= event.line state.debug.hover.line))
      (do
        (set "state.debug.hover" {:path path :word event.word :line event.line :value ""})
        (debugRequest "evaluate"
                      "debugOnHover"
                      {:word event.word :line event.line}
                      "debugIgnore"
                      {:expression event.word :frameId state.debug.frameId :context "hover"}))))

(defn debugOnHover "A hovered word's value." [context body]
  (when (and (= context.word state.debug.hover.word) (= context.line state.debug.hover.line))
    (set "state.debug.hover.value" (slice (text (or-else body.result "")) 0 200))))

;;; Breakpoints

(defcomputed debugBreakpointRows
  (map (fn [it]
           {:id it.id
            :label (str (get (reverse (split it.path "/")) 0)
                        ":"
                        it.line
                        (if (empty? it.condition) "" (str "  if " it.condition))
                        (if (empty? it.hitCondition) "" (str "  hits " it.hitCondition))
                        (if (empty? it.logMessage) "" (str "  log " it.logMessage))
                        (if (and (not= state.debug.status "inactive") (not it.verified) (not (empty? it.message)))
                            (str "  (" it.message ")")
                            ""))
            :icon (cond (not it.enabled) "circle-off" (not (empty? it.logMessage)) "diamond" :else "circle")
            :iconColor (if (or (not it.enabled) (and (not= state.debug.status "inactive") (not it.verified)))
                           "#848484"
                           "#e51400")})
         state.debug.breakpoints))

(defn debugToggleBreakpoint "Adds a breakpoint at a file's line, or removes the one there." [path line]
  (if (some? (find (fn [it] (and (= it.path path) (= it.line line))) state.debug.breakpoints))
    (set "state.debug.breakpoints"
         (filter (fn [it] (not (and (= it.path path) (= it.line line)))) state.debug.breakpoints))
    (debugAddBreakpoint path line))
  (debugSendBreakpoints path))

(defn debugAddBreakpoint "Adds a breakpoint at a file's line (10000 at most)." [path line]
  (when (< (count state.debug.breakpoints) 10000)
    (push "state.debug.breakpoints"
          {:id (str "b" state.debug.breakpointNext)
           :path path
           :line line
           :enabled true
           :condition ""
           :hitCondition ""
           :logMessage ""
           :verified false
           :message ""
           :adapterId nil})
    (increment "state.debug.breakpointNext" :by 1)))

(defcommand debugToggleBreakpointAtCaret "Toggle Breakpoint"
  :title ~(t "command.debugToggleBreakpointAtCaret")
  :description ~(t "command.debugToggleBreakpointAtCaret") :category ~(t "category.debug") :icon "circle-dot"
  :enablement ~(and (some? computed.activeDoc) (= computed.activeDoc.kind "file")) []
  (when (and (some? computed.activeDoc) (= computed.activeDoc.kind "file"))
    (debugToggleBreakpoint computed.activeDoc.id state.caret.line)))

(defn debugGutterClick
  "A click in an editor's gutter: a breakpoint toggled; with Shift, edited; with Alt, enabled or disabled."
  [path]
  (cond
    event.shift (debugEditBreakpointAt path (+ event.line 1))
    event.alt
      (do
        (set "state.debug.breakpoints"
             (map (fn [it]
                    (if (and (= it.path path) (= it.line (+ event.line 1))) (merge it {:enabled (not it.enabled)}) it))
                  state.debug.breakpoints))
        (debugSendBreakpoints path))
    :else (debugToggleBreakpoint path (+ event.line 1))))

(defn debugEditBreakpointAt "Edits the breakpoint at a line, adding one first." [path line]
  (when (nil? (find (fn [it] (and (= it.path path) (= it.line line))) state.debug.breakpoints))
    (debugAddBreakpoint path line))
  (debugEditBreakpoint
    (or-else (:id (find (fn [it] (and (= it.path path) (= it.line line))) state.debug.breakpoints)) "")))

(defn debugEditBreakpoint "Opens a breakpoint's condition, hit count and log message." [id]
  (when (some? (find (fn [it] (= it.id id)) state.debug.breakpoints))
    (set "state.debug.edit"
         (merge {:open true :id id} (or-else (find (fn [it] (= it.id id)) state.debug.breakpoints) {})))))

(defcommand debugEditSelectedBreakpoint "Edit Breakpoint"
  :title ~(t "command.debugEditSelectedBreakpoint")
  :description ~(t "command.debugEditSelectedBreakpoint") :category ~(t "category.debug") :icon "pencil"
  :enablement ~(some? state.debug.selectedBreakpoint) []
  (debugEditBreakpoint (or-else state.debug.selectedBreakpoint "")))

(defn debugSaveBreakpoint
  "Keeps the breakpoint's condition, hit count and log message, and sends its file's breakpoints."
  []
  (set "state.debug.edit.open" false)
  (set "state.debug.breakpoints"
       (map (fn [it]
              (if (= it.id state.debug.edit.id)
                (merge it
                       {:condition (slice (trim state.debug.edit.condition) 0 1000)
                        :hitCondition (slice (trim state.debug.edit.hitCondition) 0 1000)
                        :logMessage (slice state.debug.edit.logMessage 0 1000)})
                it))
            state.debug.breakpoints))
  (debugSendBreakpoints (or-else (:path (find (fn [it] (= it.id state.debug.edit.id)) state.debug.breakpoints)) "")))

(defn debugApplyBreakpoint "Plays a breakpoint given in the event, as the scenarios do." []
  (debugToggleBreakpoint event.path event.line)
  (set "state.debug.breakpoints"
       (map (fn [it]
              (if (and (= it.path event.path) (= it.line event.line))
                (merge it
                       {:condition (or-else event.condition "")
                        :hitCondition (or-else event.hitCondition "")
                        :logMessage (or-else event.logMessage "")})
                it))
            state.debug.breakpoints)))

(defn debugToggleEnabled "Enables or disables a breakpoint." [id]
  (set "state.debug.breakpoints"
       (map (fn [it] (if (= it.id id) (merge it {:enabled (not it.enabled)}) it)) state.debug.breakpoints))
  (debugSendBreakpoints (or-else (:path (find (fn [it] (= it.id id)) state.debug.breakpoints)) "")))

(defn debugToggleSelectedEnabled "Enables or disables the breakpoint chosen." []
  (when (some? state.debug.selectedBreakpoint) (debugToggleEnabled state.debug.selectedBreakpoint)))

(defn debugOpenBreakpoint "A row of the breakpoints, opened: its file at its line." []
  (when (some? (find (fn [it] (= it.id event.id)) state.debug.breakpoints))
    (debugGoTo (find (fn [it] (= it.id event.id)) state.debug.breakpoints))))

(defn debugGoTo "Opens a breakpoint's file at its line." [breakpoint]
  (openPath breakpoint.path (baseName breakpoint.path))
  (goToLine breakpoint.line))

(defn debugRemoveBreakpoint "Removes the breakpoint chosen." []
  (when (some? state.debug.selectedBreakpoint)
    (debugRemoveBreakpointOf (find (fn [it] (= it.id state.debug.selectedBreakpoint)) state.debug.breakpoints))))

(defn debugRemoveBreakpointOf "Removes a breakpoint, and sends its file's breakpoints." [breakpoint]
  (when (some? breakpoint)
    (set "state.debug.breakpoints" (filter (fn [it] (not= it.id breakpoint.id)) state.debug.breakpoints))
    (set "state.debug.selectedBreakpoint" nil)
    (debugSendBreakpoints breakpoint.path)))

(defn distinct "A list's items, each once, in the order they first come." [items]
  (reduce (fn [seen x] (if (some (fn [y] (= y x)) seen) seen (conj seen x))) [] items))

(defcommand debugRemoveAllBreakpoints "Remove All Breakpoints"
  :title ~(t "command.debugRemoveAllBreakpoints")
  :description ~(t "command.debugRemoveAllBreakpoints") :category ~(t "category.debug") :icon "list-x" []
  (doseq [path (distinct (map (fn [it] it.path) state.debug.breakpoints))]
    (set "state.debug.breakpoints" (filter (fn [it] (not= it.path path)) state.debug.breakpoints))
    (debugSendBreakpoints path))
  (set "state.debug.selectedBreakpoint" nil))

(defn debugSendBreakpoints "A file's breakpoints, sent to the adapter once the session is configured." [path]
  (when (and (not= state.debug.status "inactive") state.debug.configured)
    (debugRequest "setBreakpoints"
                  "debugOnSetBreakpoints"
                  {:path path
                   :ids (map (fn [it] it.id)
                             (filter (fn [it] (and (= it.path path) it.enabled)) state.debug.breakpoints))}
                  ""
                  {:source {:path path}
                   :breakpoints (map (fn [it]
                                       (merge {:line it.line}
                                              (if (empty? it.condition) {} {:condition it.condition})
                                              (if (empty? it.hitCondition) {} {:hitCondition it.hitCondition})
                                              (if (empty? it.logMessage) {} {:logMessage it.logMessage})))
                                     (filter (fn [it] (and (= it.path path) it.enabled)) state.debug.breakpoints))
                   :sourceModified false})))

(defn debugOnSetBreakpoints
  "The adapter's answer for a file's breakpoints: each verified or not, at the line it moved to."
  [context body]
  (set "state.debug.breakpoints"
       (map (fn [it]
              (if (contains context.ids it.id)
                (merge it
                       {:verified (or-else (get-in body.breakpoints [(indexOf context.ids it.id) :verified]) false)
                        :message (text (or-else (get-in body.breakpoints [(indexOf context.ids it.id) :message]) ""))
                        :adapterId (or-else (get-in body.breakpoints [(indexOf context.ids it.id) :id]) nil)})
                it))
            state.debug.breakpoints)))

(defn debugSendFilters "The exception filters that are on, sent to the adapter." []
  (when (and (not= state.debug.status "inactive") state.debug.configured)
    (debugRequest "setExceptionBreakpoints"
                  ""
                  {}
                  ""
                  {:filters (map (fn [it] it.filter) (filter (fn [it] it.enabled) state.debug.exceptionFilters))})))

(defn debugToggleFilter "Turns an exception filter on or off, remembered for the next session." [filter]
  (set "state.debug.exceptionFilters"
       (map (fn [it] (if (= it.filter filter) (merge it {:enabled (not it.enabled)}) it))
            state.debug.exceptionFilters))
  (set "state.debug.filterChoices"
       (assoc state.debug.filterChoices
              filter
              (:enabled (find (fn [it] (= it.filter filter)) state.debug.exceptionFilters))))
  (debugSendFilters))

(defkey "f9" debugToggleBreakpointAtCaret)

;;; Sources outside the folder

(defn debugOpenOutside "Asks the adapter for the text of the frame outside the folder." []
  (when (some? state.debug.outside.frame)
    (debugRequest "source"
                  "debugOnSource"
                  {:frame state.debug.outside.frame}
                  "debugOnSourceRefused"
                  {:source (merge (if (empty? state.debug.outside.frame.path)
                                    {}
                                    {:path state.debug.outside.frame.path})
                                  {:sourceReference state.debug.outside.frame.reference})
                   :sourceReference state.debug.outside.frame.reference})))

(defn debugOnSource "A source's text, from the adapter, shown read-only in a tab." [context body]
  (debugShowSource
    (str "debug-source:" (if (empty? context.frame.path) (str "ref:" context.frame.reference) context.frame.path))
    (t "debug.readOnly" {:name (if (empty? context.frame.path) context.frame.name (baseName context.frame.path))})
    (slice (text (or-else body.content "")) 0 4194304)))

(defn debugOnSourceRefused "Says why the adapter has no source for the frame." [message]
  (notify "info" (t "debug.noSource" {:error message})))

(defn debugShowSource "Shows a debugged source's text in a tab of its own, read-only." [id label content]
  (set "state.docs" (filter (fn [it] (not= it.id id)) state.docs))
  (push "state.docs" {:id id :label label :kind "debugSource" :text content :dirty false :language ""})
  (showTab id label))

(defn debugApplySource "Plays a source given in the event (path, label, content), as the scenarios do." []
  (debugShowSource (str "debug-source:" event.path) event.label event.content))

;;; The program's terminal

(defn debugLldbRunInTerminal "lldb-dap asks to run the program on a terminal." []
  (debugRunInTerminal))

(defn debugPythonRunInTerminal "debugpy asks to run the program on a terminal." []
  (debugRunInTerminal))

(defn debugRunInTerminal
  "The adapter's runInTerminal: its program runs, quoted by shellLine(), on the debug terminal's POSIX shell sh."
  []
  (debugRunLine (and (= (type event.args) "list") (not (empty? event.args)))
                (if (= (type event.args) "list")
                  (shellLine event.args (if (= (type event.env) "object") event.env nil))
                  nil)
                (if (= (type event.cwd) "string") event.cwd "")))

(defn debugRunLine "Runs a command line on the debug terminal, or answers the adapter why not." [named line folder]
  (cond
    (not named) (respond nil :error "the adapter named no program to run")
    (nil? line) (respond nil :error "the adapter's command line is not plain text, or is too long")
    :else
      (do
        (set "state.debug.terminalSent" false)
        (invoke "debugTerminal.open"
                :id "debug-terminal"
                :timeout 600000
                :with {:cols 120 :rows 30 :folder folder}
                :onEvent [debugRunLineEvent line]
                :onError debugRunLineFailed))))

(defn debugRunLineEvent "The debug terminal opened: the line is typed once, and the adapter answered." [line]
  (when (and (some? event.session) (not state.debug.terminalSent))
    (set "state.debug.terminalSent" true)
    (invoke "debugTerminal.write"
            :with {:session event.session :data (str "exec " line "\n")}
            :onError debugRunLineEventFailed
            :onDone debugRunLineEventDone))
  (when (some? event.data) (debugOutput {:category "stdout" :output event.data})))

(defn debugRunLineEventDone "The line was typed: the adapter hears it ran." []
  (respond {}))

(defn debugRunLineEventFailed "The line couldn't be typed: the adapter hears why." [error]
  (respond nil :error error.message))

(defn debugRunLineFailed "The debug terminal didn't open: the adapter hears why." [error]
  (when (not state.debug.terminalSent) (respond nil :error error.message)))

(defpart debug-events "What the adapters' requests run: each a function."
  :actions {"debugLldb:runInTerminal" ~(handler debugLldbRunInTerminal)
            "debugPython:runInTerminal" ~(handler debugPythonRunInTerminal)})
