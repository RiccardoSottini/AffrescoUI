;;; debugViews.lisp --- The debugger's views (debug.json has the logic; the source page's actions are here): the Run and Debug side bar (configurations, variables, watches, the call stack and the breakpoints), the Debug Console panel, the floating toolbar and the dialogs, and the read-only page for a source outside the open folder.

;;; Views

(defcomponent forgeDebugView "The forgeDebugView component."
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
     (list#debug-configurations :when ~(> (count state.debug.configurations) 0)
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
       :onOpen {:run debugStart})
     (label#debug-status :when ~(not= state.debug.status "inactive")
       :width "100%"
       :height 22
       :padding 8
       :icon bug
       :textColor ~(if (= state.debug.status "stopped") "#e5c07b" "@color.muted")
       :text ~(cond (= state.debug.status "stopped") (str "Paused on " state.debug.reason)
                    (= state.debug.status "starting") "Starting…"
                    :else "Running"))
     ~(debug-variables-head)
     (list#debug-variables :when ~(not (contains state.debug.collapsed "variables"))
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
       :onOpen {:run debugExpand :with {:path ~event.id}})
     ~(debug-watch-head)
     (textarea#debug-watch-input :when ~(not (contains state.debug.collapsed "watch"))
       :width "100%"
       :height 24
       :padding 5
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :bind state.debug.watchDraft
       :placeholder ~(t "debug-watch-input.placeholder")
       :onSubmit {:run debugAddWatch})
     (list#debug-watches :when ~(not (contains state.debug.collapsed "watch"))
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
       :accentColor @color.accent)
     (label#debug-stack-title :role button
       :label ~(t "debug-stack-title.label")
       :icon ~(if (contains state.debug.collapsed "stack") "chevron-right" "chevron-down")
       :text ~(t "debug-stack-title")
       :cursor hand
       :focusable true
       :isOpen ~(not (contains state.debug.collapsed "stack"))
       :onClick {:run debugToggleSection :with {:section stack}}
       :width "100%"
       :height 22
       :textColor @color.text
       :padding 4)
     (list#debug-stack :when ~(not (contains state.debug.collapsed "stack"))
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
       :onChange {:run debugSelectStack :with {:id ~(or-else event.value "")}})
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
          :onClick {:run debugToggleFilter :with {:filter ~f.filter}}))
     (list#debug-breakpoints :when ~(not (contains state.debug.collapsed "breakpoints"))
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
       :onOpen {:run debugOpenBreakpoint})))

(defcomponent forgeDebugConsole "The forgeDebugConsole component."
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

(defcomponent forgeDebugOverlays "The forgeDebugOverlays component."
  {}
  `(column#debug-overlays
     ~(debug-toolbar)
     ~(debug-edit-dialog-view)
     ~(debug-value-dialog-view)
     (node#debug-outside-dialog :use dialog
       :with {:open state.debug.outside.open
              :title "Open a source outside the folder?"
              :message ~(str "The debugger stopped in "
                             (or state.debug.outside.frame.path state.debug.outside.frame.name "a frame")
                             ", outside the open folder. Forge asks the debugger for its text and shows it read-only.")
              :buttons [{:title ~(t "debug-outside-dialog.open") :run debugOpenOutside}
                        {:title ~(t "debug-outside-dialog.cancel")}]})))

(defcomponent forgeDebugSource "The forgeDebugSource component."
  {:doc {}}
  `(column#debug-source :width "100%" :grow 1 :color @color.editor
     (label#debug-source-note :width "100%"
       :height 22
       :padding 6
       :icon file-code
       :textColor @color.muted
       :text ~(t "debug-source-note"))
     ~(debug-source-lines doc)))

(defview debug-head "#debug-head." []
  `(row#debug-head :width "100%" :height 35 :alignItems center
     (label#debug-title :text ~(t "debug-title") :textColor @color.muted :grow 1 :height 35 :padding 12)
     (label#debug-start :icon play
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if (= state.debug.status "inactive") "@color.accent" "@color.muted")
       :tooltip ~(t "debug-start.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugContinueOrStart})
     (label#debug-reload :icon refresh-cw
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-reload.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugLoadConfigurations})))

(defview debug-empty "#debug-empty." []
  `(column#debug-empty :when ~(and (= (count state.debug.configurations) 0)
                                   (empty? state.debug.error)
                                   (= state.debug.status "inactive"))
     :width "100%"
     :height 124
     :gap 10
     {:id debug-empty-top :height 4}
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
         :onClick {:run debugContinueOrStart}))
     (label#debug-empty-text :wrap true
       :width "100%"
       :height 70
       :padding 12
       :textColor @color.text
       :text ~(str "To choose what runs, add a configuration of type lldb or python to "
                   (if (empty? state.folder.root) "the open folder" "this folder")
                   "'s .forge/launch.json."
                   (if (> state.debug.skipped 0)
                       (str " " (plural state.debug.skipped "configuration") " Forge can't run were left out.")
                       "")))))

(defview debug-variables-head "#debug-variables-head." []
  `(row#debug-variables-head :width "100%" :height 22 :alignItems center
     (label#debug-variables-title :role button
       :label ~(t "debug-variables-title.label")
       :icon ~(if (contains state.debug.collapsed "variables") "chevron-right" "chevron-down")
       :text ~(t "debug-variables-title")
       :cursor hand
       :focusable true
       :isOpen ~(not (contains state.debug.collapsed "variables"))
       :onClick {:run debugToggleSection :with {:section variables}}
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
       :onClick {:run debugEditValue})))

(defview debug-watch-head "#debug-watch-head." []
  `(row#debug-watch-head :width "100%" :height 22 :alignItems center
     (label#debug-watch-title :role button
       :label ~(t "debug-watch-title.label")
       :icon ~(if (contains state.debug.collapsed "watch") "chevron-right" "chevron-down")
       :text ~(t "debug-watch-title")
       :cursor hand
       :focusable true
       :isOpen ~(not (contains state.debug.collapsed "watch"))
       :onClick {:run debugToggleSection :with {:section watch}}
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
       :onClick {:run debugRemoveWatch})))

(defview debug-breakpoints-head "#debug-breakpoints-head." []
  `(row#debug-breakpoints-head :width "100%" :height 22 :alignItems center
     (label#debug-breakpoints-title :role button
       :label ~(t "debug-breakpoints-title.label")
       :icon ~(if (contains state.debug.collapsed "breakpoints") "chevron-right" "chevron-down")
       :text ~(t "debug-breakpoints-title")
       :cursor hand
       :focusable true
       :isOpen ~(not (contains state.debug.collapsed "breakpoints"))
       :onClick {:run debugToggleSection :with {:section breakpoints}}
       :textColor @color.text
       :grow 1
       :height 22
       :padding 4)
     (label#debug-breakpoint-edit :icon pencil
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-edit.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugEditSelectedBreakpoint})
     (label#debug-breakpoint-enable :icon circle-off
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-enable.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugToggleSelectedEnabled})
     (label#debug-breakpoint-remove :icon x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-remove.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugRemoveBreakpoint})
     (label#debug-breakpoint-remove-all :icon list-x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-breakpoint-remove-all.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugRemoveAllBreakpoints})))

(defview debug-console-input-row "#debug-console-input-row." []
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
       :placeholder ~(if (= state.debug.status "inactive")
                         "Start a debug session to evaluate expressions"
                         "Evaluate (Enter; Up and Down for history; Tab to complete)")
       :onSubmit {:run debugEvaluate}
       :keys {:up debugHistoryBack :down debugHistoryForward :tab debugComplete})
     (label#debug-console-clear :icon trash-2
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "debug-console-clear.tip")
       :hover {:textColor @color.text}
       :onClick {:run debugClearConsole})))

(defview debug-toolbar "#debug-toolbar." []
  `(row#debug-toolbar :color @color.sideBar
     :borderWidth 1
     :borderColor @color.border
     :radius 4
     :gap 2
     :height 30
     :anchors {:top 40 :right 220}
     :overlay {:placement window :open ~(not= state.debug.status "inactive")}
     (label#debug-toolbar-continue :icon ~(if (= state.debug.status "stopped") "play" "pause")
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.accent
       :tooltip ~(if (= state.debug.status "stopped") "Continue (F5)" "Pause (F6)")
       :hover {:color @color.hover}
       :onClick {:run ~(if (= state.debug.status "stopped") "debugContinue" "debugPause")})
     (label#debug-toolbar-over :icon redo-dot
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if (= state.debug.status "stopped") "@color.info" "@color.muted")
       :tooltip ~(t "debug-toolbar-over.tip")
       :hover {:color @color.hover}
       :onClick {:run debugStepOver})
     (label#debug-toolbar-into :icon arrow-down-to-dot
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if (= state.debug.status "stopped") "@color.info" "@color.muted")
       :tooltip ~(t "debug-toolbar-into.tip")
       :hover {:color @color.hover}
       :onClick {:run debugStepInto})
     (label#debug-toolbar-out :icon arrow-up-from-dot
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if (= state.debug.status "stopped") "@color.info" "@color.muted")
       :tooltip ~(t "debug-toolbar-out.tip")
       :hover {:color @color.hover}
       :onClick {:run debugStepOut})
     (label#debug-toolbar-restart :icon rotate-ccw
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.accent
       :tooltip ~(t "debug-toolbar-restart.tip")
       :hover {:color @color.hover}
       :onClick {:run debugRestart})
     (label#debug-toolbar-stop :icon square
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.error
       :tooltip ~(t "debug-toolbar-stop.tip")
       :hover {:color @color.hover}
       :onClick {:run debugStop})))

(defview debug-edit-dialog-view "#debug-edit-dialog." []
  `(node#debug-edit-dialog :use dialog
     :with {:open state.debug.edit.open
            :height 230
            :title "Edit Breakpoint"
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
       :onSubmit {:run debugSaveBreakpoint})
     (textarea#debug-edit-hits :bind state.debug.edit.hitCondition
       :anchors {:left 20 :right 20 :top 84}
       :height 28
       :padding 7
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "debug-edit-hits.placeholder")
       :onSubmit {:run debugSaveBreakpoint})
     (textarea#debug-edit-log :bind state.debug.edit.logMessage
       :anchors {:left 20 :right 20 :top 120}
       :height 28
       :padding 7
       :font code
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "debug-edit-log.placeholder")
       :onSubmit {:run debugSaveBreakpoint})))

(defview debug-value-dialog-view "#debug-value-dialog." []
  `(node#debug-value-dialog :use dialog
     :with {:open state.debug.setValue.open
            :height 150
            :title ~(str "Set Value of " state.debug.setValue.name)
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
       :onSubmit {:run debugSetValue})))

(defview debug-source-lines "#debug-source-lines." [doc]
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

;;; Functions

(defn debugOpenOutside "TODO" []
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

(defn debugOnSource "TODO" [context body]
  (debugShowSource
    (str "debug-source:" (if (empty? context.frame.path) (str "ref:" context.frame.reference) context.frame.path))
    (str (if (empty? context.frame.path) context.frame.name (get (reverse (split context.frame.path "/")) 0))
         " (read-only)")
    (slice (text (or-else body.content "")) 0 4194304)))

(defn debugOnSourceRefused "TODO" [message]
  (notify "info" (str "The debugger has no source for this frame: " message)))

(defn debugShowSource "TODO" [id label content]
  (set "state.docs" (filter (fn [it] (not= it.id id)) state.docs))
  (push "state.docs" {:id id :label label :kind "debugSource" :text content :dirty false :language ""})
  (showTab id label))

(defn debugApplySource "TODO" []
  (debugShowSource (str "debug-source:" event.path) event.label event.content))
