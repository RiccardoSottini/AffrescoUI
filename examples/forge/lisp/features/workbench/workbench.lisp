;;; workbench.lisp --- Forge's workbench extras: the outline, breadcrumbs, recent files, editor groups, settings.json
;;;
;;; The secondary side bar shows the Outline: the active file's symbols from the language server, else its headings
;;; or the lines that define something. Breadcrumbs over the editor show the file's folders and the symbol at the
;;; caret. Tabs move between groups and along their strip; a pinned tab stays at the start. Recently opened files
;;; come first in Go to File. The settings you changed open as JSON to edit, and the keys as JSON to read.

;;; The outline

(defstate outline [])

(defstate outlineSymbols "")

(defcomputed outlineItems
  (map-indexed (fn [index it]
                   {:id (str (text it.line) ":" index)
                    :label (+ (slice "                " 0 (* 4 (min it.depth 4))) it.label)
                    :icon (cond (nil? it.kind) (if (> it.depth 0) "chevron-right" "box")
                                (contains ["function" "method" "constructor"] it.kind) "parentheses"
                                (contains ["class" "struct" "interface"] it.kind) "component"
                                (contains ["namespace" "module" "package"] it.kind) "braces"
                                (contains ["variable" "field" "property" "constant"] it.kind) "variable"
                                (contains ["enum" "enumMember"] it.kind) "list-tree"
                                :else "hash")})
                 state.outline))

(defcommand toggleSecondarySidebar "Show or hide the side bar at the right, with the Outline of the active file."
  :title ~(t "command.toggleSecondarySidebar") :category ~(t "category.view") :icon "panel-right"
  :description ~(t "command.toggleSecondarySidebar.about") []
  (toggle "state.layout.secondary")
  (refreshOutline))

(defcommand refreshOutline "Refresh Outline"
  :title ~(t "command.refreshOutline")
  :description ~(t "command.refreshOutline") :category ~(t "category.outline") :icon "refresh-cw" []
  (if (= computed.activeDoc.kind "file")
    (do
      (when (some? computed.lspLanguage) (lspAsk "outline" nil nil "" ""))
      (when (not= state.outlineSymbols computed.activeDoc.id)
        (outlineOf (if (= computed.activeDoc.language "Markdown")
                     []
                     (map-indexed (fn [index it] {:line (+ index 1) :row it :code (trim it)})
                                  (slice (split computed.activeDoc.text "\n") 0 20000)))
                   (if (= computed.activeDoc.language "Markdown")
                     (filter (fn [it] (= (indexOf it.row "#") 0))
                             (map-indexed (fn [index it] {:line (+ index 1) :row it})
                                          (slice (split computed.activeDoc.text "\n") 0 20000)))
                     [])
                   (= computed.activeDoc.language "Python"))))
    (do (set "state.outline" []) (set "state.outlineSymbols" ""))))

(defn outlineOf
  "A file's outline from its lines (`rows`, `headings`, whether `python`): headings, or the lines that define."
  [rows headings python]
  (set "state.outline"
       (slice (concat (markdownOutline headings)
                      (if python (pythonOutline rows) [])
                      (if python [] (braceOutline rows)))
              0
              2000)))

(defn markdownOutline "A Markdown file's headings (# to ####), each at its level." [headings]
  (map (fn [it]
         {:line it.line
          :depth (- (indexOf it.row " ") 1)
          :label (slice (trim (slice it.row (indexOf it.row " "))) 0 120)})
       (filter (fn [it] (contains ["# " "## " "### " "#### "] (slice it.row 0 (+ (indexOf it.row " ") 1)))) headings)))

(defn pythonOutline "A Python file's classes and functions, each as deep as it is indented (3 levels at most)." [rows]
  (map (fn [it]
         {:line it.line
          :depth (min (cond it.tab it.indent (>= it.indent 12) 3 (>= it.indent 8) 2 (>= it.indent 1) 1 :else 0) 3)
          :kind it.kind
          :label (slice (slice it.rest
                               0
                               (cond
                                 (> (indexOf it.rest "(") 0) (indexOf it.rest "(")
                                 (> (indexOf it.rest ":") 0) (indexOf it.rest ":")
                                 :else (count it.rest)))
                        0
                        120)})
       (map (fn [it]
              {:line it.line
               :indent (indexOf it.row it.code)
               :tab (= (slice it.row 0 1) "\t")
               :kind (if (= (slice it.code 0 6) "class ") "class" "function")
               :rest (trim (slice it.code
                                  (cond (= (slice it.code 0 10) "async def ") 10 (= (slice it.code 0 4) "def ") 4 :else 6)))})
            (filter (fn [it]
                      (or (= (slice it.code 0 4) "def ")
                          (= (slice it.code 0 6) "class ")
                          (= (slice it.code 0 10) "async def ")))
                    rows))))

(defn braceOutline
  "A file's lines that open a block with {, at the top or one level in: classes, namespaces and functions."
  [rows]
  (map (fn [it]
         {:line it.line
          :depth (if (= it.indent 0) 0 1)
          :label it.label
          :kind (cond
                  (contains ["class " "struct" "interf"] (slice it.label 0 6)) "class"
                  (= (slice it.label 0 10) "namespace ") "namespace"
                  (and (contains it.label "(") (not (contains ["if " "if(" "for" "whi" "swi"] (slice it.label 0 3))))
                    "function"
                  :else nil)})
       (filter (fn [it]
                 (and (not (empty? it.label)) (or (= it.indent 0) (if it.tab (= it.indent 1) (<= it.indent 4)))))
               (map (fn [it]
                      {:line it.line
                       :indent (indexOf it.row it.code)
                       :tab (= (slice it.row 0 1) "\t")
                       :label (slice (trim (slice it.code 0 (- 1))) 0 120)})
                    (filter (fn [it]
                              (and (= (slice it.code (- 1)) "{") (not (contains ["}" "/" "#"] (slice it.code 0 1)))))
                            rows)))))

(defn outlinePicked "A symbol of the outline, picked: the caret goes to its line." []
  (when (> (number (get (split event.value ":") 0)) 0) (goToLine (number (get (split event.value ":") 0)))))

(defn outlineFollowsCaret "The caret moved: the outline is read again once it rests 300 ms." []
  (debounce 300 refreshOutline))

(defpart outline-follows "The outline follows the caret, once it rests."
  :watch {:state.caret outlineFollowsCaret})

(defkey "mod+alt+b" toggleSecondarySidebar)

;;; Breadcrumbs, the minimap and the diff's layout

(defcomputed symbolTrail
  (filter (fn [it] (some? it))
            (reduce (fn [acc it]
                      (concat (slice (concat acc [nil nil nil nil nil nil nil nil]) 0 (min it.depth 7)) [it]))
                    []
                    (filter (fn [it] (<= it.line state.caret.line)) state.outline))))

(defcustom breadcrumbs.enabled true
  "Show the file's folders and the symbol at the caret above the editor."
  :description ~(t "settings.breadcrumbs.enabled.about")
  :type boolean
  :title ~(t "settings.breadcrumbs.enabled"))

(defcommand toggleBreadcrumbs "Toggle Breadcrumbs"
  :title ~(t "command.toggleBreadcrumbs")
  :description ~(t "command.toggleBreadcrumbs") :category ~(t "category.view") :icon "chevrons-right" []
  (set "settings.breadcrumbs.enabled" (not settings.breadcrumbs.enabled)))

(defn breadcrumbClicked
  "A breadcrumb clicked: a folder (the `si`th part of `path`) opens in the explorer, the file's name shows its symbols."
  [path si]
  (if (< si (- (count (split path "/")) 1))
    (revealFolder (join "/" (slice (split path "/") 0 (+ si 1))))
    (goToSymbol)))

(defn revealFolder "Opens the explorer with a folder and its parents open, the folder chosen." [path]
  (explorerShow path)
  (when (not (contains state.explorer.expanded path))
    (set "state.explorer.expanded" (concat state.explorer.expanded [path]))
    (explorerList path))
  (set "state.sideView" "explorer")
  (set "state.layout.sidebar" true))

(defcustom editor.minimap.enabled false
  "Show a narrow overview of the file at the editor's right edge; press or drag it to scroll."
  :description ~(t "settings.editor.minimap.enabled.about")
  :type boolean
  :title ~(t "settings.editor.minimap.enabled"))

(defcommand toggleMinimap "Toggle Minimap"
  :title ~(t "command.toggleMinimap")
  :description ~(t "command.toggleMinimap") :category ~(t "category.view") :icon "map" []
  (set "settings.editor.minimap.enabled" (not settings.editor.minimap.enabled)))

(defcustom diffEditor.renderSideBySide true
  "Compare a changed file side by side; off, inline (the deleted lines above the inserted ones)."
  :description ~(t "settings.diffEditor.renderSideBySide.about")
  :type boolean
  :title ~(t "settings.diffEditor.renderSideBySide"))

(defcommand toggleDiffInline "Toggle Inline View"
  :title ~(t "command.toggleDiffInline")
  :description ~(t "command.toggleDiffInline") :category ~(t "category.compare") :icon "columns" []
  (set "settings.diffEditor.renderSideBySide" (not settings.diffEditor.renderSideBySide)))

;;; Recently opened files

(defn rememberRecent "A file just shown (`id`, `label`) comes first in the recently opened files."
  [id label]
  (set "state.quickPick.recent"
       (slice (concat [{:id id :title label :detail (join "/" (slice (split id "/") 0 (- (count (split id "/")) 1)))}]
                      (filter (fn [it] (not= it.id id)) (or-else state.quickPick.recent [])))
              0
              10))
  (refreshOutline))

(defcommand openRecent "Go to File, with the recently opened files first."
  :title ~(t "command.openRecent") :category ~(t "category.file") :icon "history"
  :description ~(t "command.openRecent.about") []
  (quickOpen))

(defcommand clearRecent "Clear Recently Opened"
  :title ~(t "command.clearRecent") :description ~(t "command.clearRecent") :category ~(t "category.file") :icon "trash"
  :enablement ~(> (count (or-else state.quickPick.recent [])) 0) []
  (set "state.quickPick.recent" []))

(defkey "ctrl+r" openRecent)

;;; Editor groups and tabs

(defn moveTabToGroup
  "A tab dropped on group `to`'s strip at `at`: it leaves its group, which closes if empty."
  [id to at]
  (when (holdingTab id to)
    (set (str "state.groups[" to "].tabs")
         (concat (slice (filter (fn [it] (not= it.id id)) (get-in state.groups [to :tabs])) 0 at)
                 [(find (fn [it] (= it.id id)) (:tabs (get state.groups (holdingTab id to))))]
                 (slice (filter (fn [it] (not= it.id id)) (get-in state.groups [to :tabs])) at)))
    (set (str "state.groups[" to "].active") id)
    (closeTab (indexOf (map-indexed (fn [index it] (and (not= index to) (contains (map (fn [it] it.id) it.tabs) id)))
                                    state.groups)
                       true)
              id)
    (set "state.activeGroup" (indexOf (map (fn [it] (= it.active id)) state.groups) true))))

(defn holdingTab "The index of the group other than `to` that has the tab `id`, or nil." [id to]
  (first (filter (fn [g] (and (not= g to) (some (fn [tab] (= tab.id id)) (get-in state.groups [g :tabs]))))
                 (range (count state.groups)))))

(defn without "A list without its item at `at`." [items at]
  (concat (take at items) (drop (inc at) items)))

(defn reorderTab "A tab dragged along its strip, from one place to another." [gi from to]
  (set (str "state.groups[" gi "].tabs")
       (concat (slice (without (get-in state.groups [gi :tabs]) from) 0 to)
               [(get-in state.groups [gi :tabs from])]
               (slice (without (get-in state.groups [gi :tabs]) from) to))))

(defcommand moveEditorToNextGroup "Move the active editor into the group at its right, splitting if there is none."
  :title ~(t "command.moveEditorToNextGroup") :category ~(t "category.view") :icon "arrow-right"
  :description ~(t "command.moveEditorToNextGroup.about")
  :enablement ~(and (not (empty? (get-in state.groups [state.activeGroup :active])))
                    (or (< state.activeGroup (- (count state.groups) 1))
                        (> (count (get-in state.groups [state.activeGroup :tabs])) 1)))
  []
  (when (= state.activeGroup (- (count state.groups) 1)) (push "state.groups" {:tabs [] :active nil}))
  (moveTabToGroup (activeTabId) (+ state.activeGroup 1) 1000))

(defcommand moveEditorToPreviousGroup "Move Editor into Previous Group"
  :title ~(t "command.moveEditorToPreviousGroup")
  :description ~(t "command.moveEditorToPreviousGroup") :category ~(t "category.view") :icon "arrow-left"
  :enablement ~(and (not (empty? (get-in state.groups [state.activeGroup :active]))) (> state.activeGroup 0)) []
  (moveTabToGroup (activeTabId) (- state.activeGroup 1) 1000))

(defcommand focusFirstGroup "Focus First Editor Group"
  :title ~(t "command.focusFirstGroup") :description ~(t "command.focusFirstGroup") :category ~(t "category.view") []
  (set "state.activeGroup" 0))

(defcommand focusSecondGroup "Focus Second Editor Group"
  :title ~(t "command.focusSecondGroup")
  :description ~(t "command.focusSecondGroup") :category ~(t "category.view") :enablement ~(> (count state.groups) 1) []
  (set "state.activeGroup" 1))

(defcommand pinEditor
  "Keep the active editor at the start of its group, without a close button; Close Other Editors leaves it open."
  :title ~(t "command.pinEditor") :category ~(t "category.view") :icon "pin"
  :description ~(t "command.pinEditor.about")
  :enablement ~(some (fn [it] (and (= it.id (get-in state.groups [state.activeGroup :active])) (not it.sticky))) (get-in state.groups [state.activeGroup :tabs])) []
  (set (str "state.groups[" state.activeGroup "].tabs")
       (concat (map (fn [it] (merge it {:sticky true :preview false}))
                    (filter (fn [it] (or it.sticky (= it.id (get-in state.groups [state.activeGroup :active]))))
                            (get-in state.groups [state.activeGroup :tabs])))
               (filter (fn [it] (and (not it.sticky) (not= it.id (get-in state.groups [state.activeGroup :active]))))
                       (get-in state.groups [state.activeGroup :tabs])))))

(defcommand unpinEditor "Unpin Editor"
  :title ~(t "command.unpinEditor") :description ~(t "command.unpinEditor") :category ~(t "category.view")
  :icon "pin-off"
  :enablement ~(some (fn [it] (and (= it.id (get-in state.groups [state.activeGroup :active])) it.sticky))
                     (get-in state.groups [state.activeGroup :tabs]))
  []
  (set (str "state.groups[" state.activeGroup "].tabs")
       (concat (filter (fn [it] (and it.sticky (not= it.id (activeTabId)))) (activeTabs))
               (map (fn [it] (merge it {:sticky false})) (filter (fn [it] (= it.id (activeTabId))) (activeTabs)))
               (filter (fn [it] (and (not it.sticky) (not= it.id (activeTabId)))) (activeTabs)))))

(defcommand closeOtherEditors "Close the active group's editors but the active one and the pinned ones."
  :title ~(t "command.closeOtherEditors") :category ~(t "category.view")
  :description ~(t "command.closeOtherEditors.about")
  :enablement ~(some (fn [it] (and (not= it.id (get-in state.groups [state.activeGroup :active])) (not it.sticky)))
                     (get-in state.groups [state.activeGroup :tabs]))
  []
  (doseq [tab (filter (fn [it] (and (not= it.id (activeTabId)) (not it.sticky))) (activeTabs))]
    (closeTab state.activeGroup tab.id)))

(defkey "mod+ctrl+right" moveEditorToNextGroup)

(defkey "mod+ctrl+left" moveEditorToPreviousGroup)

(defkey "mod+1" focusFirstGroup)

(defkey "mod+2" focusSecondGroup)

(defkey "mod+k shift+enter" pinEditor)

(defkey "mod+k mod+shift+enter" unpinEditor)

(defkey "mod+k u" closeOtherEditors)

;;; settings.json and keybindings.json

(defstate settingsJson "")

(defstate settingsJsonError "")

(defcomputed keybindingsJson
  (json (map (fn [it] {:key it.key :command it.command :when (or-else it.when "")}) keybindings)))

(defcommand openSettingsJson "The settings you changed, as JSON: edit them and Apply."
  :title ~(t "command.openSettingsJson") :category ~(t "category.preferences") :icon "braces"
  :description ~(t "command.openSettingsJson.about") []
  (set "state.settingsJson" (json (or-else state.settings {})))
  (set "state.settingsJsonError" "")
  (openPage "forge:settings.json" "settings.json" "settingsJson"))

(defcommand applySettingsJson "Apply Settings (JSON)"
  :title ~(t "command.applySettingsJson")
  :description ~(t "command.applySettingsJson") :category ~(t "category.preferences") :icon "check" []
  (if (= (type (parseJson state.settingsJson)) "object")
    (do (set "state.settings" (parseJson state.settingsJson)) (set "state.settingsJsonError" ""))
    (set "state.settingsJsonError" (t "settingsJson.notObject"))))

(defcommand openKeybindingsJson
  "The keys every feature contributes, as JSON (read-only: a feature's keys section changes them)."
  :title ~(t "command.openKeybindingsJson") :category ~(t "category.preferences") :icon "braces"
  :description ~(t "command.openKeybindingsJson.about") []
  (openPage "forge:keybindings.json" "keybindings.json" "keybindingsJson"))
