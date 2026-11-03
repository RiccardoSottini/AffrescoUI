;;; views.lisp --- what editor.lisp shows

;;; Views

(defcomponent forgeEditors "The forgeEditors component."
  {}
  `(split#editor-groups :bind state.layout.groupSizes
     :minSize 160
     :sashSize 4
     :sashColor @color.border
     :sashActiveColor @color.accent
     ~(repeat [g state.groups :index gi]
       `(column#group :color @color.editor :onClick {:set state.activeGroup :to ~gi}
          ~(group-tabs g gi)
          (node :when ~(and settings.breadcrumbs.enabled
                            (some (fn [it] (and (= it.id g.active) (= it.kind "file"))) state.docs))
            :use forgeBreadcrumbs
            :with {:path ~g.active})
          (node :when ~(and (empty? g.active) (= gi 0) (= settings.workbench.startupEditor "welcome")) :use forgeWelcome)
          ~(repeat [d (filter (fn [it] (>= it 0)) [(indexOf computed.docIds g.active)]) :key d]
            (group-body gi d))))))

(defview group-tabs "#group-tabs." [g gi]
  `(tabs#group-tabs :width "100%"
     :height 34
     :bind "state.groups[gi].active"
     :color @color.tabs
     :textColor @color.muted
     :activeTextColor @color.text
     :activeBackground @color.tabActive
     :accentEdge bottom
     :accentColor ~(if (= gi state.activeGroup) "@color.accent" "@color.border")
     :tabs ~(map (fn [it]
                   (merge it
                          {:italic (= it.preview true)
                           :marker (contains computed.dirtyIds it.id)
                           :closable (not it.sticky)
                           :icon (:icon (if (and settings.workbench.fileIcons (contains it.label "."))
                                            (or-else (get state.iconTheme.names it.label)
                                                     (get state.iconTheme.extensions
                                                          (lower-case (get (reverse (split it.label ".")) 0)))
                                                     state.iconTheme.file)
                                            nil))
                           :iconColor (:color (if (and settings.workbench.fileIcons (contains it.label "."))
                                                  (or-else (get state.iconTheme.names it.label)
                                                           (get state.iconTheme.extensions
                                                                (lower-case (get (reverse (split it.label ".")) 0)))
                                                           state.iconTheme.file)
                                                  nil))}))
                 g.tabs)
     :onClose {:run closeTab :with {:gi ~gi :id ~event.id}}
     :drag {}
     :onDrop {:run moveTabToGroup :with {:id ~event.data.id :to ~gi :at ~event.index}}
     :onMove {:run reorderTab :with {:gi ~gi :from ~event.from :to ~event.to}}))

(defview editor "#editor." [gi d]
  `(editor#editor :when ~(= (get-in state.docs [d :kind]) "file")
     :font code
     :width "100%"
     :grow 1
     :bind "state.docs[d].text"
     :onChange {:run textChanged :with {:id ~(get-in state.docs [d :id])}}
     :language ~(get-in state.docs [d :language])
     :grammar ~(get-in state.docs [d :label])
     :lineNumbers ~settings.editor.lineNumbers
     :goTo ~(if (and (= gi state.activeGroup) (> state.goTo.request 0))
                {:line (max (- state.goTo.line 1) 0)
                 :column (max (- (or-else state.goTo.column 1) 1) 0)
                 :request state.goTo.request}
                nil)
     :caret state.caret
     :wrap ~settings.editor.wordWrap
     :minimap ~settings.editor.minimap.enabled
     :scrollMarks true
     :brackets ~settings.editor.bracketMatching
     :indentGuides ~settings.editor.guides.indentation
     :tabSize ~(or-else settings.editor.tabSize 4)
     :find ~(if (= gi state.activeGroup) computed.findQuery nil)
     :decorations ~(concat (or-else (:decorations (or-else (find (fn [it] (= it.id (get-in state.docs [d :id])))
                                                                 state.lsp.files)
                                                           {}))
                                    [])
                           (map (fn [it] it.mark)
                                (filter (fn [it] (= it.path (get-in state.docs [d :id]))) computed.debugMarks))
                           (if (= gi state.activeGroup) computed.conflictDecorations []))
     :tokens ~(or-else (:tokens (or-else (find (fn [it] (= it.id (get-in state.docs [d :id]))) state.lsp.files) {}))
                       [])
     :onTextHover lspHoverAt
     :onType lspTyped
     :popup ~(and (= gi state.activeGroup) state.lsp.completion.open)
     :onPopupKey lspPopupKey
     :insert ~(if (and (= gi state.activeGroup) (> state.lsp.insert.request 0)) state.lsp.insert nil)
     :color @color.editor
     :textColor @color.editorText
     :lineNumberColor @color.muted
     :activeLineNumberColor @color.text
     :lineColor @color.hover
     :selectionColor @color.selection
     :caretColor @color.text
     :onGutterClick {:run debugGutterClick :with {:path ~(get-in state.docs [d :id])}}
     :onWordHover {:run debugHover :with {:path ~(get-in state.docs [d :id])}}
     :keys {:f5 debugContinueOrStart
            "shift+f5" debugStop
            "mod+shift+f5" debugRestart
            :f6 debugPause
            :f9 debugToggleBreakpointAtCaret
            :f10 debugStepOver
            :f11 debugStepInto
            "shift+f11" debugStepOut}
     :commands {"mod+/" toggleComment
                "alt+up" moveLinesUp
                "alt+down" moveLinesDown
                "mod+shift+k" deleteLines
                "mod+[" outdentLines
                "mod+]" indentLines
                "mod+c" copyLine
                "mod+x" cutLine}))

(defview group-body "#group-body." [gi d]
  `(column#group-body :width "100%" :grow 1
     (node :when ~(and state.findBar.open (= gi state.activeGroup) (= (get-in state.docs [d :kind]) "file"))
       :use forgeFindBar)
     (node :when ~(= (get-in state.docs [d :kind]) "file")
       :use forgeConflictBar
       :visible ~(and (= gi state.activeGroup) (not (empty? computed.conflictBlocks))))
     ~(editor gi d)
     (node :when ~(= (get-in state.docs [d :kind]) "settings") :use forgeSettingsPage)
     (node :when ~(= (get-in state.docs [d :kind]) "keybindings") :use forgeKeybindingsPage)
     (node :when ~(= (get-in state.docs [d :kind]) "searchResults") :use forgeSearchEditor)
     (node :when ~(= (get-in state.docs [d :kind]) "settingsJson") :use forgeSettingsJsonPage)
     (node :when ~(= (get-in state.docs [d :kind]) "keybindingsJson") :use forgeKeybindingsJsonPage)
     (node :when ~(= (get-in state.docs [d :kind]) "diff") :use forgeDiffEditor :with {:d ~d})
     (node :when
           ~(= (get-in state.docs [d :kind]) "debugSource")
           :use
           forgeDebugSource
           :with
           {:doc ~(get state.docs d)})))
