;;; debug.lisp --- Forge's debugger: launch configurations read from .forge/launch.json through the files service (comments allowed: parseJsonc), the subset Forge runs made in debugReadConfigurations, a session with lldb-dap or debugpy through the dap service the host granted (Debug.cpp), breakpoints in the editor's gutter, the call stack, variables, watches and the debug console
;;;
;;; Every request goes through debugRequest, which names the service by state.debug.type and runs the answer action
;;; with `body` and `context`; the adapter's events go to debugEvent. The actions that take the adapter's messages as
;;; events (debugApplySession, debugApplyEvent, debugApplyResponse) let a scenario or an agent drive the views without
;;; an adapter. What an adapter sends is bounded in the dap service; here the console keeps 2000 lines, the history
;;; 100 entries and the variables 5000 rows. The views are in debugViews.json.

;;; State and definitions

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

(defcomputed debugConfigurationRows
  (map-indexed (fn [index it] {:id (text index) :label it.name :icon (if (= it.type "python") "file-code" "cpu")})
                 state.debug.configurations))

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
                                (if (empty? it.path) "unknown source" (get (reverse (split it.path "/")) 0))
                                ":"
                                it.line)
                    :icon (if (= it.id state.debug.frameId) "arrow-right" "")
                    :iconColor (if it.inside "#e5c07b" "#848484")})
                 state.debug.frames)
            (map (fn [it] {:id (str "t:" it.id) :label it.name :icon "list-tree"})
                 (slice state.debug.threads
                        (+ (indexOf (map (fn [it] it.id) state.debug.threads) state.debug.threadId) 1)))))

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

(defcomputed debugWatchRows
  (map (fn [it]
           {:id it.id
            :label (str it.expression
                        " = "
                        (if (empty? it.error) (if (= state.debug.status "stopped") it.value "not available") it.error))
            :icon "eye"
            :iconColor (if (empty? it.error) "#848484" "#f85149")})
         state.debug.watches))

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

(defcomputed debugConsoleRows
  (map (fn [it]
           {:id it.id
            :label it.text
            :icon (cond (= it.kind "input") "chevron-right"
                        (or (= it.kind "error") (= it.kind "stderr")) "circle-x"
                        :else "")
            :iconColor (if (= it.kind "input") "#3794ff" "#f85149")})
         state.debug.console))

(defkey "f5" debugContinueOrStart)

(defkey "shift+f5" debugStop)

(defkey "mod+shift+f5" debugRestart)

(defkey "f6" debugPause)

(defkey "f9" debugToggleBreakpointAtCaret)

(defkey "f10" debugStepOver)

(defkey "f11" debugStepInto)

(defkey "shift+f11" debugStepOut)

(defkey "mod+shift+d" showDebug)

(defpart debug
         "What debug adds."
         :services {:debugLldb {:provider dap :process lldb-dap :root workspace :restart {:after 0 :times 100}}
                    :debugPython {:provider dap
                                  :process debugpy-adapter
                                  :root workspace
                                  :restart {:after 0 :times 100}}
                    :debugTerminal {:provider pty :process sh :root workspace}})

;;; Functions and commands

(defcommand showDebug "Show Debugger"
  :title ~(t "command.showDebug") :category ~(t "category.view") :icon "bug" []
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

(defn debugReadLaunchFile "TODO" [launch]
  (if (nil? launch)
    (set "state.debug.error" "Cannot read .forge/launch.json: it isn't JSON")
    (debugReadConfigurations
      (if (and (= (type launch) "object") (= (type launch.configurations) "list")) launch.configurations []))))

(defn debugReadConfigurations "The configurations Forge runs: type lldb (or lldb-dap) or python (or debugpy), request launch (with a program) or attach (debugpy to a local port, lldb to a process id); of the rest only program, cwd, stopOnEntry, args, env and console are passed on, each checked and bounded. The first 100 are kept, the others counted as skipped."
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

(defn debugMakeConfigurations "TODO" [total known]
  (debugKeepConfigurations
    total
    (filter (fn [it] (some? it))
            (map (fn [it]
                   (if (cond
                         (= it.request "launch") (and (= (type it.c.program) "string") (<= (count it.c.program) 4096))
                         (= it.adapter "python")
                           (and (= (type it.port) "number")
                                (= (round it.port) it.port)
                                (>= it.port 1)
                                (<= it.port 65535))
                         :else (and (= (type it.c.pid) "number") (= (round it.c.pid) it.c.pid) (> it.c.pid 0)))
                     {:name (if (and (= (type it.c.name) "string") (not= it.c.name "") (<= (count it.c.name) 200))
                              it.c.name
                              (str it.adapter " " it.request))
                      :type it.adapter
                      :request it.request
                      :arguments (merge (if (and (= (type it.c.program) "string") (<= (count it.c.program) 4096))
                                          {:program it.c.program}
                                          {})
                                        (if (and (= (type it.c.cwd) "string") (<= (count it.c.cwd) 4096))
                                          {:cwd it.c.cwd}
                                          {})
                                        (if (= (type it.c.stopOnEntry) "boolean") {:stopOnEntry it.c.stopOnEntry} {})
                                        (if (= (type it.c.args) "list")
                                          {:args (slice (filter (fn [it]
                                                                  (and (= (type it) "string") (<= (count it) 4096)))
                                                                it.c.args)
                                                        0
                                                        1000)}
                                          {})
                                        (if (= (type it.c.env) "object")
                                          {:env (fromEntries (slice (filter (fn [it]
                                                                              (and (= (type it.value) "string")
                                                                                   (<= (count it.value) 4096)))
                                                                            (entries it.c.env))
                                                                    0
                                                                    1000))}
                                          {})
                                        (cond
                                          (not= (type it.c.console) "string") {}
                                          (= it.adapter "python")
                                            {:console (if it.terminal "integratedTerminal" "internalConsole")}
                                          it.terminal {:runInTerminal true}
                                          :else {})
                                        (cond
                                          (not= it.request "attach") {}
                                          (= it.adapter "python") {:connect {:host "127.0.0.1" :port it.port}}
                                          :else {:pid it.c.pid}))}
                     nil))
                 known))))

(defn debugKeepConfigurations "TODO" [total kept]
  (debugApplyConfigurations {:configurations (slice kept 0 100) :skipped (- total (min (count kept) 100))}))

(defn debugApplyConfigurations "TODO" [found]
  (set "state.debug.configurations" (or-else found.configurations []))
  (set "state.debug.skipped" (or-else found.skipped 0))
  (set "state.debug.error" "")
  (when (>= (number state.debug.selected) (count state.debug.configurations)) (set "state.debug.selected" "0")))

(defn debugApplyLaunchFile "TODO" [event]
  (debugReadLaunchFile event))

(defcommand debugContinueOrStart "Start Debugging or Continue"
  :title ~(t "command.debugContinueOrStart") :category ~(t "category.debug") :icon "play" []
  (if (= state.debug.status "stopped") (debugContinue) (when (= state.debug.status "inactive") (debugStart))))

(defcommand debugStart "Start Debugging"
  :title ~(t "command.debugStart") :category ~(t "category.debug") :icon "play"
  :enablement ~(= state.debug.status "inactive") []
  (if (nil? computed.debugConfiguration)
    (notify "info"
            "No launch configuration: add one of type lldb or python to the open folder's .forge/launch.json, then reload.")
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

(defn debugBegin "TODO" [configuration]
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
  (debugPrint (str "Starting " configuration.name) "console")
  (set "state.layout.panel" true)
  (set "state.panelTab" "debugConsole"))

(defn debugApplySession "TODO" []
  (set "state.debug.configurations" [event.configuration])
  (set "state.debug.selected" "0")
  (debugBegin event.configuration)
  (set "state.debug.capabilities" (or-else event.capabilities {}))
  (debugApplyFilters (or-else event.capabilities.exceptionBreakpointFilters []))
  (set "state.debug.status" "running"))

(defn debugSubscribe "TODO" []
  (if (= state.debug.type "python")
    (subscribe "debugPython.events" :id "debug-events" :do debugSubscribeEach :onError debugSubscribeFailed)
    (subscribe "debugLldb.events" :id "debug-events" :do debugSubscribeEach2 :onError debugSubscribeFailed2)))

(defn debugRequest "TODO" [command answer context refused arguments]
  (if (= state.debug.type "python")
    (debugRequestPython command arguments answer context refused)
    (debugRequestLldb command arguments answer context refused)))

(defn debugRequestPython "TODO" [command arguments answer context refused]
  (when (= command "initialize")
    (invoke "debugPython.initialize"
            :with arguments
            :onDone [debugRequestPythonDone answer context]
            :onError [debugRequestPythonFailed refused context command]))
  (when (= command "launch")
    (invoke "debugPython.launch"
            :with arguments
            :timeout 60000
            :onDone [debugRequestPythonDone2 answer context]
            :onError [debugRequestPythonFailed2 refused context command]))
  (when (= command "attach")
    (invoke "debugPython.attach"
            :with arguments
            :timeout 60000
            :onDone [debugRequestPythonDone3 answer context]
            :onError [debugRequestPythonFailed3 refused context command]))
  (when (= command "configurationDone")
    (invoke "debugPython.configurationDone"
            :with arguments
            :onDone [debugRequestPythonDone4 answer context]
            :onError [debugRequestPythonFailed4 refused context command]))
  (when (= command "setBreakpoints")
    (invoke "debugPython.setBreakpoints"
            :with arguments
            :onDone [debugRequestPythonDone5 answer context]
            :onError [debugRequestPythonFailed5 refused context command]))
  (when (= command "setExceptionBreakpoints")
    (invoke "debugPython.setExceptionBreakpoints"
            :with arguments
            :onDone [debugRequestPythonDone6 answer context]
            :onError [debugRequestPythonFailed6 refused context command]))
  (when (= command "threads")
    (invoke "debugPython.threads"
            :with arguments
            :onDone [debugRequestPythonDone7 answer context]
            :onError [debugRequestPythonFailed7 refused context command]))
  (when (= command "stackTrace")
    (invoke "debugPython.stackTrace"
            :with arguments
            :onDone [debugRequestPythonDone8 answer context]
            :onError [debugRequestPythonFailed8 refused context command]))
  (when (= command "scopes")
    (invoke "debugPython.scopes"
            :with arguments
            :onDone [debugRequestPythonDone9 answer context]
            :onError [debugRequestPythonFailed9 refused context command]))
  (when (= command "variables")
    (invoke "debugPython.variables"
            :with arguments
            :onDone [debugRequestPythonDone10 answer context]
            :onError [debugRequestPythonFailed10 refused context command]))
  (when (= command "source")
    (invoke "debugPython.source"
            :with arguments
            :onDone [debugRequestPythonDone11 answer context]
            :onError [debugRequestPythonFailed11 refused context command]))
  (when (= command "evaluate")
    (invoke "debugPython.evaluate"
            :with arguments
            :onDone [debugRequestPythonDone12 answer context]
            :onError [debugRequestPythonFailed12 refused context command]))
  (when (= command "setVariable")
    (invoke "debugPython.setVariable"
            :with arguments
            :onDone [debugRequestPythonDone13 answer context]
            :onError [debugRequestPythonFailed13 refused context command]))
  (when (= command "continue")
    (invoke "debugPython.continue"
            :with arguments
            :onDone [debugRequestPythonDone14 answer context]
            :onError [debugRequestPythonFailed14 refused context command]))
  (when (= command "next")
    (invoke "debugPython.next"
            :with arguments
            :onDone [debugRequestPythonDone15 answer context]
            :onError [debugRequestPythonFailed15 refused context command]))
  (when (= command "stepIn")
    (invoke "debugPython.stepIn"
            :with arguments
            :onDone [debugRequestPythonDone16 answer context]
            :onError [debugRequestPythonFailed16 refused context command]))
  (when (= command "stepOut")
    (invoke "debugPython.stepOut"
            :with arguments
            :onDone [debugRequestPythonDone17 answer context]
            :onError [debugRequestPythonFailed17 refused context command]))
  (when (= command "pause")
    (invoke "debugPython.pause"
            :with arguments
            :onDone [debugRequestPythonDone18 answer context]
            :onError [debugRequestPythonFailed18 refused context command]))
  (when (= command "restart")
    (invoke "debugPython.restart"
            :with arguments
            :onDone [debugRequestPythonDone19 answer context]
            :onError [debugRequestPythonFailed19 refused context command]))
  (when (= command "disconnect")
    (invoke "debugPython.disconnect"
            :with arguments
            :onDone [debugRequestPythonDone20 answer context]
            :onError [debugRequestPythonFailed20 refused context command]))
  (when (= command "completions")
    (invoke "debugPython.completions"
            :with arguments
            :onDone [debugRequestPythonDone21 answer context]
            :onError [debugRequestPythonFailed21 refused context command]))
  (when (= command "exceptionInfo")
    (invoke "debugPython.exceptionInfo"
            :with arguments
            :onDone [debugRequestPythonDone22 answer context]
            :onError [debugRequestPythonFailed22 refused context command])))

(defn debugRequestLldb "TODO" [command arguments answer context refused]
  (when (= command "initialize")
    (invoke "debugLldb.initialize"
            :with arguments
            :onDone [debugRequestLldbDone answer context]
            :onError [debugRequestLldbFailed refused context command]))
  (when (= command "launch")
    (invoke "debugLldb.launch"
            :with arguments
            :timeout 60000
            :onDone [debugRequestLldbDone2 answer context]
            :onError [debugRequestLldbFailed2 refused context command]))
  (when (= command "attach")
    (invoke "debugLldb.attach"
            :with arguments
            :timeout 60000
            :onDone [debugRequestLldbDone3 answer context]
            :onError [debugRequestLldbFailed3 refused context command]))
  (when (= command "configurationDone")
    (invoke "debugLldb.configurationDone"
            :with arguments
            :onDone [debugRequestLldbDone4 answer context]
            :onError [debugRequestLldbFailed4 refused context command]))
  (when (= command "setBreakpoints")
    (invoke "debugLldb.setBreakpoints"
            :with arguments
            :onDone [debugRequestLldbDone5 answer context]
            :onError [debugRequestLldbFailed5 refused context command]))
  (when (= command "setExceptionBreakpoints")
    (invoke "debugLldb.setExceptionBreakpoints"
            :with arguments
            :onDone [debugRequestLldbDone6 answer context]
            :onError [debugRequestLldbFailed6 refused context command]))
  (when (= command "threads")
    (invoke "debugLldb.threads"
            :with arguments
            :onDone [debugRequestLldbDone7 answer context]
            :onError [debugRequestLldbFailed7 refused context command]))
  (when (= command "stackTrace")
    (invoke "debugLldb.stackTrace"
            :with arguments
            :onDone [debugRequestLldbDone8 answer context]
            :onError [debugRequestLldbFailed8 refused context command]))
  (when (= command "scopes")
    (invoke "debugLldb.scopes"
            :with arguments
            :onDone [debugRequestLldbDone9 answer context]
            :onError [debugRequestLldbFailed9 refused context command]))
  (when (= command "variables")
    (invoke "debugLldb.variables"
            :with arguments
            :onDone [debugRequestLldbDone10 answer context]
            :onError [debugRequestLldbFailed10 refused context command]))
  (when (= command "source")
    (invoke "debugLldb.source"
            :with arguments
            :onDone [debugRequestLldbDone11 answer context]
            :onError [debugRequestLldbFailed11 refused context command]))
  (when (= command "evaluate")
    (invoke "debugLldb.evaluate"
            :with arguments
            :onDone [debugRequestLldbDone12 answer context]
            :onError [debugRequestLldbFailed12 refused context command]))
  (when (= command "setVariable")
    (invoke "debugLldb.setVariable"
            :with arguments
            :onDone [debugRequestLldbDone13 answer context]
            :onError [debugRequestLldbFailed13 refused context command]))
  (when (= command "continue")
    (invoke "debugLldb.continue"
            :with arguments
            :onDone [debugRequestLldbDone14 answer context]
            :onError [debugRequestLldbFailed14 refused context command]))
  (when (= command "next")
    (invoke "debugLldb.next"
            :with arguments
            :onDone [debugRequestLldbDone15 answer context]
            :onError [debugRequestLldbFailed15 refused context command]))
  (when (= command "stepIn")
    (invoke "debugLldb.stepIn"
            :with arguments
            :onDone [debugRequestLldbDone16 answer context]
            :onError [debugRequestLldbFailed16 refused context command]))
  (when (= command "stepOut")
    (invoke "debugLldb.stepOut"
            :with arguments
            :onDone [debugRequestLldbDone17 answer context]
            :onError [debugRequestLldbFailed17 refused context command]))
  (when (= command "pause")
    (invoke "debugLldb.pause"
            :with arguments
            :onDone [debugRequestLldbDone18 answer context]
            :onError [debugRequestLldbFailed18 refused context command]))
  (when (= command "restart")
    (invoke "debugLldb.restart"
            :with arguments
            :onDone [debugRequestLldbDone19 answer context]
            :onError [debugRequestLldbFailed19 refused context command]))
  (when (= command "disconnect")
    (invoke "debugLldb.disconnect"
            :with arguments
            :onDone [debugRequestLldbDone20 answer context]
            :onError [debugRequestLldbFailed20 refused context command]))
  (when (= command "completions")
    (invoke "debugLldb.completions"
            :with arguments
            :onDone [debugRequestLldbDone21 answer context]
            :onError [debugRequestLldbFailed21 refused context command]))
  (when (= command "exceptionInfo")
    (invoke "debugLldb.exceptionInfo"
            :with arguments
            :onDone [debugRequestLldbDone22 answer context]
            :onError [debugRequestLldbFailed22 refused context command])))

(defn debugAnswered "TODO" [answer result context]
  (when (not (empty? answer)) (run answer :with {:body (or-else result {}) :context context})))

(defn debugRefused "TODO" [command refused context error]
  (when (not (= error.code "ended"))
    (cond
      (not (empty? refused)) (run refused :with {:message error.message :context context})
      (or (= command "initialize") (= command "launch") (= command "attach"))
        (debugEnd (str "The debugger could not " command ": " error.message))
      (= command "disconnect") (debugEnd "")
      :else (debugPrint (str command ": " error.message) "error"))))

(defn debugApplyResponse "TODO" []
  (run event.answer :with {:body (or-else event.body {}) :context (or-else event.context {})}))

(defn debugApplyRefusal "TODO" []
  (debugRefused event.command (or-else event.refused "") (or-else event.context {}) event.error))

(defn debugIgnore "TODO" []
  nil)

(defn debugOnInitialize "TODO" [body]
  (set "state.debug.capabilities" body)
  (debugApplyFilters (or-else body.exceptionBreakpointFilters []))
  (debugRequest computed.debugConfiguration.request "debugOnLaunch" {} "" computed.debugConfiguration.arguments))

(defn debugApplyFilters "TODO" [filters]
  (set "state.debug.exceptionFilters"
       (map (fn [it]
              {:filter (text it.filter)
               :label (text (or-else it.label it.filter))
               :enabled (or-else (get state.debug.filterChoices (text it.filter)) it.default false)})
            (slice filters 0 20))))

(defn debugOnLaunch "TODO" []
  (when (= state.debug.status "starting") (set "state.debug.status" "running")))

(defn debugConfigure "TODO" []
  (set "state.debug.configured" true)
  (doseq [path (distinct (map (fn [it] it.path) state.debug.breakpoints))]
    (debugSendBreakpoints path))
  (when (> (count state.debug.exceptionFilters) 0) (debugSendFilters))
  (when (or-else state.debug.capabilities.supportsConfigurationDoneRequest false)
    (debugRequest "configurationDone" "" {} "" {})))

(defn debugSendBreakpoints "TODO" [path]
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

(defn debugOnSetBreakpoints "TODO" [context body]
  (set "state.debug.breakpoints"
       (map (fn [it]
              (if (contains context.ids it.id)
                (merge it
                       {:verified (or-else (get-in body.breakpoints [(indexOf context.ids it.id) :verified]) false)
                        :message (text (or-else (get-in body.breakpoints [(indexOf context.ids it.id) :message]) ""))
                        :adapterId (or-else (get-in body.breakpoints [(indexOf context.ids it.id) :id]) nil)})
                it))
            state.debug.breakpoints)))

(defn debugSendFilters "TODO" []
  (when (and (not= state.debug.status "inactive") state.debug.configured)
    (debugRequest "setExceptionBreakpoints"
                  ""
                  {}
                  ""
                  {:filters (map (fn [it] it.filter) (filter (fn [it] it.enabled) state.debug.exceptionFilters))})))

(defn debugEvent "TODO" [message]
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
    (debugPrint (str "The program exited with code " (or-else message.body.exitCode 0)) "console"))
  (when (= message.event "terminated") (debugEnd "")))

(defn debugApplyEvent "TODO" [event]
  (debugEvent event))

(defn debugOutput "TODO" [body]
  (when (and (not= body.category "telemetry") (not (empty? body.output)))
    (debugPrintLines (split body.output "\n") (or-else body.category "console"))))

(defn debugPrintLines "TODO" [lines kind]
  (set "state.debug.console"
       (slice (concat state.debug.console
                      (map-indexed (fn [index it]
                                     {:id (str "c" (+ state.debug.consoleNext index)) :text it :kind kind})
                                   (slice lines 0 (- (count lines) (if (empty? (get lines (- (count lines) 1))) 1 0)))))
              (- 2000)))
  (increment "state.debug.consoleNext" :by (count lines)))

(defn debugPrint "TODO" [text kind]
  (debugPrintLines [text] kind))

(defn debugStopped "TODO" [body]
  (set "state.debug.status" "stopped")
  (set "state.debug.reason" (or-else body.reason "pause"))
  (set "state.debug.threadId" (or-else body.threadId state.debug.threadId (or-else state.debug.threads.0.id 1)))
  (set "state.debug.exception"
       (if (= body.reason "exception") (text (or-else body.text body.description "Exception")) ""))
  (debugRequest "threads" "debugOnThreads" {} "" {})
  (debugLoadStack)
  (when (and (= body.reason "exception") (or-else state.debug.capabilities.supportsExceptionInfoRequest false))
    (debugRequest "exceptionInfo" "debugOnExceptionInfo" {} "debugIgnore" {:threadId state.debug.threadId})))

(defn debugLoadStack "TODO" []
  (debugRequest "stackTrace"
                "debugOnStackTrace"
                {:threadId state.debug.threadId}
                ""
                {:threadId state.debug.threadId :startFrame 0 :levels 50}))

(defn debugOnThreads "TODO" [body]
  (set "state.debug.threads"
       (map (fn [it] {:id it.id :name (text (or-else it.name it.id))}) (or-else body.threads []))))

(defn debugOnExceptionInfo "TODO" [body]
  (set "state.debug.exception"
       (str (text (or-else body.exceptionId "Exception"))
            (if (empty? body.description) "" (str ": " body.description)))))

(defn debugOnStackTrace "TODO" [context body]
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

(defn debugResumed "TODO" []
  (set "state.debug.frames" [])
  (set "state.debug.frameId" nil)
  (set "state.debug.variables" [])
  (set "state.debug.exception" "")
  (set "state.debug.current" {:path "" :line 0 :inside false})
  (set "state.debug.hover" {:path "" :word "" :line 0 :value ""}))

(defn debugEnd "TODO" [message]
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
      (debugPrint "The debug session ended." "console"))
    (when state.debug.restartAfter (set "state.debug.restartAfter" false) (debugStart))))

(defcommand debugContinue "Continue"
  :title ~(t "command.debugContinue") :category ~(t "category.debug") :icon "play"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "continue"))

(defcommand debugStepOver "Step Over"
  :title ~(t "command.debugStepOver") :category ~(t "category.debug") :icon "redo-dot"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "next"))

(defcommand debugStepInto "Step Into"
  :title ~(t "command.debugStepInto") :category ~(t "category.debug") :icon "arrow-down-to-dot"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "stepIn"))

(defcommand debugStepOut "Step Out"
  :title ~(t "command.debugStepOut") :category ~(t "category.debug") :icon "arrow-up-from-dot"
  :enablement ~(= state.debug.status "stopped") []
  (debugStep "stepOut"))

(defn debugStep "TODO" [command]
  (when (= state.debug.status "stopped")
    (set "state.debug.status" "running")
    (debugResumed)
    (debugRequest command "" {} "" {:threadId state.debug.threadId})))

(defcommand debugPause "Pause"
  :title ~(t "command.debugPause") :category ~(t "category.debug") :icon "pause"
  :enablement ~(= state.debug.status "running") []
  (debugRequest "threads" "debugOnThreadsPause" {} "" {}))

(defn debugOnThreadsPause "TODO" [body]
  (debugOnThreads body)
  (when (> (count state.debug.threads) 0) (debugRequest "pause" "" {} "" {:threadId state.debug.threads.0.id})))

(defcommand debugStop "Stop"
  :title ~(t "command.debugStop") :category ~(t "category.debug") :icon "square"
  :enablement ~(not= state.debug.status "inactive") []
  (when (not= state.debug.status "inactive")
    (debugRequest "disconnect" "debugOnDisconnect" {} "" {:terminateDebuggee true})))

(defn debugOnDisconnect "TODO" []
  (debugEnd ""))

(defcommand debugRestart "Restart"
  :title ~(t "command.debugRestart") :category ~(t "category.debug") :icon "rotate-ccw"
  :enablement ~(not= state.debug.status "inactive") []
  (if (or-else state.debug.capabilities.supportsRestartRequest false)
    (do
      (set "state.debug.status" "running")
      (debugResumed)
      (debugPrint "Restarting" "console")
      (debugRequest "restart" "" {} "" {:arguments computed.debugConfiguration.arguments}))
    (do (set "state.debug.restartAfter" true) (debugStop))))

(defn debugSelectStack "TODO" [id]
  (if (= (slice id 0 2) "t:")
    (do
      (when (and (= state.debug.status "stopped") (not= (slice id 2) (text state.debug.threadId)))
        (set "state.debug.threadId" (:id (find (fn [it] (= (text it.id) (slice id 2))) state.debug.threads)))
        (debugLoadStack)))
    (do
      (when (some? (find (fn [it] (= (str "f:" it.id) id)) state.debug.frames))
        (debugSelectFrame (find (fn [it] (= (str "f:" it.id) id)) state.debug.frames) true)))))

(defn debugSelectFrame "TODO" [frame ask]
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
      (do (openPath frame.relative (get (reverse (split frame.relative "/")) 0)) (goToLine frame.line))
    (contains computed.docIds state.debug.current.path)
      (showTab state.debug.current.path (:label (find (fn [it] (= it.id state.debug.current.path)) state.docs)))
    :else
      (do
        (when (and ask (or (> frame.reference 0) (not (empty? frame.path))))
          (set "state.debug.outside" {:open true :frame frame})))))

(defn debugOnScopes "TODO" [context body]
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

(defn debugExpand "TODO" [path]
  (debugExpandRow (find (fn [it] (= it.path path)) state.debug.variables)))

(defn debugExpandRow "TODO" [row]
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

(defn debugLoadVariables "TODO" [parent ref depth start indexed replace]
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

(defn debugOnVariables "TODO" [context body]
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

(defn debugInsertRows "TODO" [at skip rows]
  (set "state.debug.variables"
       (concat (slice state.debug.variables 0 at) rows (slice state.debug.variables (+ at skip)))))

(defcommand debugEditValue "Set Value"
  :title ~(t "command.debugEditValue") :category ~(t "category.debug") :icon "pencil"
  :enablement ~(= state.debug.status "stopped") []
  (debugEditValueOf (find (fn [it] (= it.path state.debug.selectedVariable)) state.debug.variables)))

(defn debugEditValueOf "TODO" [row]
  (if (and (some? row) (= row.kind "variable") (or-else state.debug.capabilities.supportsSetVariable false))
    (set "state.debug.setValue" {:open true :path row.path :name row.name :value row.value})
    (notify "info" "Select a variable: the debugger can set the value of variables only.")))

(defn debugSetValue "TODO" []
  (set "state.debug.setValue.open" false)
  (debugRequest "setVariable"
                "debugOnSetVariable"
                {:path state.debug.setValue.path}
                ""
                {:variablesReference (:container (find (fn [it] (= it.path state.debug.setValue.path))
                                                       state.debug.variables))
                 :name state.debug.setValue.name
                 :value state.debug.setValue.value}))

(defn debugOnSetVariable "TODO" [context body]
  (set "state.debug.variables"
       (map (fn [it]
              (if (= it.path context.path)
                (merge it {:value (text (or-else body.value it.value)) :type (text (or-else body.type it.type))})
                it))
            state.debug.variables))
  (debugRefreshWatches))

(defn debugAddWatch "TODO" []
  (when (and (not (empty? (trim state.debug.watchDraft))) (< (count state.debug.watches) 100))
    (push "state.debug.watches"
          {:id (str "w" state.debug.watchNext) :expression (trim state.debug.watchDraft) :value "" :error ""})
    (increment "state.debug.watchNext" :by 1)
    (set "state.debug.watchDraft" "")
    (debugRefreshWatches)))

(defn debugToggleSection "section: variables, watch, stack or breakpoints. Collapses it, or expands it again"
  [section]
  (set "state.debug.collapsed"
       (if (contains state.debug.collapsed section)
         (filter (fn [it] (not= it section)) state.debug.collapsed)
         (concat state.debug.collapsed [section]))))

(defn debugRemoveWatch "TODO" []
  (set "state.debug.watches" (filter (fn [it] (not= it.id state.debug.selectedWatch)) state.debug.watches)))

(defn debugRefreshWatches "TODO" []
  (when (and (= state.debug.status "stopped") (some? state.debug.frameId))
    (doseq [watch state.debug.watches]
      (debugRequest "evaluate"
                    "debugOnWatch"
                    {:id watch.id}
                    "debugOnWatchError"
                    {:expression watch.expression :frameId state.debug.frameId :context "watch"}))))

(defn debugOnWatch "TODO" [context body]
  (set "state.debug.watches"
       (map (fn [it] (if (= it.id context.id) (merge it {:value (text (or-else body.result "")) :error ""}) it))
            state.debug.watches)))

(defn debugOnWatchError "TODO" [context message]
  (set "state.debug.watches"
       (map (fn [it] (if (= it.id context.id) (merge it {:value "" :error message}) it)) state.debug.watches)))

(defn debugEvaluate "TODO" []
  (when (not (empty? (trim state.debug.input)))
    (debugPrint state.debug.input "input")
    (set "state.debug.history"
         (slice (concat (filter (fn [it] (not= it state.debug.input)) state.debug.history) [state.debug.input])
                (- 100)))
    (set "state.debug.historyAt" (count state.debug.history))
    (set "state.debug.completions" [])
    (if (= state.debug.status "inactive")
      (debugPrint "Start a debug session to evaluate expressions." "error")
      (debugRequest "evaluate"
                    "debugOnEvaluate"
                    {}
                    "debugOnEvaluateError"
                    (merge {:expression state.debug.input :context "repl"}
                           (if (nil? state.debug.frameId) {} {:frameId state.debug.frameId}))))
    (set "state.debug.input" "")))

(defn debugOnEvaluate "TODO" [body]
  (debugPrint (text (or-else body.result "")) "result"))

(defn debugOnEvaluateError "TODO" [message]
  (debugPrint message "error"))

(defn debugHistoryBack "TODO" []
  (when (> state.debug.historyAt 0)
    (increment "state.debug.historyAt" :by -1)
    (set "state.debug.input" (get state.debug.history state.debug.historyAt))))

(defn debugHistoryForward "TODO" []
  (when (< state.debug.historyAt (count state.debug.history)) (increment "state.debug.historyAt" :by 1))
  (set "state.debug.input" (or-else (get state.debug.history state.debug.historyAt) "")))

(defn debugComplete "TODO" []
  (when (and (not= state.debug.status "inactive") (or-else state.debug.capabilities.supportsCompletionsRequest false))
    (debugRequest "completions"
                  "debugOnCompletions"
                  {:input state.debug.input}
                  "debugIgnore"
                  (merge {:text state.debug.input :column (+ (count state.debug.input) 1)}
                         (if (nil? state.debug.frameId) {} {:frameId state.debug.frameId})))))

(defn debugOnCompletions "TODO" [context body]
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
  :title ~(t "command.debugClearConsole") :category ~(t "category.debug") :icon "eraser" []
  (set "state.debug.console" []))

(defcommand showDebugConsole "Show Debug Console"
  :title ~(t "command.showDebugConsole") :category ~(t "category.view") :icon "terminal" []
  (set "state.layout.panel" true)
  (set "state.panelTab" "debugConsole"))

(defn debugHover "TODO" [path]
  (if (or (not= state.debug.status "stopped") (empty? event.word) (nil? event.line))
    (set "state.debug.hover" {:path "" :word "" :line 0 :value ""})
    (do
      (when (or (not= event.word state.debug.hover.word) (not= event.line state.debug.hover.line))
        (set "state.debug.hover" {:path path :word event.word :line event.line :value ""})
        (debugRequest "evaluate"
                      "debugOnHover"
                      {:word event.word :line event.line}
                      "debugIgnore"
                      {:expression event.word :frameId state.debug.frameId :context "hover"})))))

(defn debugOnHover "TODO" [context body]
  (when (and (= context.word state.debug.hover.word) (= context.line state.debug.hover.line))
    (set "state.debug.hover.value" (slice (text (or-else body.result "")) 0 200))))

(defn debugToggleBreakpoint "TODO" [path line]
  (if (some? (find (fn [it] (and (= it.path path) (= it.line line))) state.debug.breakpoints))
    (set "state.debug.breakpoints"
         (filter (fn [it] (not (and (= it.path path) (= it.line line)))) state.debug.breakpoints))
    (debugAddBreakpoint path line))
  (debugSendBreakpoints path))

(defn debugAddBreakpoint "TODO" [path line]
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
  :title ~(t "command.debugToggleBreakpointAtCaret") :category ~(t "category.debug") :icon "circle-dot"
  :enablement ~(and (some? computed.activeDoc) (= computed.activeDoc.kind "file")) []
  (when (and (some? computed.activeDoc) (= computed.activeDoc.kind "file"))
    (debugToggleBreakpoint computed.activeDoc.id state.caret.line)))

(defn debugGutterClick "TODO" [path]
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

(defn debugEditBreakpointAt "TODO" [path line]
  (when (nil? (find (fn [it] (and (= it.path path) (= it.line line))) state.debug.breakpoints))
    (debugAddBreakpoint path line))
  (debugEditBreakpoint
    (or-else (:id (find (fn [it] (and (= it.path path) (= it.line line))) state.debug.breakpoints)) "")))

(defn debugEditBreakpoint "TODO" [id]
  (when (some? (find (fn [it] (= it.id id)) state.debug.breakpoints))
    (set "state.debug.edit"
         (merge {:open true :id id} (or-else (find (fn [it] (= it.id id)) state.debug.breakpoints) {})))))

(defcommand debugEditSelectedBreakpoint "Edit Breakpoint"
  :title ~(t "command.debugEditSelectedBreakpoint") :category ~(t "category.debug") :icon "pencil"
  :enablement ~(some? state.debug.selectedBreakpoint) []
  (debugEditBreakpoint (or-else state.debug.selectedBreakpoint "")))

(defn debugSaveBreakpoint "TODO" []
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

(defn debugApplyBreakpoint "TODO" []
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

(defn debugToggleEnabled "TODO" [id]
  (set "state.debug.breakpoints"
       (map (fn [it] (if (= it.id id) (merge it {:enabled (not it.enabled)}) it)) state.debug.breakpoints))
  (debugSendBreakpoints (or-else (:path (find (fn [it] (= it.id id)) state.debug.breakpoints)) "")))

(defn debugToggleSelectedEnabled "TODO" []
  (when (some? state.debug.selectedBreakpoint) (debugToggleEnabled state.debug.selectedBreakpoint)))

(defn debugOpenBreakpoint "TODO" []
  (when (some? (find (fn [it] (= it.id event.id)) state.debug.breakpoints))
    (debugGoTo (find (fn [it] (= it.id event.id)) state.debug.breakpoints))))

(defn debugGoTo "TODO" [breakpoint]
  (openPath breakpoint.path (get (reverse (split breakpoint.path "/")) 0))
  (goToLine breakpoint.line))

(defn debugRemoveBreakpoint "TODO" []
  (when (some? state.debug.selectedBreakpoint)
    (debugRemoveBreakpointOf (find (fn [it] (= it.id state.debug.selectedBreakpoint)) state.debug.breakpoints))))

(defn debugRemoveBreakpointOf "TODO" [breakpoint]
  (when (some? breakpoint)
    (set "state.debug.breakpoints" (filter (fn [it] (not= it.id breakpoint.id)) state.debug.breakpoints))
    (set "state.debug.selectedBreakpoint" nil)
    (debugSendBreakpoints breakpoint.path)))

(defn distinct "A list's items, each once, in the order they first come." [items]
  (reduce (fn [seen x] (if (some (fn [y] (= y x)) seen) seen (conj seen x))) [] items))

(defcommand debugRemoveAllBreakpoints "Remove All Breakpoints"
  :title ~(t "command.debugRemoveAllBreakpoints") :category ~(t "category.debug") :icon "list-x" []
  (doseq [path (distinct (map (fn [it] it.path) state.debug.breakpoints))]
    (set "state.debug.breakpoints" (filter (fn [it] (not= it.path path)) state.debug.breakpoints))
    (debugSendBreakpoints path))
  (set "state.debug.selectedBreakpoint" nil))

(defn debugToggleFilter "TODO" [filter]
  (set "state.debug.exceptionFilters"
       (map (fn [it] (if (= it.filter filter) (merge it {:enabled (not it.enabled)}) it))
            state.debug.exceptionFilters))
  (set "state.debug.filterChoices"
       (assoc state.debug.filterChoices filter (:enabled (find (fn [it] (= it.filter filter)) state.debug.exceptionFilters))))
  (debugSendFilters))

(defn debugLldbRunInTerminal "TODO" []
  (debugRunInTerminal))

(defn debugPythonRunInTerminal "TODO" []
  (debugRunInTerminal))

(defn debugRunInTerminal "The adapter's runInTerminal: its program runs on the debug terminal, started in its cwd (the pty service keeps it inside the folder), as `exec` and the line shellLine() quotes (null if a word isn't plain text or the line is too long), typed into the POSIX shell `sh`, never the person's login shell, which may be one (fish, csh, cmd.exe) where single quotes don't protect a word."
  []
  (debugRunLine (and (= (type event.args) "list") (not (empty? event.args)))
                (if (= (type event.args) "list")
                  (shellLine event.args (if (= (type event.env) "object") event.env nil))
                  nil)
                (if (= (type event.cwd) "string") event.cwd "")))

(defn debugRunLine "TODO" [named line folder]
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

(defn debugLoadConfigurationsFailed "What debugLoadConfigurations does when it fails." [error]
  (if (= error.code "notFound")
    (debugApplyConfigurations {:configurations [] :skipped 0})
    (set "state.debug.error" (str "Cannot read .forge/launch.json: " error.message))))

(defn debugLoadConfigurationsDone "What debugLoadConfigurations does when it is done." [result]
  (debugReadLaunchFile (parseJsonc result.text)))

(defn debugSubscribeEach "What debugSubscribe does when its time comes." [event]
  (debugEvent event))

(defn debugSubscribeFailed "What debugSubscribe does when it fails." [error]
  (debugEnd error.message))

(defn debugSubscribeEach2 "What debugSubscribe does when its time comes." [event]
  (debugEvent event))

(defn debugSubscribeFailed2 "What debugSubscribe does when it fails." [error]
  (debugEnd error.message))

(defn debugRequestPythonDone "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone2 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed2 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone3 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed3 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone4 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed4 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone5 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed5 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone6 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed6 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone7 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed7 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone8 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed8 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone9 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed9 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone10 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed10 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone11 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed11 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone12 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed12 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone13 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed13 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone14 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed14 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone15 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed15 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone16 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed16 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone17 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed17 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone18 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed18 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone19 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed19 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone20 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed20 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone21 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed21 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestPythonDone22 "What debugRequestPython does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestPythonFailed22 "What debugRequestPython does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone2 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed2 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone3 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed3 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone4 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed4 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone5 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed5 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone6 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed6 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone7 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed7 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone8 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed8 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone9 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed9 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone10 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed10 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone11 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed11 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone12 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed12 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone13 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed13 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone14 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed14 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone15 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed15 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone16 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed16 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone17 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed17 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone18 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed18 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone19 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed19 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone20 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed20 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone21 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed21 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRequestLldbDone22 "What debugRequestLldb does when it is done." [answer context result]
  (debugAnswered answer result context))

(defn debugRequestLldbFailed22 "What debugRequestLldb does when it fails." [refused context command error]
  (debugRefused command refused context error))

(defn debugRunLineEvent "What debugRunLine does when it gets an event." [line]
  (when (and (some? event.session) (not state.debug.terminalSent))
    (set "state.debug.terminalSent" true)
    (invoke "debugTerminal.write"
            :with {:session event.session :data (str "exec " line "\n")}
            :onError debugRunLineEventFailed
            :onDone debugRunLineEventDone))
  (when (some? event.data) (debugOutput {:category "stdout" :output event.data})))

(defn debugRunLineEventFailed "What debugRunLineEvent does when it fails." [error]
  (respond nil :error error.message))

(defn debugRunLineEventDone "What debugRunLineEvent does when it is done." []
  (respond {}))

(defn debugRunLineFailed "What debugRunLine does when it fails." [error]
  (when (not state.debug.terminalSent) (respond nil :error error.message)))

(defpart debug-events "What its channels' and services' events run: each a function."
  :actions {"debugLldb:runInTerminal" ~(handler debugLldbRunInTerminal)
            "debugPython:runInTerminal" ~(handler debugPythonRunInTerminal)})
