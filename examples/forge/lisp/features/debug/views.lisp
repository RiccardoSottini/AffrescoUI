;;; views.lisp --- the debugger's views: Run and Debug in the side bar, the debug console, the toolbar over the
;;; editors, its dialogs, and a source outside the folder

;;; Run and Debug

(defcomponent forgeDebugView "Run and Debug: the configuration and Start, the status, then the variables, watches, call stack and breakpoints, each a section that folds."
  {}
  `(column#debug-view :width "100%" :grow 1 :color @color.sideBar :gap 2
     ~(debug-head)
     ~(debug-empty)
     (label#debug-error :when ~(not (empty? state.debug.error))
       :icon circle-x
       :wrap true
       :width "100%"
       :padding 8
       :textColor @color.text
       :text ~state.debug.error)
     ~(debug-configurations)
     ~(debug-status)
     ~(debug-variables-head)
     ~(debug-variables)
     ~(debug-watch-head)
     ~(debug-watch-input)
     ~(debug-watches)
     ~(debug-stack-title)
     ~(debug-stack)
     ~(debug-breakpoints-head)
     ~(repeat [f state.debug.exceptionFilters :key f.filter]
       `(label#debug-filter :when ~(not (contains state.debug.collapsed "breakpoints"))
          :width "100%"
          :height 22
          :padding 8
          :cursor hand
          :textColor @color.text
          :icon ~(if f.enabled "square-check" "square")
          :text ~f.label
          :hover {:color @color.hover}
          :onClick ~(handler debugToggleFilter f.filter)))
     ~(debug-breakpoints)))

(defview debug-head "The view's title, and Start and Reload." []
  `(row#debug-head :width "100%" :height 35 :alignItems center
     (label#debug-title :text ~(t "debug-title") :textColor @color.muted :grow 1 :height 35 :padding 12)
     (label#debug-start :icon play
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if (= state.debug.status "inactive") "@color.accent" "@color.muted")
       :tooltip ~(t "debug-start.tip")
       :hover {:textColor @color.text}
       :onClick debugContinueOrStart)
     (label#debug-reload :icon refresh-cw
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-reload.tip")
       :hover {:textColor @color.text}
       :onClick debugLoadConfigurations)))

(defview debug-configurations "The launch configurations, one chosen, and Start." []
  `(list#debug-configurations :when ~(> (count state.debug.configurations) 0)
     :width "100%"
     :height ~(* (min 4 (count state.debug.configurations)) 22)
     :items ~computed.debugConfigurationRows
     :bind state.debug.selected
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onOpen debugStart))

(defview debug-empty "No configuration: how to add one, and how many were left out." []
  `(column#debug-empty :when ~(and (= (count state.debug.configurations) 0)
                                   (empty? state.debug.error)
                                   (= state.debug.status "inactive"))
     :width "100%"
     :height 124
     :gap 10
     (node#debug-empty-top :height 4)
     (node :use forgeInset :height 28
       (label#debug-empty-start :role button
         :label ~(t "debug-empty-start.label")
         :text ~(t "debug-empty-start")
         :grow 1
         :height 28
         :align center
         :radius 2
         :cursor hand
         :focusable true
         :color @color.accent
         :textColor "#ffffff"
         :onClick debugContinueOrStart))
     (label#debug-empty-text :wrap true
       :width "100%"
       :height 70
       :padding 12
       :textColor @color.text
       :text ~(str (if (empty? state.folder.root) (t "debug-empty.noFolder") (t "debug-empty.folder"))
                   (if (> state.debug.skipped 0) (t "debug-empty.skipped" {:n state.debug.skipped}) "")))))

(defview debug-status "Whether the program is running, starting or paused, and why." []
  `(label#debug-status :when ~(not= state.debug.status "inactive")
     :width "100%"
     :height 22
     :padding 8
     :icon bug
     :textColor ~(if (= state.debug.status "stopped") "#e5c07b" "@color.muted")
     :text ~(cond (= state.debug.status "stopped") (t "debug-status.paused" {:reason state.debug.reason})
                  (= state.debug.status "starting") (t "debug-status.starting")
                  :else (t "debug-status.running"))))

(defview debug-variables-head "The variables' title, and Set Value." []
  `(row#debug-variables-head :width "100%" :height 22 :alignItems center
     (label#debug-variables-title :role button
       :label ~(t "debug-variables-title.label")
       :icon ~(if (contains state.debug.collapsed "variables") "chevron-right" "chevron-down")
       :text ~(t "debug-variables-title")
       :cursor hand
       :focusable true
       :isOpen ~(not (contains state.debug.collapsed "variables"))
       :onClick ~(handler debugToggleSection "variables")
       :textColor @color.text
       :grow 1
       :height 22
       :padding 4)
     (label#debug-set-value :icon pencil
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-set-value.tip")
       :hover {:textColor @color.text}
       :onClick debugEditValue)))

(defview debug-variables "The variables of the frame shown, a tree that reads a row's children as it opens." []
  `(list#debug-variables :when ~(not (contains state.debug.collapsed "variables"))
     :width "100%"
     :grow 2
     :font code
     :items ~computed.debugVariableRows
     :bind state.debug.selectedVariable
     :rowHeight 20
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onOpen ~(handler debugExpand event.id)))

(defview debug-watch-head "The watches' title, and Remove." []
  `(row#debug-watch-head :width "100%" :height 22 :alignItems center
     (label#debug-watch-title :role button
       :label ~(t "debug-watch-title.label")
       :icon ~(if (contains state.debug.collapsed "watch") "chevron-right" "chevron-down")
       :text ~(t "debug-watch-title")
       :cursor hand
       :focusable true
       :isOpen ~(not (contains state.debug.collapsed "watch"))
       :onClick ~(handler debugToggleSection "watch")
       :textColor @color.text
       :grow 1
       :height 22
       :padding 4)
     (label#debug-watch-remove :icon x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-watch-remove.tip")
       :hover {:textColor @color.text}
       :onClick debugRemoveWatch)))

(defview debug-watch-input "The expression to watch: Enter adds it." []
  `(textarea#debug-watch-input :when ~(not (contains state.debug.collapsed "watch"))
     :width "100%"
     :height 24
     :padding 5
     :color @color.input
     :textColor @color.text
     :placeholderColor @color.muted
     :bind state.debug.watchDraft
     :placeholder ~(t "debug-watch-input.placeholder")
     :onSubmit debugAddWatch))

(defview debug-watches "The watches, each its value in the frame shown." []
  `(list#debug-watches :when ~(not (contains state.debug.collapsed "watch"))
     :width "100%"
     :grow 1
     :font code
     :items ~computed.debugWatchRows
     :bind state.debug.selectedWatch
     :rowHeight 20
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent))

(defview debug-stack-title "The call stack's title." []
  `(label#debug-stack-title :role button
     :label ~(t "debug-stack-title.label")
     :icon ~(if (contains state.debug.collapsed "stack") "chevron-right" "chevron-down")
     :text ~(t "debug-stack-title")
     :cursor hand
     :focusable true
     :isOpen ~(not (contains state.debug.collapsed "stack"))
     :onClick ~(handler debugToggleSection "stack")
     :width "100%"
     :height 22
     :textColor @color.text
     :padding 4))

(defview debug-stack "The call stack: threads, and the stopped one's frames; a row shows its frame." []
  `(list#debug-stack :when ~(not (contains state.debug.collapsed "stack"))
     :width "100%"
     :grow 1
     :items ~computed.debugStackRows
     :bind state.debug.selectedStack
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onChange ~(handler debugSelectStack (or-else event.value ""))))

(defview debug-breakpoints-head "The breakpoints' title, and Edit, Enable, Remove and Remove All." []
  `(row#debug-breakpoints-head :width "100%" :height 22 :alignItems center
     ~(debug-breakpoints-title)
     (label#debug-breakpoint-edit :icon pencil
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-edit.tip")
       :hover {:textColor @color.text}
       :onClick debugEditSelectedBreakpoint)
     (label#debug-breakpoint-enable :icon circle-off
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-enable.tip")
       :hover {:textColor @color.text}
       :onClick debugToggleSelectedEnabled)
     (label#debug-breakpoint-remove :icon x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-remove.tip")
       :hover {:textColor @color.text}
       :onClick debugRemoveBreakpoint)
     (label#debug-breakpoint-remove-all :icon list-x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-remove-all.tip")
       :hover {:textColor @color.text}
       :onClick debugRemoveAllBreakpoints)))

(defview debug-breakpoints-title "The breakpoints' title, which folds the section." []
  `(label#debug-breakpoints-title :role button
     :label ~(t "debug-breakpoints-title.label")
     :icon ~(if (contains state.debug.collapsed "breakpoints") "chevron-right" "chevron-down")
     :text ~(t "debug-breakpoints-title")
     :cursor hand
     :focusable true
     :isOpen ~(not (contains state.debug.collapsed "breakpoints"))
     :onClick ~(handler debugToggleSection "breakpoints")
     :textColor @color.text
     :grow 1
     :height 22
     :padding 4))

(defview debug-breakpoints "The breakpoints: a row opens its place." []
  `(list#debug-breakpoints :when ~(not (contains state.debug.collapsed "breakpoints"))
     :width "100%"
     :grow 1
     :items ~computed.debugBreakpointRows
     :bind state.debug.selectedBreakpoint
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onOpen debugOpenBreakpoint))

;;; The debug console

(defcomponent forgeDebugConsole "The debug console: what the program printed and the answers, and the input."
  {}
  `(column#debug-console :width "100%" :grow 1 :color @color.panel
     (list#debug-console-lines :width "100%"
       :grow 1
       :font code
       :items ~computed.debugConsoleRows
       :rowHeight 18
       :color @color.panel
       :textColor @color.text
       :selectedTextColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent)
     (label#debug-completions :when ~(> (count state.debug.completions) 1)
       :width "100%"
       :height 20
       :padding 4
       :font code
       :textColor @color.muted
       :text ~(join "  " state.debug.completions))
     ~(debug-console-input-row)))

(defview debug-console-input-row "The console's input: Enter evaluates it, Up and Down walk the history, Tab completes."
  []
  `(row#debug-console-input-row :width "100%" :height 26 :alignItems center
     (label#debug-console-prompt :icon chevron-right :padding 4 :fitText true :textColor @color.info)
     (textarea#debug-console-input :grow 1
       :height 24
       :padding 5
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :bind state.debug.input
       :placeholder ~(if (= state.debug.status "inactive") (t "debug-console.inactive") (t "debug-console.placeholder"))
       :onSubmit debugEvaluate
       :keys {:up debugHistoryBack :down debugHistoryForward :tab debugComplete})
     (label#debug-console-clear :icon trash-2
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-console-clear.tip")
       :hover {:textColor @color.text}
       :onClick debugClearConsole)))

;;; Over the workbench

(defcomponent forgeDebugOverlays "What the debugger shows over the workbench: the toolbar and its dialogs."
  {}
  `(column#debug-overlays
     ~(debug-toolbar)
     ~(debug-edit-dialog)
     ~(debug-value-dialog)
     (node#debug-outside-dialog :use dialog
       :with {:open state.debug.outside.open
              :title ~(t "debug-outside-dialog.title")
              :message ~(t "debug-outside-dialog.message"
                           {:frame (or state.debug.outside.frame.path state.debug.outside.frame.name (t "debug.aFrame"))})
              :buttons [{:title ~(t "debug-outside-dialog.open") :run debugOpenOutside}
                        {:title ~(t "debug-outside-dialog.cancel")}]})))

(defview debug-toolbar "The toolbar over the editors while a session runs: Continue or Pause, Step Over, Into and Out, Restart, Stop."
  []
  `(row#debug-toolbar :color @color.sideBar
     :borderWidth 1
     :borderColor @color.border
     :radius 4
     :gap 2
     :height 30
     :anchors {:top 40 :right 220}
     :overlay {:placement window :open ~(not= state.debug.status "inactive")}
     ~(debug-toolbar-continue)
     (label#debug-toolbar-over :icon redo-dot
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if (= state.debug.status "stopped") "@color.info" "@color.muted")
       :tooltip ~(t "debug-toolbar-over.tip")
       :hover {:color @color.hover}
       :onClick debugStepOver)
     ~(debug-toolbar-into)
     ~(debug-toolbar-out)
     (label#debug-toolbar-restart :icon rotate-ccw
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.accent
       :tooltip ~(t "debug-toolbar-restart.tip")
       :hover {:color @color.hover}
       :onClick debugRestart)
     (label#debug-toolbar-stop :icon square
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.error
       :tooltip ~(t "debug-toolbar-stop.tip")
       :hover {:color @color.hover}
       :onClick debugStop)))

(defview debug-toolbar-continue "Continue while paused, Pause while running." []
  `(label#debug-toolbar-continue :icon ~(if (= state.debug.status "stopped") "play" "pause")
     :padding 6
     :fitText true
     :cursor hand
     :textColor @color.accent
     :tooltip ~(if (= state.debug.status "stopped") (t "debug-toolbar.continue") (t "debug-toolbar.pause"))
     :hover {:color @color.hover}
     :onClick {:run ~(if (= state.debug.status "stopped") "debugContinue" "debugPause")}))

(defview debug-toolbar-into "Step Into." []
  `(label#debug-toolbar-into :icon arrow-down-to-dot
     :padding 6
     :fitText true
     :cursor hand
     :textColor ~(if (= state.debug.status "stopped") "@color.info" "@color.muted")
     :tooltip ~(t "debug-toolbar-into.tip")
     :hover {:color @color.hover}
     :onClick debugStepInto))

(defview debug-toolbar-out "Step Out." []
  `(label#debug-toolbar-out :icon arrow-up-from-dot
     :padding 6
     :fitText true
     :cursor hand
     :textColor ~(if (= state.debug.status "stopped") "@color.info" "@color.muted")
     :tooltip ~(t "debug-toolbar-out.tip")
     :hover {:color @color.hover}
     :onClick debugStepOut))

(defview debug-edit-dialog "A breakpoint's condition, hit count and log message." []
  `(node#debug-edit-dialog :use dialog
     :with {:open state.debug.edit.open
            :height 230
            :title ~(t "command.debugEditSelectedBreakpoint")
            :message ""
            :buttons [{:title ~(t "debug-edit-dialog.ok") :run debugSaveBreakpoint}
                      {:title ~(t "debug-edit-dialog.cancel")}]}
     (textarea#debug-edit-condition :bind state.debug.edit.condition
       :autofocus true
       :anchors {:left 20 :right 20 :top 48}
       :height 28
       :padding 7
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "debug-edit-condition.placeholder")
       :onSubmit debugSaveBreakpoint)
     (textarea#debug-edit-hits :bind state.debug.edit.hitCondition
       :anchors {:left 20 :right 20 :top 84}
       :height 28
       :padding 7
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "debug-edit-hits.placeholder")
       :onSubmit debugSaveBreakpoint)
     (textarea#debug-edit-log :bind state.debug.edit.logMessage
       :anchors {:left 20 :right 20 :top 120}
       :height 28
       :padding 7
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "debug-edit-log.placeholder")
       :onSubmit debugSaveBreakpoint)))

(defview debug-value-dialog "A variable's new value." []
  `(node#debug-value-dialog :use dialog
     :with {:open state.debug.setValue.open
            :height 150
            :title ~(t "debug-value-dialog.title" {:name state.debug.setValue.name})
            :message ""
            :buttons [{:title ~(t "debug-value-dialog.set") :run debugSetValue} {:title ~(t "debug-value-dialog.cancel")}]}
     (textarea#debug-value :bind state.debug.setValue.value
       :autofocus true
       :anchors {:left 20 :right 20 :top 48}
       :height 28
       :padding 7
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "debug-value.placeholder")
       :onSubmit debugSetValue)))

;;; A source outside the folder

(defcomponent forgeDebugSource "A source outside the folder, read-only, its lines numbered: `doc` the open document."
  {:doc {}}
  `(column#debug-source :width "100%" :grow 1 :color @color.editor
     (label#debug-source-note :width "100%"
       :height 22
       :padding 6
       :icon file-code
       :textColor @color.muted
       :text ~(t "debug-source-note"))
     ~(debug-source-lines doc)))

(defview debug-source-lines "A debugged source's lines, numbered, as a list." [doc]
  `(list#debug-source-lines :width "100%"
     :grow 1
     :font code
     :rowHeight 18
     :items ~(map-indexed (fn [index it]
                            {:id (text (+ index 1))
                             :label (+ (pad (text (+ index 1)) 6)
                                       (if (and (= state.debug.current.path doc.id)
                                                (= state.debug.current.line (+ index 1)))
                                           " → "
                                           "   ")
                                       it)})
                          (slice (split (or-else doc.text "") "\n") 0 20000))
     :color @color.editor
     :textColor @color.editorText
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent))
