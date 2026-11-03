;;; views.lisp --- the editor groups: each a strip of tabs over the page its active tab shows
;;;
;;; The page is a file's editor (with the find bar and the merge conflict bar over it), or one of the pages other
;;; features give: the settings, the keybindings, search results, a diff, a debugged source.

;;; The groups

(defcomponent forgeEditors "The editor groups, side by side: each its tabs, its breadcrumbs and its page."
  {}
  `(split#editor-groups :bind state.layout.groupSizes
     :minSize 160
     :sashSize 4
     :sashColor @color.border
     :sashActiveColor @color.accent
     ~(repeat [g state.groups :index gi]
       `(column#group :color @color.editor :onClick ~(handler focusGroup gi)
          ~(group-tabs g gi)
          (node :when ~(and settings.breadcrumbs.enabled
                            (some (fn [it] (and (= it.id g.active) (= it.kind "file"))) state.docs))
            :use forgeBreadcrumbs
            :with {:path ~g.active})
          (node :when ~(and (empty? g.active) (= gi 0) (= settings.workbench.startupEditor "welcome")) :use forgeWelcome)
          ~(repeat [d (filter (fn [it] (>= it 0)) [(indexOf computed.docIds g.active)]) :key d]
            (group-body gi d))))))

(defview group-tabs "A group's tabs: a dirty file's has a marker, the preview's is in italics, each its file's icon."
  [g gi]
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
                           :icon (:icon (tab-icon it))
                           :iconColor (:color (tab-icon it))}))
                 g.tabs)
     :onClose {:run closeTab :with {:gi ~gi :id ~event.id}}
     :drag {}
     :onDrop {:run moveTabToGroup :with {:id ~event.data.id :to ~gi :at ~event.index}}
     :onMove {:run reorderTab :with {:gi ~gi :from ~event.from :to ~event.to}}))

(defmacro tab-icon
  ;; A macro, not a function: what it writes stays an expression, worked out at its speed.
  "The icon theme's icon of a tab's file, by its name or else its extension; none for a page or without file icons."
  [tab]
  `(if (and settings.workbench.fileIcons (contains ~(symbol (str tab ".label")) "."))
     (or-else (get state.iconTheme.names ~(symbol (str tab ".label")))
              (get state.iconTheme.extensions (lower-case (get (reverse (split ~(symbol (str tab ".label")) ".")) 0)))
              state.iconTheme.file)
     nil))

;;; A group's page

(defview group-body "A group's page: the active tab's file in an editor, or the page another feature gives it." [gi d]
  `(column#group-body :width "100%" :grow 1
     (node :when ~(and state.findBar.open (= gi state.activeGroup) (file-page? d)) :use forgeFindBar)
     (node :when ~(file-page? d)
       :use forgeConflictBar
       :visible ~(and (= gi state.activeGroup) (not (empty? computed.conflictBlocks))))
     ~(editor gi d)
     (node :when ~(page? d "settings") :use forgeSettingsPage)
     (node :when ~(page? d "keybindings") :use forgeKeybindingsPage)
     (node :when ~(page? d "searchResults") :use forgeSearchEditor)
     (node :when ~(page? d "settingsJson") :use forgeSettingsJsonPage)
     (node :when ~(page? d "keybindingsJson") :use forgeKeybindingsJsonPage)
     (node :when ~(page? d "diff") :use forgeDiffEditor :with {:d ~d})
     (node :when ~(page? d "debugSource") :use forgeDebugSource :with {:doc ~(get state.docs d)})))

(defmacro page? "Whether the open document `d` is a page of that kind." [d kind]
  `(= (get-in state.docs [~d :kind]) ~kind))

(defmacro file-page? "Whether the open document `d` is a file." [d]
  `(page? ~d "file"))

(defmacro doc-of "A field of the open document `d`." [d field]
  `(get-in state.docs [~d ~field]))

(defview editor "A file's editor: its text, the language server's marks and colours, the debugger's, and its keys." [gi d]
  `(editor#editor :when ~(file-page? d)
     :font code
     :width "100%"
     :grow 1
     :bind "state.docs[d].text"
     :onChange ~(handler textChanged (doc-of d :id))
     :language ~(doc-of d :language)
     :grammar ~(doc-of d :label)
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
     :decorations ~(concat (or-else (:decorations (or-else (find (fn [it] (= it.id (doc-of d :id))) state.lsp.files) {})) [])
                           (map (fn [it] it.mark) (filter (fn [it] (= it.path (doc-of d :id))) computed.debugMarks))
                           (if (= gi state.activeGroup) computed.conflictDecorations []))
     :tokens ~(or-else (:tokens (or-else (find (fn [it] (= it.id (doc-of d :id))) state.lsp.files) {})) [])
     :onTextHover lspHoverAt
     :onType lspTyped
     :popup ~(and (= gi state.activeGroup) state.lsp.completion.open)
     :onPopupKey lspPopupKey
     :insert ~(if (and (= gi state.activeGroup) (> state.lsp.insert.request 0)) state.lsp.insert nil)
     :onGutterClick ~(handler debugGutterClick (doc-of d :id))
     :onWordHover ~(handler debugHover (doc-of d :id))
     :keys ~debugger-keys
     :commands ~line-commands
     ~@editor-colours))

(def editor-colours "An editor's colours, from the theme." 
  [:color "@color.editor" :textColor "@color.editorText" :lineNumberColor "@color.muted"
   :activeLineNumberColor "@color.text" :lineColor "@color.hover" :selectionColor "@color.selection"
   :caretColor "@color.text"])

(def debugger-keys "The debugger's keys in an editor, each a command of debug.lisp."
  {:f5 "debugContinueOrStart" "shift+f5" "debugStop" "mod+shift+f5" "debugRestart" :f6 "debugPause"
   :f9 "debugToggleBreakpointAtCaret" :f10 "debugStepOver" :f11 "debugStepInto" "shift+f11" "debugStepOut"})

(def line-commands "The line commands' keys in an editor, each a command of editing.lisp."
  {"mod+/" "toggleComment" "alt+up" "moveLinesUp" "alt+down" "moveLinesDown" "mod+shift+k" "deleteLines"
   "mod+[" "outdentLines" "mod+]" "indentLines" "mod+c" "copyLine" "mod+x" "cutLine"})
