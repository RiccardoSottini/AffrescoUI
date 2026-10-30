{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "The debugger's views (debug.json has the logic; the source page's actions are here): the Run and Debug side bar (configurations, variables, watches, the call stack and the breakpoints), the Debug Console panel, the floating toolbar and the dialogs, and the read-only page for a source outside the open folder."
  :actions {:debugOpenOutside [{:if "{{state.debug.outside.frame != null}}"
        :then [{:run debugRequest
            :with {:command source
              :answer debugOnSource
              :refused debugOnSourceRefused
              :context "{{ {frame: state.debug.outside.frame} }}"
              :arguments "{{ {source: merge(empty(state.debug.outside.frame.path) ? {} : {path: state.debug.outside.frame.path}, {sourceReference: state.debug.outside.frame.reference}), sourceReference: state.debug.outside.frame.reference} }}"}}]}]
    :debugOnSource [{:run debugShowSource
        :with (node #"{{'debug-source:' + (empty(context.frame.path) ? 'ref:' + context.frame.reference : context.frame.path)}}"
          :label "{{(empty(context.frame.path) ? context.frame.name : reverse(split(context.frame.path, '/'))[0]) + ' (read-only)'}}"
          :content "{{slice(text(body.content ?? ''), 0, 4194304)}}")}]
    :debugOnSourceRefused [{:run notify
        :with {:severity info :message "{{'The debugger has no source for this frame: ' + message}}"}}]
    :debugShowSource [{:set state.docs :to "{{filter(state.docs, it.id != id)}}"}
      {:push state.docs
        :value "{{ {id: id, label: label, kind: 'debugSource', text: content, dirty: false, language: ''} }}"}
      {:run showTab :with (node #"{{id}}" :label "{{label}}")}]
    :debugApplySource [{:run debugShowSource
        :with (node #"{{'debug-source:' + event.path}}" :label "{{event.label}}" :content "{{event.content}}")}]}
  :components {:forgeDebugView {:node (column #debug-view :width "100%" :grow 1 :color @color.sideBar :gap 2
        (row #debug-head :width "100%" :height 35 :alignItems center
          (label #debug-title :text Debugger :textColor @color.muted :grow 1 :height 35 :padding 12)
          (label #debug-start
            :icon play
            :padding 6
            :fitText true
            :cursor hand
            :textColor "{{state.debug.status == 'inactive' ? '@color.accent' : '@color.muted'}}"
            :tooltip "Start Debugging (F5)"
            :hover {:textColor @color.text}
            :onClick {:run debugContinueOrStart})
          (label #debug-reload
            :icon refresh-cw
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Reload .forge/launch.json"
            :hover {:textColor @color.text}
            :onClick {:run debugLoadConfigurations}))
        (column #debug-empty
          :when "{{length(state.debug.configurations) == 0 && empty(state.debug.error) && state.debug.status == 'inactive'}}"
          :width "100%"
          :height 124
          :gap 10
          (node #debug-empty-top :height 4)
          {:use forgeInset
            :height 28
            :children [(label #debug-empty-start
                :role button
                :label "Start Debugging"
                :text "Start Debugging"
                :grow 1
                :height 28
                :align center
                :radius 2
                :cursor hand
                :focusable true
                :color @color.accent
                :textColor "#ffffff"
                :onClick {:run debugContinueOrStart})]}
          (label #debug-empty-text
            :wrap true
            :width "100%"
            :height 70
            :padding 12
            :textColor @color.text
            :text "{{'To choose what runs, add a configuration of type lldb or python to ' + (empty(state.folder.root) ? 'the open folder' : 'this folder') + '\\'s .forge/launch.json.' + (state.debug.skipped > 0 ? ' ' + plural(state.debug.skipped, 'configuration') + ' Forge can\\'t run were left out.' : '')}}"))
        (label #debug-error
          :when "{{!empty(state.debug.error)}}"
          :icon circle-x
          :wrap true
          :width "100%"
          :padding 8
          :textColor @color.text
          :text "{{state.debug.error}}")
        (list #debug-configurations
          :when "{{length(state.debug.configurations) > 0}}"
          :width "100%"
          :height "{{min(4, length(state.debug.configurations)) * 22}}"
          :items "{{computed.debugConfigurationRows}}"
          :bind state.debug.selected
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onOpen {:run debugStart})
        (label #debug-status
          :when "{{state.debug.status != 'inactive'}}"
          :width "100%"
          :height 22
          :padding 8
          :icon bug
          :textColor "{{state.debug.status == 'stopped' ? '#e5c07b' : '@color.muted'}}"
          :text "{{state.debug.status == 'stopped' ? 'Paused on ' + state.debug.reason : state.debug.status == 'starting' ? 'Starting…' : 'Running'}}")
        (row #debug-variables-head :width "100%" :height 22 :alignItems center
          (label #debug-variables-title
            :role button
            :label Variables
            :icon "{{contains(state.debug.collapsed, 'variables') ? 'chevron-right' : 'chevron-down'}}"
            :text VARIABLES
            :cursor hand
            :focusable true
            :isOpen "{{!contains(state.debug.collapsed, 'variables')}}"
            :onClick {:run debugToggleSection :with {:section variables}}
            :textColor @color.text
            :grow 1
            :height 22
            :padding 4)
          (label #debug-set-value
            :icon pencil
            :padding 4
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Set Value"
            :hover {:textColor @color.text}
            :onClick {:run debugEditValue}))
        (list #debug-variables
          :when "{{!contains(state.debug.collapsed, 'variables')}}"
          :width "100%"
          :grow 2
          :font code
          :items "{{computed.debugVariableRows}}"
          :bind state.debug.selectedVariable
          :rowHeight 20
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onOpen {:run debugExpand :with {:path "{{event.id}}"}})
        (row #debug-watch-head :width "100%" :height 22 :alignItems center
          (label #debug-watch-title
            :role button
            :label Watch
            :icon "{{contains(state.debug.collapsed, 'watch') ? 'chevron-right' : 'chevron-down'}}"
            :text WATCH
            :cursor hand
            :focusable true
            :isOpen "{{!contains(state.debug.collapsed, 'watch')}}"
            :onClick {:run debugToggleSection :with {:section watch}}
            :textColor @color.text
            :grow 1
            :height 22
            :padding 4)
          (label #debug-watch-remove
            :icon x
            :padding 4
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Remove the selected expression"
            :hover {:textColor @color.text}
            :onClick {:run debugRemoveWatch}))
        (textarea #debug-watch-input
          :when "{{!contains(state.debug.collapsed, 'watch')}}"
          :width "100%"
          :height 24
          :padding 5
          :color @color.input
          :textColor @color.text
          :placeholderColor @color.muted
          :bind state.debug.watchDraft
          :placeholder "Expression to watch (Enter)"
          :onSubmit {:run debugAddWatch})
        (list #debug-watches
          :when "{{!contains(state.debug.collapsed, 'watch')}}"
          :width "100%"
          :grow 1
          :font code
          :items "{{computed.debugWatchRows}}"
          :bind state.debug.selectedWatch
          :rowHeight 20
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent)
        (label #debug-stack-title
          :role button
          :label "Call Stack"
          :icon "{{contains(state.debug.collapsed, 'stack') ? 'chevron-right' : 'chevron-down'}}"
          :text "CALL STACK"
          :cursor hand
          :focusable true
          :isOpen "{{!contains(state.debug.collapsed, 'stack')}}"
          :onClick {:run debugToggleSection :with {:section stack}}
          :width "100%"
          :height 22
          :textColor @color.text
          :padding 4)
        (list #debug-stack
          :when "{{!contains(state.debug.collapsed, 'stack')}}"
          :width "100%"
          :grow 1
          :items "{{computed.debugStackRows}}"
          :bind state.debug.selectedStack
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onChange {:run debugSelectStack :with (node #"{{event.value ?? ''}}")})
        (row #debug-breakpoints-head :width "100%" :height 22 :alignItems center
          (label #debug-breakpoints-title
            :role button
            :label Breakpoints
            :icon "{{contains(state.debug.collapsed, 'breakpoints') ? 'chevron-right' : 'chevron-down'}}"
            :text BREAKPOINTS
            :cursor hand
            :focusable true
            :isOpen "{{!contains(state.debug.collapsed, 'breakpoints')}}"
            :onClick {:run debugToggleSection :with {:section breakpoints}}
            :textColor @color.text
            :grow 1
            :height 22
            :padding 4)
          (label #debug-breakpoint-edit
            :icon pencil
            :padding 4
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Edit Condition, Hit Count or Log Message"
            :hover {:textColor @color.text}
            :onClick {:run debugEditSelectedBreakpoint})
          (label #debug-breakpoint-enable
            :icon circle-off
            :padding 4
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Enable or Disable the selected breakpoint"
            :hover {:textColor @color.text}
            :onClick {:run debugToggleSelectedEnabled})
          (label #debug-breakpoint-remove
            :icon x
            :padding 4
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Remove the selected breakpoint"
            :hover {:textColor @color.text}
            :onClick {:run debugRemoveBreakpoint})
          (label #debug-breakpoint-remove-all
            :icon list-x
            :padding 4
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Remove All Breakpoints"
            :hover {:textColor @color.text}
            :onClick {:run debugRemoveAllBreakpoints}))
        (label #debug-filter
          :when "{{!contains(state.debug.collapsed, 'breakpoints')}}"
          :repeat {:for f :in state.debug.exceptionFilters :key "{{f.filter}}"}
          :width "100%"
          :height 22
          :padding 8
          :cursor hand
          :textColor @color.text
          :icon "{{f.enabled ? 'square-check' : 'square'}}"
          :text "{{f.label}}"
          :hover {:color @color.hover}
          :onClick {:run debugToggleFilter :with {:filter "{{f.filter}}"}})
        (list #debug-breakpoints
          :when "{{!contains(state.debug.collapsed, 'breakpoints')}}"
          :width "100%"
          :grow 1
          :items "{{computed.debugBreakpointRows}}"
          :bind state.debug.selectedBreakpoint
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onOpen {:run debugOpenBreakpoint}))}
    :forgeDebugConsole {:node (column #debug-console :width "100%" :grow 1 :color @color.panel
        (list #debug-console-lines
          :width "100%"
          :grow 1
          :font code
          :items "{{computed.debugConsoleRows}}"
          :rowHeight 18
          :color @color.panel
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent)
        (label #debug-completions
          :when "{{length(state.debug.completions) > 1}}"
          :width "100%"
          :height 20
          :padding 4
          :font code
          :textColor @color.muted
          :text "{{join(state.debug.completions, '  ')}}")
        (row #debug-console-input-row :width "100%" :height 26 :alignItems center
          (label #debug-console-prompt :icon chevron-right :padding 4 :fitText true :textColor @color.info)
          (textarea #debug-console-input
            :grow 1
            :height 24
            :padding 5
            :font code
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :bind state.debug.input
            :placeholder "{{state.debug.status == 'inactive' ? 'Start a debug session to evaluate expressions' : 'Evaluate (Enter; Up and Down for history; Tab to complete)'}}"
            :onSubmit {:run debugEvaluate}
            :keys {:up debugHistoryBack :down debugHistoryForward :tab debugComplete})
          (label #debug-console-clear
            :icon trash-2
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Clear Console"
            :hover {:textColor @color.text}
            :onClick {:run debugClearConsole})))}
    :forgeDebugOverlays {:node (column #debug-overlays
        (row #debug-toolbar
          :color @color.sideBar
          :borderWidth 1
          :borderColor @color.border
          :radius 4
          :gap 2
          :height 30
          :anchors {:top 40 :right 220}
          :overlay {:placement window :open "{{state.debug.status != 'inactive'}}"}
          (label #debug-toolbar-continue
            :icon "{{state.debug.status == 'stopped' ? 'play' : 'pause'}}"
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.accent
            :tooltip "{{state.debug.status == 'stopped' ? 'Continue (F5)' : 'Pause (F6)'}}"
            :hover {:color @color.hover}
            :onClick {:run "{{state.debug.status == 'stopped' ? 'debugContinue' : 'debugPause'}}"})
          (label #debug-toolbar-over
            :icon redo-dot
            :padding 6
            :fitText true
            :cursor hand
            :textColor "{{state.debug.status == 'stopped' ? '@color.info' : '@color.muted'}}"
            :tooltip "Step Over (F10)"
            :hover {:color @color.hover}
            :onClick {:run debugStepOver})
          (label #debug-toolbar-into
            :icon arrow-down-to-dot
            :padding 6
            :fitText true
            :cursor hand
            :textColor "{{state.debug.status == 'stopped' ? '@color.info' : '@color.muted'}}"
            :tooltip "Step Into (F11)"
            :hover {:color @color.hover}
            :onClick {:run debugStepInto})
          (label #debug-toolbar-out
            :icon arrow-up-from-dot
            :padding 6
            :fitText true
            :cursor hand
            :textColor "{{state.debug.status == 'stopped' ? '@color.info' : '@color.muted'}}"
            :tooltip "Step Out (Shift+F11)"
            :hover {:color @color.hover}
            :onClick {:run debugStepOut})
          (label #debug-toolbar-restart
            :icon rotate-ccw
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.accent
            :tooltip "Restart (Cmd+Shift+F5)"
            :hover {:color @color.hover}
            :onClick {:run debugRestart})
          (label #debug-toolbar-stop
            :icon square
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.error
            :tooltip "Stop (Shift+F5)"
            :hover {:color @color.hover}
            :onClick {:run debugStop}))
        (node :use dialog
          #debug-edit-dialog
          :with {:open state.debug.edit.open
            :height 230
            :title "Edit Breakpoint"
            :message ""
            :buttons [{:title OK :run debugSaveBreakpoint} {:title Cancel}]}
          (textarea #debug-edit-condition
            :bind state.debug.edit.condition
            :autofocus true
            :anchors {:left 20 :right 20 :top 48}
            :height 28
            :padding 7
            :font code
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :placeholder "Condition: break when it is true"
            :onSubmit {:run debugSaveBreakpoint})
          (textarea #debug-edit-hits
            :bind state.debug.edit.hitCondition
            :anchors {:left 20 :right 20 :top 84}
            :height 28
            :padding 7
            :font code
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :placeholder "Hit count: break when it is reached"
            :onSubmit {:run debugSaveBreakpoint})
          (textarea #debug-edit-log
            :bind state.debug.edit.logMessage
            :anchors {:left 20 :right 20 :top 120}
            :height 28
            :padding 7
            :font code
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :placeholder "Log message: log it instead of breaking; {expression} inserts a value"
            :onSubmit {:run debugSaveBreakpoint}))
        (node :use dialog
          #debug-value-dialog
          :with {:open state.debug.setValue.open
            :height 150
            :title "{{'Set Value of ' + state.debug.setValue.name}}"
            :message ""
            :buttons [{:title Set :run debugSetValue} {:title Cancel}]}
          (textarea #debug-value
            :bind state.debug.setValue.value
            :autofocus true
            :anchors {:left 20 :right 20 :top 48}
            :height 28
            :padding 7
            :font code
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :placeholder "New value"
            :onSubmit {:run debugSetValue}))
        (node :use dialog
          #debug-outside-dialog
          :with {:open state.debug.outside.open
            :title "Open a source outside the folder?"
            :message "{{'The debugger stopped in ' + (state.debug.outside.frame.path || state.debug.outside.frame.name || 'a frame') + ', outside the open folder. Forge asks the debugger for its text and shows it read-only.'}}"
            :buttons [{:title Open :run debugOpenOutside} {:title Cancel}]}))}
    :forgeDebugSource {:params {:doc {}}
      :node (column #debug-source :width "100%" :grow 1 :color @color.editor
        (label #debug-source-note
          :width "100%"
          :height 22
          :padding 6
          :icon file-code
          :textColor @color.muted
          :text "Read-only: the debugger's text of a source outside the open folder")
        (list #debug-source-lines
          :width "100%"
          :grow 1
          :font code
          :rowHeight 18
          :items "{{map(slice(split(doc.text ?? '', '\n'), 0, 20000), {id: text(index + 1), label: pad(text(index + 1), 6) + (state.debug.current.path == doc.id && state.debug.current.line == index + 1 ? ' → ' : '   ') + it})}}"
          :color @color.editor
          :textColor @color.editorText
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent))}}}
