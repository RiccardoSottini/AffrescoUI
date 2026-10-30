{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's debugger: launch configurations read from .forge/launch.json through the files service (comments allowed: parseJsonc), the subset Forge runs made in debugReadConfigurations, a session with lldb-dap or debugpy through the dap service the host granted (Debug.cpp), breakpoints in the editor's gutter, the call stack, variables, watches and the debug console. Every request goes through debugRequest, which names the service by state.debug.type and runs the answer action with `body` and `context`; the adapter's events go to debugEvent. The actions that take the adapter's messages as events (debugApplySession, debugApplyEvent, debugApplyResponse) let a scenario or an agent drive the views without an adapter. What an adapter sends is bounded in the dap service; here the console keeps 2000 lines, the history 100 entries and the variables 5000 rows. The views are in debugViews.json."
  :services {:debugLldb {:provider dap :process lldb-dap :root workspace :restart {:after 0 :times 100}}
    :debugPython {:provider dap :process debugpy-adapter :root workspace :restart {:after 0 :times 100}}
    :debugTerminal {:provider pty :process sh :root workspace}}
  :state {:debug {:type ""
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
      :edit (node :open false #"" :condition "" :hitCondition "" :logMessage "")
      :setValue {:open false :path "" :name "" :value ""}
      :outside {:open false :frame nil}
      :hover {:path "" :word "" :line 0 :value ""}
      :terminalSent false}}
  :computed {:debugConfiguration "{{state.debug.configurations[number(state.debug.selected)] ?? null}}"
    :debugMarks "{{concat(map(state.debug.breakpoints, {path: it.path, mark: {line: it.line - 1, kind: 'gutter', text: !it.enabled ? '○' : !empty(it.logMessage) ? '◆' : '●', color: !it.enabled || (state.debug.status != 'inactive' && !it.verified) ? '#848484' : '#e51400'}}), state.debug.status == 'stopped' && state.debug.current.line > 0 ? [{path: state.debug.current.path, mark: {line: state.debug.current.line - 1, kind: 'background', color: '#ffcc0040'}}] : [], state.debug.status == 'stopped' && !empty(state.debug.exception) && state.debug.current.line > 0 ? [{path: state.debug.current.path, mark: {line: state.debug.current.line - 1, kind: 'inline', color: '#f85149', text: state.debug.exception}}] : [], state.debug.status == 'stopped' && !empty(state.debug.hover.value) ? [{path: state.debug.hover.path, mark: {line: state.debug.hover.line, kind: 'inline', color: '#3794ff', text: state.debug.hover.word + ' = ' + state.debug.hover.value}}] : [])}}"
    :debugConfigurationRows "{{map(state.debug.configurations, {id: text(index), label: it.name, icon: it.type == 'python' ? 'file-code' : 'cpu'})}}"
    :debugStackRows "{{concat(map(slice(state.debug.threads, 0, indexOf(map(state.debug.threads, it.id), state.debug.threadId) + 1), {id: 't:' + it.id, label: it.name + (it.id == state.debug.threadId && state.debug.status == 'stopped' ? '  paused on ' + state.debug.reason : ''), icon: 'list-tree'}), map(state.debug.frames, {id: 'f:' + it.id, label: '    ' + it.name + '  ' + (empty(it.path) ? 'unknown source' : reverse(split(it.path, '/'))[0]) + ':' + it.line, icon: it.id == state.debug.frameId ? 'arrow-right' : '', iconColor: it.inside ? '#e5c07b' : '#848484'}), map(slice(state.debug.threads, indexOf(map(state.debug.threads, it.id), state.debug.threadId) + 1), {id: 't:' + it.id, label: it.name, icon: 'list-tree'}))}}"
    :debugVariableRows "{{map(state.debug.variables, {id: it.path, label: pad('', it.depth * 2) + (it.kind == 'more' ? '  … ' + it.value : (it.ref > 0 ? (it.expanded ? '▾ ' : '▸ ') : '  ') + it.name + (it.kind == 'scope' ? '' : ' = ' + it.value))})}}"
    :debugWatchRows "{{map(state.debug.watches, {id: it.id, label: it.expression + ' = ' + (empty(it.error) ? (state.debug.status == 'stopped' ? it.value : 'not available') : it.error), icon: 'eye', iconColor: empty(it.error) ? '#848484' : '#f85149'})}}"
    :debugBreakpointRows "{{map(state.debug.breakpoints, {id: it.id, label: reverse(split(it.path, '/'))[0] + ':' + it.line + (empty(it.condition) ? '' : '  if ' + it.condition) + (empty(it.hitCondition) ? '' : '  hits ' + it.hitCondition) + (empty(it.logMessage) ? '' : '  log ' + it.logMessage) + (state.debug.status != 'inactive' && !it.verified && !empty(it.message) ? '  (' + it.message + ')' : ''), icon: !it.enabled ? 'circle-off' : !empty(it.logMessage) ? 'diamond' : 'circle', iconColor: !it.enabled || (state.debug.status != 'inactive' && !it.verified) ? '#848484' : '#e51400'})}}"
    :debugConsoleRows "{{map(state.debug.console, {id: it.id, label: it.text, icon: it.kind == 'input' ? 'chevron-right' : it.kind == 'error' || it.kind == 'stderr' ? 'circle-x' : '', iconColor: it.kind == 'input' ? '#3794ff' : '#f85149'})}}"}
  :actions {:showDebug {:title "Show Debugger"
      :category View
      :icon bug
      :do [{:run forgeShowView :with {:view debug}}
        {:if "{{state.sideView == 'debug' && state.debug.status == 'inactive'}}" :then [{:run debugLoadConfigurations}]}]}
    :debugLoadConfigurations {:title "Reload Launch Configurations"
      :category Debug
      :icon refresh-cw
      :description "Read the open folder's .forge/launch.json again."
      :do [{:if "{{empty(state.folder.root)}}"
          :then [{:run debugApplyConfigurations :with {:found {:configurations [] :skipped 0}}}]
          :else [{:invoke files.read
              :with {:path ".forge/launch.json"}
              :onError [{:if "{{error.code == 'notFound'}}"
                  :then [{:run debugApplyConfigurations :with {:found {:configurations [] :skipped 0}}}]
                  :else [{:set state.debug.error :to "{{'Cannot read .forge/launch.json: ' + error.message}}"}]}]
              :onDone [{:run debugReadLaunchFile :with {:launch "{{parseJsonc(result.text)}}"}}]}]}]}
    :debugReadLaunchFile [{:if "{{launch == null}}"
        :then [{:set state.debug.error :to "Cannot read .forge/launch.json: it isn't JSON"}]
        :else [{:run debugReadConfigurations
            :with {:listed "{{type(launch) == 'object' && type(launch.configurations) == 'list' ? launch.configurations : []}}"}}]}]
    :debugReadConfigurations [{:note "The configurations Forge runs: type lldb (or lldb-dap) or python (or debugpy), request launch (with a program) or attach (debugpy to a local port, lldb to a process id); of the rest only program, cwd, stopOnEntry, args, env and console are passed on, each checked and bounded. The first 100 are kept, the others counted as skipped."}
      {:run debugMakeConfigurations
        :with {:total "{{length(listed)}}"
          :known "{{filter(map(filter(listed, type(it) == 'object'), {c: it, request: it.request, adapter: it.type == 'lldb' || it.type == 'lldb-dap' ? 'lldb' : it.type == 'python' || it.type == 'debugpy' ? 'python' : null, port: it.port ?? (type(it.connect) == 'object' ? it.connect.port : null), terminal: it.console == 'integratedTerminal' || it.console == 'externalTerminal'}), it.adapter != null && (it.request == 'launch' || it.request == 'attach'))}}"}}]
    :debugMakeConfigurations [{:run debugKeepConfigurations
        :with {:total "{{total}}"
          :kept "{{filter(map(known, (it.request == 'launch' ? type(it.c.program) == 'string' && length(it.c.program) <= 4096 : it.adapter == 'python' ? type(it.port) == 'number' && round(it.port) == it.port && it.port >= 1 && it.port <= 65535 : type(it.c.pid) == 'number' && round(it.c.pid) == it.c.pid && it.c.pid > 0) ? {name: type(it.c.name) == 'string' && it.c.name != '' && length(it.c.name) <= 200 ? it.c.name : it.adapter + ' ' + it.request, type: it.adapter, request: it.request, arguments: merge(type(it.c.program) == 'string' && length(it.c.program) <= 4096 ? {program: it.c.program} : {}, type(it.c.cwd) == 'string' && length(it.c.cwd) <= 4096 ? {cwd: it.c.cwd} : {}, type(it.c.stopOnEntry) == 'boolean' ? {stopOnEntry: it.c.stopOnEntry} : {}, type(it.c.args) == 'list' ? {args: slice(filter(it.c.args, type(it) == 'string' && length(it) <= 4096), 0, 1000)} : {}, type(it.c.env) == 'object' ? {env: fromEntries(slice(filter(entries(it.c.env), type(it.value) == 'string' && length(it.value) <= 4096), 0, 1000))} : {}, type(it.c.console) != 'string' ? {} : it.adapter == 'python' ? {console: it.terminal ? 'integratedTerminal' : 'internalConsole'} : it.terminal ? {runInTerminal: true} : {}, it.request != 'attach' ? {} : it.adapter == 'python' ? {connect: {host: '127.0.0.1', port: it.port}} : {pid: it.c.pid})} : null), it != null)}}"}}]
    :debugKeepConfigurations [{:run debugApplyConfigurations
        :with {:found "{{ {configurations: slice(kept, 0, 100), skipped: total - min(length(kept), 100)} }}"}}]
    :debugApplyConfigurations [{:set state.debug.configurations :to "{{found.configurations ?? []}}"}
      {:set state.debug.skipped :to "{{found.skipped ?? 0}}"}
      {:set state.debug.error :to ""}
      {:if "{{number(state.debug.selected) >= length(state.debug.configurations)}}"
        :then [{:set state.debug.selected :to "0"}]}]
    :debugApplyLaunchFile [{:run debugReadLaunchFile :with {:launch "{{event}}"}}]
    :debugContinueOrStart {:title "Start Debugging or Continue"
      :category Debug
      :icon play
      :do [{:if "{{state.debug.status == 'stopped'}}"
          :then [{:run debugContinue}]
          :else [{:if "{{state.debug.status == 'inactive'}}" :then [{:run debugStart}]}]}]}
    :debugStart {:title "Start Debugging"
      :category Debug
      :icon play
      :enablement "{{state.debug.status == 'inactive'}}"
      :do [{:if "{{computed.debugConfiguration == null}}"
          :then [{:run notify
              :with {:severity info
                :message "No launch configuration: add one of type lldb or python to the open folder's .forge/launch.json, then reload."}}]
          :else [{:run debugBegin :with {:configuration "{{computed.debugConfiguration}}"}}
            {:run debugSubscribe}
            {:run debugRequest
              :with {:command initialize
                :answer debugOnInitialize
                :context {}
                :refused ""
                :arguments "{{ {clientID: 'forge', clientName: 'Forge', adapterID: state.debug.type == 'python' ? 'debugpy' : 'lldb-dap', locale: 'en', linesStartAt1: true, columnsStartAt1: true, pathFormat: 'path', supportsVariableType: true, supportsVariablePaging: true, supportsRunInTerminalRequest: true} }}"}}]}]}
    :debugBegin [{:set state.debug.type :to "{{configuration.type}}"}
      {:set state.debug.status :to starting}
      {:set state.debug.configured :to false}
      {:set state.debug.restartAfter :to false}
      {:set state.debug.capabilities :to {}}
      {:set state.debug.terminalSent :to false}
      {:set state.debug.threads :to []}
      {:set state.debug.threadId :to nil}
      {:run debugResumed}
      {:set state.debug.breakpoints
        :to "{{map(state.debug.breakpoints, merge(it, {verified: false, message: '', adapterId: null}))}}"}
      {:run debugPrint :with {:text "{{'Starting ' + configuration.name}}" :kind console}}
      {:set state.layout.panel :to true}
      {:set state.panelTab :to debugConsole}]
    :debugApplySession [{:set state.debug.configurations :to "{{[event.configuration]}}"}
      {:set state.debug.selected :to "0"}
      {:run debugBegin :with {:configuration "{{event.configuration}}"}}
      {:set state.debug.capabilities :to "{{event.capabilities ?? {} }}"}
      {:run debugApplyFilters :with {:filters "{{event.capabilities.exceptionBreakpointFilters ?? []}}"}}
      {:set state.debug.status :to running}]
    :debugSubscribe [{:if "{{state.debug.type == 'python'}}"
        :then [(node :subscribe debugPython.events
            #debug-events
            :do [{:run debugEvent :with {:message "{{event}}"}}]
            :onError [{:run debugEnd :with {:message "{{error.message}}"}}])]
        :else [(node :subscribe debugLldb.events
            #debug-events
            :do [{:run debugEvent :with {:message "{{event}}"}}]
            :onError [{:run debugEnd :with {:message "{{error.message}}"}}])]}]
    :debugRequest [{:if "{{state.debug.type == 'python'}}"
        :then [{:run debugRequestPython}]
        :else [{:run debugRequestLldb}]}]
    :debugRequestPython [{:if "{{command == 'initialize'}}"
        :then [{:invoke debugPython.initialize
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'launch'}}"
        :then [{:invoke debugPython.launch
            :with "{{arguments}}"
            :timeout 60000
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'attach'}}"
        :then [{:invoke debugPython.attach
            :with "{{arguments}}"
            :timeout 60000
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'configurationDone'}}"
        :then [{:invoke debugPython.configurationDone
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'setBreakpoints'}}"
        :then [{:invoke debugPython.setBreakpoints
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'setExceptionBreakpoints'}}"
        :then [{:invoke debugPython.setExceptionBreakpoints
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'threads'}}"
        :then [{:invoke debugPython.threads
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'stackTrace'}}"
        :then [{:invoke debugPython.stackTrace
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'scopes'}}"
        :then [{:invoke debugPython.scopes
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'variables'}}"
        :then [{:invoke debugPython.variables
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'source'}}"
        :then [{:invoke debugPython.source
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'evaluate'}}"
        :then [{:invoke debugPython.evaluate
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'setVariable'}}"
        :then [{:invoke debugPython.setVariable
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'continue'}}"
        :then [{:invoke debugPython.continue
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'next'}}"
        :then [{:invoke debugPython.next
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'stepIn'}}"
        :then [{:invoke debugPython.stepIn
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'stepOut'}}"
        :then [{:invoke debugPython.stepOut
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'pause'}}"
        :then [{:invoke debugPython.pause
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'restart'}}"
        :then [{:invoke debugPython.restart
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'disconnect'}}"
        :then [{:invoke debugPython.disconnect
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'completions'}}"
        :then [{:invoke debugPython.completions
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'exceptionInfo'}}"
        :then [{:invoke debugPython.exceptionInfo
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}]
    :debugRequestLldb [{:if "{{command == 'initialize'}}"
        :then [{:invoke debugLldb.initialize
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'launch'}}"
        :then [{:invoke debugLldb.launch
            :with "{{arguments}}"
            :timeout 60000
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'attach'}}"
        :then [{:invoke debugLldb.attach
            :with "{{arguments}}"
            :timeout 60000
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'configurationDone'}}"
        :then [{:invoke debugLldb.configurationDone
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'setBreakpoints'}}"
        :then [{:invoke debugLldb.setBreakpoints
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'setExceptionBreakpoints'}}"
        :then [{:invoke debugLldb.setExceptionBreakpoints
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'threads'}}"
        :then [{:invoke debugLldb.threads
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'stackTrace'}}"
        :then [{:invoke debugLldb.stackTrace
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'scopes'}}"
        :then [{:invoke debugLldb.scopes
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'variables'}}"
        :then [{:invoke debugLldb.variables
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'source'}}"
        :then [{:invoke debugLldb.source
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'evaluate'}}"
        :then [{:invoke debugLldb.evaluate
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'setVariable'}}"
        :then [{:invoke debugLldb.setVariable
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'continue'}}"
        :then [{:invoke debugLldb.continue
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'next'}}"
        :then [{:invoke debugLldb.next
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'stepIn'}}"
        :then [{:invoke debugLldb.stepIn
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'stepOut'}}"
        :then [{:invoke debugLldb.stepOut
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'pause'}}"
        :then [{:invoke debugLldb.pause
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'restart'}}"
        :then [{:invoke debugLldb.restart
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'disconnect'}}"
        :then [{:invoke debugLldb.disconnect
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'completions'}}"
        :then [{:invoke debugLldb.completions
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}
      {:if "{{command == 'exceptionInfo'}}"
        :then [{:invoke debugLldb.exceptionInfo
            :with "{{arguments}}"
            :onDone [{:run debugAnswered}]
            :onError [{:run debugRefused}]}]}]
    :debugAnswered [{:if "{{!empty(answer)}}"
        :then [{:run "{{answer}}" :with {:body "{{result ?? {} }}" :context "{{context}}"}}]}]
    :debugRefused [{:if "{{error.code == 'ended'}}" :then [{:stop true}]}
      {:if "{{!empty(refused)}}"
        :then [{:run "{{refused}}" :with {:message "{{error.message}}" :context "{{context}}"}} {:stop true}]}
      {:if "{{command == 'initialize' || command == 'launch' || command == 'attach'}}"
        :then [{:run debugEnd :with {:message "{{'The debugger could not ' + command + ': ' + error.message}}"}}]
        :else [{:if "{{command == 'disconnect'}}"
            :then [{:run debugEnd :with {:message ""}}]
            :else [{:run debugPrint :with {:text "{{command + ': ' + error.message}}" :kind error}}]}]}]
    :debugApplyResponse [{:run "{{event.answer}}"
        :with {:body "{{event.body ?? {} }}" :context "{{event.context ?? {} }}"}}]
    :debugApplyRefusal [{:run debugRefused
        :with {:command "{{event.command}}"
          :refused "{{event.refused ?? ''}}"
          :context "{{event.context ?? {} }}"
          :error "{{event.error}}"}}]
    :debugIgnore []
    :debugOnInitialize [{:set state.debug.capabilities :to "{{body}}"}
      {:run debugApplyFilters :with {:filters "{{body.exceptionBreakpointFilters ?? []}}"}}
      {:run debugRequest
        :with {:command "{{computed.debugConfiguration.request}}"
          :arguments "{{computed.debugConfiguration.arguments}}"
          :answer debugOnLaunch
          :context {}
          :refused ""}}]
    :debugApplyFilters [{:set state.debug.exceptionFilters
        :to "{{map(slice(filters, 0, 20), {filter: text(it.filter), label: text(it.label ?? it.filter), enabled: state.debug.filterChoices[text(it.filter)] ?? it.default ?? false})}}"}]
    :debugOnLaunch [{:if "{{state.debug.status == 'starting'}}" :then [{:set state.debug.status :to running}]}]
    :debugConfigure [{:set state.debug.configured :to true}
      {:forEach "{{filter(map(state.debug.breakpoints, it.path), indexOf(map(state.debug.breakpoints, it.path), it) == index)}}"
        :as path
        :do [{:run debugSendBreakpoints :with {:path "{{path}}"}}]}
      {:if "{{length(state.debug.exceptionFilters) > 0}}" :then [{:run debugSendFilters}]}
      {:if "{{state.debug.capabilities.supportsConfigurationDoneRequest ?? false}}"
        :then [{:run debugRequest :with {:command configurationDone :arguments {} :answer "" :context {} :refused ""}}]}]
    :debugSendBreakpoints [{:if "{{state.debug.status != 'inactive' && state.debug.configured}}"
        :then [{:run debugRequest
            :with {:command setBreakpoints
              :answer debugOnSetBreakpoints
              :refused ""
              :context "{{ {path: path, ids: map(filter(state.debug.breakpoints, it.path == path && it.enabled), it.id)} }}"
              :arguments "{{ {source: {path: path}, breakpoints: map(filter(state.debug.breakpoints, it.path == path && it.enabled), merge({line: it.line}, empty(it.condition) ? {} : {condition: it.condition}, empty(it.hitCondition) ? {} : {hitCondition: it.hitCondition}, empty(it.logMessage) ? {} : {logMessage: it.logMessage})), sourceModified: false} }}"}}]}]
    :debugOnSetBreakpoints [{:set state.debug.breakpoints
        :to "{{map(state.debug.breakpoints, contains(context.ids, it.id) ? merge(it, {verified: body.breakpoints[indexOf(context.ids, it.id)].verified ?? false, message: text(body.breakpoints[indexOf(context.ids, it.id)].message ?? ''), adapterId: body.breakpoints[indexOf(context.ids, it.id)].id ?? null}) : it)}}"}]
    :debugSendFilters [{:if "{{state.debug.status != 'inactive' && state.debug.configured}}"
        :then [{:run debugRequest
            :with {:command setExceptionBreakpoints
              :answer ""
              :context {}
              :refused ""
              :arguments "{{ {filters: map(filter(state.debug.exceptionFilters, it.enabled), it.filter)} }}"}}]}]
    :debugEvent [{:if "{{message.event == 'initialized'}}" :then [{:run debugConfigure}]}
      {:if "{{message.event == 'stopped'}}" :then [{:run debugStopped :with {:body "{{message.body ?? {} }}"}}]}
      {:if "{{message.event == 'continued'}}" :then [{:set state.debug.status :to running} {:run debugResumed}]}
      {:if "{{message.event == 'output'}}" :then [{:run debugOutput :with {:body "{{message.body ?? {} }}"}}]}
      {:if "{{message.event == 'breakpoint'}}"
        :then [{:set state.debug.breakpoints
            :to "{{map(state.debug.breakpoints, it.adapterId != null && it.adapterId == message.body.breakpoint.id ? merge(it, {verified: message.body.breakpoint.verified ?? it.verified, message: text(message.body.breakpoint.message ?? '')}) : it)}}"}]}
      {:if "{{message.event == 'exited'}}"
        :then [{:run debugPrint
            :with {:text "{{'The program exited with code ' + (message.body.exitCode ?? 0)}}" :kind console}}]}
      {:if "{{message.event == 'terminated'}}" :then [{:run debugEnd :with {:message ""}}]}]
    :debugApplyEvent [{:run debugEvent :with {:message "{{event}}"}}]
    :debugOutput [{:if "{{body.category != 'telemetry' && !empty(body.output)}}"
        :then [{:run debugPrintLines
            :with {:lines "{{split(body.output, '\n')}}" :kind "{{body.category ?? 'console'}}"}}]}]
    :debugPrintLines [{:set state.debug.console
        :to "{{slice(concat(state.debug.console, map(slice(lines, 0, length(lines) - (empty(lines[length(lines) - 1]) ? 1 : 0)), {id: 'c' + (state.debug.consoleNext + index), text: it, kind: kind})), -2000)}}"}
      {:increment state.debug.consoleNext :by "{{length(lines)}}"}]
    :debugPrint [{:run debugPrintLines :with {:lines "{{[text]}}" :kind "{{kind}}"}}]
    :debugStopped [{:set state.debug.status :to stopped}
      {:set state.debug.reason :to "{{body.reason ?? 'pause'}}"}
      {:set state.debug.threadId :to "{{body.threadId ?? state.debug.threadId ?? (state.debug.threads[0].id ?? 1)}}"}
      {:set state.debug.exception
        :to "{{body.reason == 'exception' ? text(body.text ?? body.description ?? 'Exception') : ''}}"}
      {:run debugRequest :with {:command threads :arguments {} :answer debugOnThreads :context {} :refused ""}}
      {:run debugLoadStack}
      {:if "{{body.reason == 'exception' && (state.debug.capabilities.supportsExceptionInfoRequest ?? false)}}"
        :then [{:run debugRequest
            :with {:command exceptionInfo
              :arguments "{{ {threadId: state.debug.threadId} }}"
              :answer debugOnExceptionInfo
              :context {}
              :refused debugIgnore}}]}]
    :debugLoadStack [{:run debugRequest
        :with {:command stackTrace
          :arguments "{{ {threadId: state.debug.threadId, startFrame: 0, levels: 50} }}"
          :answer debugOnStackTrace
          :context "{{ {threadId: state.debug.threadId} }}"
          :refused ""}}]
    :debugOnThreads [{:set state.debug.threads
        :to "{{map(body.threads ?? [], {id: it.id, name: text(it.name ?? it.id)})}}"}]
    :debugOnExceptionInfo [{:set state.debug.exception
        :to "{{text(body.exceptionId ?? 'Exception') + (empty(body.description) ? '' : ': ' + body.description)}}"}]
    :debugOnStackTrace [{:if "{{context.threadId == state.debug.threadId && state.debug.status == 'stopped'}}"
        :then [{:set state.debug.frames
            :to "{{map(body.stackFrames ?? [], {id: it.id, name: text(it.name ?? ''), line: it.line ?? 0, path: text(it.source.path ?? it.source.name ?? ''), relative: text(it.source.relative ?? ''), inside: it.source.inside ?? false, reference: it.source.sourceReference ?? 0})}}"}
          {:if "{{length(state.debug.frames) > 0}}"
            :then [{:run debugSelectFrame :with {:frame "{{state.debug.frames[0]}}" :ask false}}]}]}]
    :debugResumed [{:set state.debug.frames :to []}
      {:set state.debug.frameId :to nil}
      {:set state.debug.variables :to []}
      {:set state.debug.exception :to ""}
      {:set state.debug.current :to {:path "" :line 0 :inside false}}
      {:set state.debug.hover :to {:path "" :word "" :line 0 :value ""}}]
    :debugEnd [{:if "{{state.debug.status != 'inactive'}}"
        :then [{:set state.debug.status :to inactive}
          {:cancel debug-events}
          {:run debugResumed}
          {:set state.debug.threads :to []}
          {:set state.debug.threadId :to nil}
          {:set state.debug.breakpoints
            :to "{{map(state.debug.breakpoints, merge(it, {verified: false, message: '', adapterId: null}))}}"}
          {:if "{{!empty(message)}}"
            :then [{:run debugPrint :with {:text "{{message}}" :kind error}}
              {:run notify :with {:severity error :message "{{message}}"}}]
            :else [{:run debugPrint :with {:text "The debug session ended." :kind console}}]}
          {:if "{{state.debug.restartAfter}}" :then [{:set state.debug.restartAfter :to false} {:run debugStart}]}]}]
    :debugContinue {:title Continue
      :category Debug
      :icon play
      :enablement "{{state.debug.status == 'stopped'}}"
      :do [{:run debugStep :with {:command continue}}]}
    :debugStepOver {:title "Step Over"
      :category Debug
      :icon redo-dot
      :enablement "{{state.debug.status == 'stopped'}}"
      :do [{:run debugStep :with {:command next}}]}
    :debugStepInto {:title "Step Into"
      :category Debug
      :icon arrow-down-to-dot
      :enablement "{{state.debug.status == 'stopped'}}"
      :do [{:run debugStep :with {:command stepIn}}]}
    :debugStepOut {:title "Step Out"
      :category Debug
      :icon arrow-up-from-dot
      :enablement "{{state.debug.status == 'stopped'}}"
      :do [{:run debugStep :with {:command stepOut}}]}
    :debugStep [{:if "{{state.debug.status == 'stopped'}}"
        :then [{:set state.debug.status :to running}
          {:run debugResumed}
          {:run debugRequest
            :with {:command "{{command}}"
              :arguments "{{ {threadId: state.debug.threadId} }}"
              :answer ""
              :context {}
              :refused ""}}]}]
    :debugPause {:title Pause
      :category Debug
      :icon pause
      :enablement "{{state.debug.status == 'running'}}"
      :do [{:run debugRequest
          :with {:command threads :arguments {} :answer debugOnThreadsPause :context {} :refused ""}}]}
    :debugOnThreadsPause [{:run debugOnThreads :with {:body "{{body}}"}}
      {:if "{{length(state.debug.threads) > 0}}"
        :then [{:run debugRequest
            :with {:command pause
              :arguments "{{ {threadId: state.debug.threads[0].id} }}"
              :answer ""
              :context {}
              :refused ""}}]}]
    :debugStop {:title Stop
      :category Debug
      :icon square
      :enablement "{{state.debug.status != 'inactive'}}"
      :do [{:if "{{state.debug.status != 'inactive'}}"
          :then [{:run debugRequest
              :with {:command disconnect
                :arguments {:terminateDebuggee true}
                :answer debugOnDisconnect
                :context {}
                :refused ""}}]}]}
    :debugOnDisconnect [{:run debugEnd :with {:message ""}}]
    :debugRestart {:title Restart
      :category Debug
      :icon rotate-ccw
      :enablement "{{state.debug.status != 'inactive'}}"
      :do [{:if "{{state.debug.capabilities.supportsRestartRequest ?? false}}"
          :then [{:set state.debug.status :to running}
            {:run debugResumed}
            {:run debugPrint :with {:text Restarting :kind console}}
            {:run debugRequest
              :with {:command restart
                :arguments "{{ {arguments: computed.debugConfiguration.arguments} }}"
                :answer ""
                :context {}
                :refused ""}}]
          :else [{:set state.debug.restartAfter :to true} {:run debugStop}]}]}
    :debugSelectStack [{:if "{{slice(id, 0, 2) == 't:'}}"
        :then [{:if "{{state.debug.status == 'stopped' && slice(id, 2) != text(state.debug.threadId)}}"
            :then [{:set state.debug.threadId :to "{{find(state.debug.threads, text(it.id) == slice(id, 2)).id}}"}
              {:run debugLoadStack}]}]
        :else [{:if "{{find(state.debug.frames, 'f:' + it.id == id) != null}}"
            :then [{:run debugSelectFrame :with {:frame "{{find(state.debug.frames, 'f:' + it.id == id)}}" :ask true}}]}]}]
    :debugSelectFrame [{:set state.debug.frameId :to "{{frame.id}}"}
      {:set state.debug.selectedStack :to "{{'f:' + frame.id}}"}
      {:set state.debug.current
        :to "{{ {path: frame.inside ? frame.relative : 'debug-source:' + (empty(frame.path) ? 'ref:' + frame.reference : frame.path), line: frame.line, inside: frame.inside} }}"}
      {:set state.debug.variables :to []}
      {:run debugRequest
        :with {:command scopes
          :arguments "{{ {frameId: frame.id} }}"
          :answer debugOnScopes
          :context "{{ {frameId: frame.id} }}"
          :refused ""}}
      {:run debugRefreshWatches}
      {:if "{{frame.inside && !empty(frame.relative)}}"
        :then [{:run openPath :with {:path "{{frame.relative}}" :label "{{reverse(split(frame.relative, '/'))[0]}}"}}
          {:run goToLine :with {:line "{{frame.line}}"}}]
        :else [{:if "{{contains(computed.docIds, state.debug.current.path)}}"
            :then [{:run showTab
                :with (node #"{{state.debug.current.path}}"
                  :label "{{find(state.docs, it.id == state.debug.current.path).label}}")}]
            :else [{:if "{{ask && (frame.reference > 0 || !empty(frame.path))}}"
                :then [{:set state.debug.outside :to "{{ {open: true, frame: frame} }}"}]}]}]}]
    :debugOnScopes [{:if "{{context.frameId == state.debug.frameId}}"
        :then [{:set state.debug.variables
            :to "{{map(slice(body.scopes ?? [], 0, 20), {path: 's' + index, depth: 0, name: text(it.name ?? ''), value: '', ref: it.variablesReference ?? 0, indexed: it.indexedVariables ?? 0, expanded: false, kind: 'scope', expensive: it.expensive ?? false})}}"}
          {:if "{{length(state.debug.variables) > 0 && !state.debug.variables[0].expensive}}"
            :then [{:run debugExpand :with {:path s0}}]}]}]
    :debugExpand [{:run debugExpandRow :with {:row "{{find(state.debug.variables, it.path == path)}}"}}]
    :debugExpandRow [{:if "{{row != null && row.kind == 'more'}}"
        :then [{:run debugLoadVariables
            :with {:parent "{{row.parent}}"
              :ref "{{row.ref}}"
              :depth "{{row.depth}}"
              :start "{{row.start}}"
              :indexed "{{row.indexed}}"
              :replace "{{row.path}}"}}]
        :else [{:if "{{row != null && row.ref > 0 && row.expanded}}"
            :then [{:set state.debug.variables
                :to "{{map(filter(state.debug.variables, slice(it.path, 0, length(row.path) + 1) != row.path + '/'), it.path == row.path ? merge(it, {expanded: false}) : it)}}"}]
            :else [{:if "{{row != null && row.ref > 0}}"
                :then [{:set state.debug.variables
                    :to "{{map(state.debug.variables, it.path == row.path ? merge(it, {expanded: true}) : it)}}"}
                  {:run debugLoadVariables
                    :with {:parent "{{row.path}}"
                      :ref "{{row.ref}}"
                      :depth "{{row.depth + 1}}"
                      :start 0
                      :indexed "{{row.indexed}}"
                      :replace ""}}]}]}]}]
    :debugLoadVariables [{:run debugRequest
        :with {:command variables
          :answer debugOnVariables
          :refused ""
          :arguments "{{merge({variablesReference: ref}, indexed > 0 ? {filter: 'indexed', start: start, count: min(100, indexed - start)} : {})}}"
          :context "{{ {parent: parent, ref: ref, depth: depth, start: start, indexed: indexed, replace: replace, frameId: state.debug.frameId} }}"}}]
    :debugOnVariables [{:if "{{context.frameId == state.debug.frameId && indexOf(map(state.debug.variables, it.path), empty(context.replace) ? context.parent : context.replace) >= 0}}"
        :then [{:run debugInsertRows
            :with {:at "{{indexOf(map(state.debug.variables, it.path), empty(context.replace) ? context.parent : context.replace) + (empty(context.replace) ? 1 : 0)}}"
              :skip "{{empty(context.replace) ? 0 : 1}}"
              :rows "{{concat(map(slice(body.variables ?? [], 0, max(0, 5000 - length(state.debug.variables))), {path: context.parent + '/' + (context.start + index), depth: context.depth, name: text(it.name ?? ''), value: text(it.value ?? ''), type: text(it.type ?? ''), ref: it.variablesReference ?? 0, indexed: it.indexedVariables ?? 0, expanded: false, kind: 'variable', container: context.ref}), context.indexed > 0 && context.start + 100 < context.indexed ? [{path: context.parent + '/more' + (context.start + 100), depth: context.depth, name: '', value: text(context.indexed - context.start - 100) + ' more', ref: context.ref, indexed: context.indexed, expanded: false, kind: 'more', parent: context.parent, start: context.start + 100}] : [])}}"}}]}]
    :debugInsertRows [{:set state.debug.variables
        :to "{{concat(slice(state.debug.variables, 0, at), rows, slice(state.debug.variables, at + skip))}}"}]
    :debugEditValue {:title "Set Value"
      :category Debug
      :icon pencil
      :enablement "{{state.debug.status == 'stopped'}}"
      :do [{:run debugEditValueOf
          :with {:row "{{find(state.debug.variables, it.path == state.debug.selectedVariable)}}"}}]}
    :debugEditValueOf [{:if "{{row != null && row.kind == 'variable' && (state.debug.capabilities.supportsSetVariable ?? false)}}"
        :then [{:set state.debug.setValue :to "{{ {open: true, path: row.path, name: row.name, value: row.value} }}"}]
        :else [{:run notify
            :with {:severity info :message "Select a variable: the debugger can set the value of variables only."}}]}]
    :debugSetValue [{:set state.debug.setValue.open :to false}
      {:run debugRequest
        :with {:command setVariable
          :answer debugOnSetVariable
          :refused ""
          :arguments "{{ {variablesReference: find(state.debug.variables, it.path == state.debug.setValue.path).container, name: state.debug.setValue.name, value: state.debug.setValue.value} }}"
          :context "{{ {path: state.debug.setValue.path} }}"}}]
    :debugOnSetVariable [{:set state.debug.variables
        :to "{{map(state.debug.variables, it.path == context.path ? merge(it, {value: text(body.value ?? it.value), type: text(body.type ?? it.type)}) : it)}}"}
      {:run debugRefreshWatches}]
    :debugAddWatch [{:if "{{!empty(trim(state.debug.watchDraft)) && length(state.debug.watches) < 100}}"
        :then [{:push state.debug.watches
            :value "{{ {id: 'w' + state.debug.watchNext, expression: trim(state.debug.watchDraft), value: '', error: ''} }}"}
          {:increment state.debug.watchNext :by 1}
          {:set state.debug.watchDraft :to ""}
          {:run debugRefreshWatches}]}]
    :debugToggleSection [{:note "section: variables, watch, stack or breakpoints. Collapses it, or expands it again"}
      {:set state.debug.collapsed
        :to "{{contains(state.debug.collapsed, section) ? filter(state.debug.collapsed, it != section) : concat(state.debug.collapsed, [section])}}"}]
    :debugRemoveWatch [{:set state.debug.watches
        :to "{{filter(state.debug.watches, it.id != state.debug.selectedWatch)}}"}]
    :debugRefreshWatches [{:if "{{state.debug.status == 'stopped' && state.debug.frameId != null}}"
        :then [{:forEach "{{state.debug.watches}}"
            :as watch
            :do [{:run debugRequest
                :with {:command evaluate
                  :answer debugOnWatch
                  :refused debugOnWatchError
                  :arguments "{{ {expression: watch.expression, frameId: state.debug.frameId, context: 'watch'} }}"
                  :context "{{ {id: watch.id} }}"}}]}]}]
    :debugOnWatch [{:set state.debug.watches
        :to "{{map(state.debug.watches, it.id == context.id ? merge(it, {value: text(body.result ?? ''), error: ''}) : it)}}"}]
    :debugOnWatchError [{:set state.debug.watches
        :to "{{map(state.debug.watches, it.id == context.id ? merge(it, {value: '', error: message}) : it)}}"}]
    :debugEvaluate [{:if "{{!empty(trim(state.debug.input))}}"
        :then [{:run debugPrint :with {:text "{{state.debug.input}}" :kind input}}
          {:set state.debug.history
            :to "{{slice(concat(filter(state.debug.history, it != state.debug.input), [state.debug.input]), -100)}}"}
          {:set state.debug.historyAt :to "{{length(state.debug.history)}}"}
          {:set state.debug.completions :to []}
          {:if "{{state.debug.status == 'inactive'}}"
            :then [{:run debugPrint :with {:text "Start a debug session to evaluate expressions." :kind error}}]
            :else [{:run debugRequest
                :with {:command evaluate
                  :answer debugOnEvaluate
                  :refused debugOnEvaluateError
                  :context {}
                  :arguments "{{merge({expression: state.debug.input, context: 'repl'}, state.debug.frameId == null ? {} : {frameId: state.debug.frameId})}}"}}]}
          {:set state.debug.input :to ""}]}]
    :debugOnEvaluate [{:run debugPrint :with {:text "{{text(body.result ?? '')}}" :kind result}}]
    :debugOnEvaluateError [{:run debugPrint :with {:text "{{message}}" :kind error}}]
    :debugHistoryBack [{:if "{{state.debug.historyAt > 0}}"
        :then [{:increment state.debug.historyAt :by -1}
          {:set state.debug.input :to "{{state.debug.history[state.debug.historyAt]}}"}]}]
    :debugHistoryForward [{:if "{{state.debug.historyAt < length(state.debug.history)}}"
        :then [{:increment state.debug.historyAt :by 1}]}
      {:set state.debug.input :to "{{state.debug.history[state.debug.historyAt] ?? ''}}"}]
    :debugComplete [{:if "{{state.debug.status != 'inactive' && (state.debug.capabilities.supportsCompletionsRequest ?? false)}}"
        :then [{:run debugRequest
            :with {:command completions
              :answer debugOnCompletions
              :refused debugIgnore
              :context "{{ {input: state.debug.input} }}"
              :arguments "{{merge({text: state.debug.input, column: length(state.debug.input) + 1}, state.debug.frameId == null ? {} : {frameId: state.debug.frameId})}}"}}]}]
    :debugOnCompletions [{:if "{{context.input == state.debug.input}}"
        :then [{:set state.debug.completions :to "{{slice(map(body.targets ?? [], text(it.text ?? it.label)), 0, 50)}}"}
          {:if "{{length(state.debug.completions) == 1}}"
            :then [{:set state.debug.input
                :to "{{slice(state.debug.input, 0, length(state.debug.input) - length(reverse(split(state.debug.input, ' '))[0])) + state.debug.completions[0]}}"}
              {:set state.debug.completions :to []}]}]}]
    :debugClearConsole {:title "Clear Console" :category Debug :icon eraser :do [{:set state.debug.console :to []}]}
    :showDebugConsole {:title "Show Debug Console"
      :category View
      :icon terminal
      :do [{:set state.layout.panel :to true} {:set state.panelTab :to debugConsole}]}
    :debugHover [{:if "{{state.debug.status != 'stopped' || empty(event.word) || event.line == null}}"
        :then [{:set state.debug.hover :to {:path "" :word "" :line 0 :value ""}}]
        :else [{:if "{{event.word != state.debug.hover.word || event.line != state.debug.hover.line}}"
            :then [{:set state.debug.hover :to "{{ {path: path, word: event.word, line: event.line, value: ''} }}"}
              {:run debugRequest
                :with {:command evaluate
                  :answer debugOnHover
                  :refused debugIgnore
                  :context "{{ {word: event.word, line: event.line} }}"
                  :arguments "{{ {expression: event.word, frameId: state.debug.frameId, context: 'hover'} }}"}}]}]}]
    :debugOnHover [{:if "{{context.word == state.debug.hover.word && context.line == state.debug.hover.line}}"
        :then [{:set state.debug.hover.value :to "{{slice(text(body.result ?? ''), 0, 200)}}"}]}]
    :debugToggleBreakpoint [{:if "{{find(state.debug.breakpoints, it.path == path && it.line == line) != null}}"
        :then [{:set state.debug.breakpoints
            :to "{{filter(state.debug.breakpoints, !(it.path == path && it.line == line))}}"}]
        :else [{:run debugAddBreakpoint}]}
      {:run debugSendBreakpoints :with {:path "{{path}}"}}]
    :debugAddBreakpoint [{:if "{{length(state.debug.breakpoints) < 10000}}"
        :then [{:push state.debug.breakpoints
            :value "{{ {id: 'b' + state.debug.breakpointNext, path: path, line: line, enabled: true, condition: '', hitCondition: '', logMessage: '', verified: false, message: '', adapterId: null} }}"}
          {:increment state.debug.breakpointNext :by 1}]}]
    :debugToggleBreakpointAtCaret {:title "Toggle Breakpoint"
      :category Debug
      :icon circle-dot
      :enablement "{{computed.activeDoc != null && computed.activeDoc.kind == 'file'}}"
      :do [{:if "{{computed.activeDoc != null && computed.activeDoc.kind == 'file'}}"
          :then [{:run debugToggleBreakpoint :with {:path "{{computed.activeDoc.id}}" :line "{{state.caret.line}}"}}]}]}
    :debugGutterClick [{:if "{{event.shift}}"
        :then [{:run debugEditBreakpointAt :with {:path "{{path}}" :line "{{event.line + 1}}"}}]
        :else [{:if "{{event.alt}}"
            :then [{:set state.debug.breakpoints
                :to "{{map(state.debug.breakpoints, it.path == path && it.line == event.line + 1 ? merge(it, {enabled: !it.enabled}) : it)}}"}
              {:run debugSendBreakpoints :with {:path "{{path}}"}}]
            :else [{:run debugToggleBreakpoint :with {:path "{{path}}" :line "{{event.line + 1}}"}}]}]}]
    :debugEditBreakpointAt [{:if "{{find(state.debug.breakpoints, it.path == path && it.line == line) == null}}"
        :then [{:run debugAddBreakpoint}]}
      {:run debugEditBreakpoint
        :with (node #"{{find(state.debug.breakpoints, it.path == path && it.line == line).id ?? ''}}")}]
    :debugEditBreakpoint [{:if "{{find(state.debug.breakpoints, it.id == id) != null}}"
        :then [{:set state.debug.edit
            :to "{{merge({open: true, id: id}, find(state.debug.breakpoints, it.id == id) ?? {})}}"}]}]
    :debugEditSelectedBreakpoint {:title "Edit Breakpoint"
      :category Debug
      :icon pencil
      :enablement "{{state.debug.selectedBreakpoint != null}}"
      :do [{:run debugEditBreakpoint :with (node #"{{state.debug.selectedBreakpoint ?? ''}}")}]}
    :debugSaveBreakpoint [{:set state.debug.edit.open :to false}
      {:set state.debug.breakpoints
        :to "{{map(state.debug.breakpoints, it.id == state.debug.edit.id ? merge(it, {condition: slice(trim(state.debug.edit.condition), 0, 1000), hitCondition: slice(trim(state.debug.edit.hitCondition), 0, 1000), logMessage: slice(state.debug.edit.logMessage, 0, 1000)}) : it)}}"}
      {:run debugSendBreakpoints
        :with {:path "{{find(state.debug.breakpoints, it.id == state.debug.edit.id).path ?? ''}}"}}]
    :debugApplyBreakpoint [{:run debugToggleBreakpoint :with {:path "{{event.path}}" :line "{{event.line}}"}}
      {:set state.debug.breakpoints
        :to "{{map(state.debug.breakpoints, it.path == event.path && it.line == event.line ? merge(it, {condition: event.condition ?? '', hitCondition: event.hitCondition ?? '', logMessage: event.logMessage ?? ''}) : it)}}"}]
    :debugToggleEnabled [{:set state.debug.breakpoints
        :to "{{map(state.debug.breakpoints, it.id == id ? merge(it, {enabled: !it.enabled}) : it)}}"}
      {:run debugSendBreakpoints :with {:path "{{find(state.debug.breakpoints, it.id == id).path ?? ''}}"}}]
    :debugToggleSelectedEnabled [{:if "{{state.debug.selectedBreakpoint != null}}"
        :then [{:run debugToggleEnabled :with (node #"{{state.debug.selectedBreakpoint}}")}]}]
    :debugOpenBreakpoint [{:if "{{find(state.debug.breakpoints, it.id == event.id) != null}}"
        :then [{:run debugGoTo :with {:breakpoint "{{find(state.debug.breakpoints, it.id == event.id)}}"}}]}]
    :debugGoTo [{:run openPath :with {:path "{{breakpoint.path}}" :label "{{reverse(split(breakpoint.path, '/'))[0]}}"}}
      {:run goToLine :with {:line "{{breakpoint.line}}"}}]
    :debugRemoveBreakpoint [{:if "{{state.debug.selectedBreakpoint != null}}"
        :then [{:run debugRemoveBreakpointOf
            :with {:breakpoint "{{find(state.debug.breakpoints, it.id == state.debug.selectedBreakpoint)}}"}}]}]
    :debugRemoveBreakpointOf [{:if "{{breakpoint != null}}"
        :then [{:set state.debug.breakpoints :to "{{filter(state.debug.breakpoints, it.id != breakpoint.id)}}"}
          {:set state.debug.selectedBreakpoint :to nil}
          {:run debugSendBreakpoints :with {:path "{{breakpoint.path}}"}}]}]
    :debugRemoveAllBreakpoints {:title "Remove All Breakpoints"
      :category Debug
      :icon list-x
      :do [{:forEach "{{filter(map(state.debug.breakpoints, it.path), indexOf(map(state.debug.breakpoints, it.path), it) == index)}}"
          :as path
          :do [{:set state.debug.breakpoints :to "{{filter(state.debug.breakpoints, it.path != path)}}"}
            {:run debugSendBreakpoints :with {:path "{{path}}"}}]}
        {:set state.debug.selectedBreakpoint :to nil}]}
    :debugToggleFilter [{:set state.debug.exceptionFilters
        :to "{{map(state.debug.exceptionFilters, it.filter == filter ? merge(it, {enabled: !it.enabled}) : it)}}"}
      {:set "state.debug.filterChoices[filter]"
        :to "{{find(state.debug.exceptionFilters, it.filter == filter).enabled}}"}
      {:run debugSendFilters}]
    :"debugLldb:runInTerminal" [{:run debugRunInTerminal}]
    :"debugPython:runInTerminal" [{:run debugRunInTerminal}]
    :debugRunInTerminal [{:note "The adapter's runInTerminal: its program runs on the debug terminal, started in its cwd (the pty service keeps it inside the folder), as `exec` and the line shellLine() quotes (null if a word isn't plain text or the line is too long), typed into the POSIX shell `sh`, never the person's login shell, which may be one (fish, csh, cmd.exe) where single quotes don't protect a word."}
      {:run debugRunLine
        :with {:named "{{type(event.args) == 'list' && !empty(event.args)}}"
          :line "{{type(event.args) == 'list' ? shellLine(event.args, type(event.env) == 'object' ? event.env : null) : null}}"
          :folder "{{type(event.cwd) == 'string' ? event.cwd : ''}}"}}]
    :debugRunLine [{:if "{{!named}}"
        :then [{:respond nil :error "the adapter named no program to run"}]
        :else [{:if "{{line == null}}"
            :then [{:respond nil :error "the adapter's command line is not plain text, or is too long"}]
            :else [{:set state.debug.terminalSent :to false}
              (node :invoke debugTerminal.open
                #debug-terminal
                :timeout 600000
                :with {:cols 120 :rows 30 :folder "{{folder}}"}
                :onEvent [{:if "{{event.session != null && !state.debug.terminalSent}}"
                    :then [{:set state.debug.terminalSent :to true}
                      {:invoke debugTerminal.write
                        :with {:session "{{event.session}}" :data "{{'exec ' + line + '\n'}}"}
                        :onError [{:respond nil :error "{{error.message}}"}]
                        :onDone [{:respond {}}]}]}
                  {:if "{{event.data != null}}"
                    :then [{:run debugOutput :with {:body "{{ {category: 'stdout', output: event.data} }}"}}]}]
                :onError [{:if "{{!state.debug.terminalSent}}" :then [{:respond nil :error "{{error.message}}"}]}])]}]}]}
  :keys {:f5 debugContinueOrStart
    :"shift+f5" debugStop
    :"mod+shift+f5" debugRestart
    :f6 debugPause
    :f9 debugToggleBreakpointAtCaret
    :f10 debugStepOver
    :f11 debugStepInto
    :"shift+f11" debugStepOut
    :"mod+shift+d" showDebug}
  :menus {:view {:debug {:run showDebug :group "1_views" :order 3}
      :debugConsole {:run showDebugConsole :group "3_panels" :order 5}}}}
