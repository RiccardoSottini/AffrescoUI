;;; workbench.lisp --- Forge's workbench extras: the secondary side bar with the Outline (the active file's symbols from the language server, else its headings or the lines that open a block), moving tabs between editor groups (by dragging, or Move Editor into Next or Previous Group), the breadcrumbs over the editor, the recently opened files (first in Go to File, kept by persist), the minimap setting, and the settings and keyboard shortcuts as JSON
;;;
;;; The editor groups (features/editor) use forgeBreadcrumbs and the pages; the shell (features/shell) has
;;; state.layout.secondary.

;;; State and definitions

(defstate outline [])

(defstate outlineSymbols "")

(defstate settingsJson "")

(defstate settingsJsonError "")

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

(defcomputed symbolTrail
  (filter (fn [it] (some? it))
            (reduce (fn [acc it]
                      (concat (slice (concat acc [nil nil nil nil nil nil nil nil]) 0 (min it.depth 7)) [it]))
                    []
                    (filter (fn [it] (<= it.line state.caret.line)) state.outline))))

(defcomputed keybindingsJson
  (json (map (fn [it] {:key it.key :command it.command :when (or-else it.when "")}) keybindings)))

(defkey "mod+alt+b" toggleSecondarySidebar)

(defkey "mod+ctrl+right" moveEditorToNextGroup)

(defkey "mod+ctrl+left" moveEditorToPreviousGroup)

(defkey "ctrl+r" openRecent)

(defkey "mod+1" focusFirstGroup)

(defkey "mod+2" focusSecondGroup)

(defkey "mod+k shift+enter" pinEditor)

(defkey "mod+k mod+shift+enter" unpinEditor)

(defkey "mod+k u" closeOtherEditors)

(defcustom editor.minimap.enabled
  false "Show a narrow overview of the file at the editor's right edge; press or drag it to scroll." :description ~(t "settings.editor.minimap.enabled.about") :type boolean :title ~(t "settings.editor.minimap.enabled"))

(defcustom breadcrumbs.enabled
  true "Show the file's folders and the symbol at the caret above the editor." :description ~(t "settings.breadcrumbs.enabled.about") :type boolean :title ~(t "settings.breadcrumbs.enabled"))

(defcustom diffEditor.renderSideBySide
  true "Compare a changed file side by side; off, inline (the deleted lines above the inserted ones)." :description ~(t "settings.diffEditor.renderSideBySide.about") :type boolean :title ~(t "settings.diffEditor.renderSideBySide"))

(defpart workbench "What workbench adds." :watch {:state.caret outlineFollowsCaret})

;;; Functions and commands

(defcommand toggleSecondarySidebar "Show or hide the side bar at the right, with the Outline of the active file."
  :title ~(t "command.toggleSecondarySidebar") :category ~(t "category.view") :icon "panel-right"
  :description ~(t "command.toggleSecondarySidebar.about") []
  (toggle "state.layout.secondary")
  (refreshOutline))

(defcommand refreshOutline "Refresh Outline"
  :title ~(t "command.refreshOutline") :category ~(t "category.outline") :icon "refresh-cw" []
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

(defn outlineOf "rows: a file's lines (line, row, code: the row trimmed); headings: a Markdown file's lines that start with #; python: whether rows are Python's. The outline: the headings, Python's classes and functions by their indentation, or the lines of code that open a block, at the first or second level"
  [rows headings python]
  (set "state.outline"
       (slice (concat (map (fn [it]
                             {:line it.line
                              :depth (- (indexOf it.row " ") 1)
                              :label (slice (trim (slice it.row (indexOf it.row " "))) 0 120)})
                           (filter (fn [it]
                                     (contains ["# " "## " "### " "#### "]
                                               (slice it.row 0 (+ (indexOf it.row " ") 1))))
                                   headings))
                      (map (fn [it]
                             {:line it.line
                              :depth (min (cond
                                            it.tab it.indent
                                            (>= it.indent 12) 3
                                            (>= it.indent 8) 2
                                            (>= it.indent 1) 1
                                            :else 0)
                                          3)
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
                                                      (cond
                                                        (= (slice it.code 0 10) "async def ") 10
                                                        (= (slice it.code 0 4) "def ") 4
                                                        :else 6)))})
                                (filter (fn [it]
                                          (and python
                                               (or (= (slice it.code 0 4) "def ")
                                                   (= (slice it.code 0 6) "class ")
                                                   (= (slice it.code 0 10) "async def "))))
                                        rows)))
                      (map (fn [it]
                             {:line it.line
                              :depth (if (= it.indent 0) 0 1)
                              :label it.label
                              :kind (cond
                                      (contains ["class " "struct" "interf"] (slice it.label 0 6)) "class"
                                      (= (slice it.label 0 10) "namespace ") "namespace"
                                      (and (contains it.label "(")
                                           (not (contains ["if " "if(" "for" "whi" "swi"] (slice it.label 0 3))))
                                        "function"
                                      :else nil)})
                           (filter (fn [it]
                                     (and (not (empty? it.label))
                                          (or (= it.indent 0) (if it.tab (= it.indent 1) (<= it.indent 4)))))
                                   (map (fn [it]
                                          {:line it.line
                                           :indent (indexOf it.row it.code)
                                           :tab (= (slice it.row 0 1) "\t")
                                           :label (slice (trim (slice it.code 0 (- 1))) 0 120)})
                                        (filter (fn [it]
                                                  (and (not python)
                                                       (= (slice it.code (- 1)) "{")
                                                       (not (contains ["}" "/" "#"] (slice it.code 0 1)))))
                                                rows)))))
              0
              2000)))

(defn outlinePicked "TODO" []
  (when (> (number (get (split event.value ":") 0)) 0) (goToLine (number (get (split event.value ":") 0)))))

(defcommand toggleMinimap "Toggle Minimap"
  :title ~(t "command.toggleMinimap") :category ~(t "category.view") :icon "map" []
  (set "settings.editor.minimap.enabled" (not settings.editor.minimap.enabled)))

(defcommand toggleDiffInline "Toggle Inline View"
  :title ~(t "command.toggleDiffInline") :category ~(t "category.compare") :icon "columns" []
  (set "settings.diffEditor.renderSideBySide" (not settings.diffEditor.renderSideBySide)))

(defcommand toggleBreadcrumbs "Toggle Breadcrumbs"
  :title ~(t "command.toggleBreadcrumbs") :category ~(t "category.view") :icon "chevrons-right" []
  (set "settings.breadcrumbs.enabled" (not settings.breadcrumbs.enabled)))

(defn revealFolder "path: a folder's path. Opens the explorer with that folder and its parents open" [path]
  (explorerShow path)
  (when (not (contains state.explorer.expanded path))
    (set "state.explorer.expanded" (concat state.explorer.expanded [path]))
    (explorerList path))
  (set "state.sideView" "explorer")
  (set "state.layout.sidebar" true))

(defn rememberRecent "id, label: a file just shown. Puts it first in the recently opened files (Go to File lists them first)"
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
  :title ~(t "command.clearRecent") :category ~(t "category.file") :icon "trash"
  :enablement ~(> (count (or-else state.quickPick.recent [])) 0) []
  (set "state.quickPick.recent" []))

(defn moveTabToGroup "id, to, at: a tab dropped on group to's strip at place at. It leaves the group it was in (closed if empty)"
  [id to at]
  (when (holdingTab id to)
    (set (str "state.groups[" to "].tabs")
         (concat (slice (filter (fn [it] (not= it.id id)) (get-in state.groups [to :tabs])) 0 at)
                 [(find (fn [it] (= it.id id))
                        (:tabs (get state.groups (holdingTab id to))))]
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

(defn reorderTab "gi, from, to: a tab dragged along its strip" [gi from to]
  (set (str "state.groups[" gi "].tabs")
       (concat (slice (without (get-in state.groups [gi :tabs]) from) 0 to)
               [(get-in state.groups [gi :tabs from])]
               (slice (without (get-in state.groups [gi :tabs]) from) to))))

(defcommand moveEditorToNextGroup "Move the active editor into the group at its right, splitting if there is none."
  :title ~(t "command.moveEditorToNextGroup") :category ~(t "category.view") :icon "arrow-right"
  :description ~(t "command.moveEditorToNextGroup.about")
  :enablement ~(and (not (empty? (get-in state.groups [state.activeGroup :active]))) (or (< state.activeGroup (- (count state.groups) 1)) (> (count (get-in state.groups [state.activeGroup :tabs])) 1))) []
  (when (= state.activeGroup (- (count state.groups) 1)) (push "state.groups" {:tabs [] :active nil}))
  (moveTabToGroup (get-in state.groups [state.activeGroup :active]) (+ state.activeGroup 1) 1000))

(defcommand moveEditorToPreviousGroup "Move Editor into Previous Group"
  :title ~(t "command.moveEditorToPreviousGroup") :category ~(t "category.view") :icon "arrow-left"
  :enablement ~(and (not (empty? (get-in state.groups [state.activeGroup :active]))) (> state.activeGroup 0)) []
  (moveTabToGroup (get-in state.groups [state.activeGroup :active]) (- state.activeGroup 1) 1000))

(defcommand focusFirstGroup "Focus First Editor Group"
  :title ~(t "command.focusFirstGroup") :category ~(t "category.view") []
  (set "state.activeGroup" 0))

(defcommand focusSecondGroup "Focus Second Editor Group"
  :title ~(t "command.focusSecondGroup") :category ~(t "category.view") :enablement ~(> (count state.groups) 1) []
  (set "state.activeGroup" 1))

(defcommand pinEditor "Keep the active editor at the start of its group, without a close button; Close Other Editors leaves it open."
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
  :title ~(t "command.unpinEditor") :category ~(t "category.view") :icon "pin-off"
  :enablement ~(some (fn [it] (and (= it.id (get-in state.groups [state.activeGroup :active])) it.sticky)) (get-in state.groups [state.activeGroup :tabs])) []
  (set (str "state.groups[" state.activeGroup "].tabs")
       (concat (filter (fn [it] (and it.sticky (not= it.id (get-in state.groups [state.activeGroup :active]))))
                       (get-in state.groups [state.activeGroup :tabs]))
               (map (fn [it] (merge it {:sticky false}))
                    (filter (fn [it] (= it.id (get-in state.groups [state.activeGroup :active])))
                            (get-in state.groups [state.activeGroup :tabs])))
               (filter (fn [it] (and (not it.sticky) (not= it.id (get-in state.groups [state.activeGroup :active]))))
                       (get-in state.groups [state.activeGroup :tabs])))))

(defcommand closeOtherEditors "Close the active group's editors but the active one and the pinned ones."
  :title ~(t "command.closeOtherEditors") :category ~(t "category.view")
  :description ~(t "command.closeOtherEditors.about")
  :enablement ~(some (fn [it] (and (not= it.id (get-in state.groups [state.activeGroup :active])) (not it.sticky))) (get-in state.groups [state.activeGroup :tabs])) []
  (doseq [tab
          (filter (fn [it] (and (not= it.id (get-in state.groups [state.activeGroup :active])) (not it.sticky)))
                  (get-in state.groups [state.activeGroup :tabs]))]
    (closeTab state.activeGroup tab.id)))

(defcommand openSettingsJson "The settings you changed, as JSON: edit them and Apply."
  :title ~(t "command.openSettingsJson") :category ~(t "category.preferences") :icon "braces"
  :description ~(t "command.openSettingsJson.about") []
  (set "state.settingsJson" (json (or-else state.settings {})))
  (set "state.settingsJsonError" "")
  (openPage "forge:settings.json" "settings.json" "settingsJson"))

(defcommand applySettingsJson "Apply Settings (JSON)"
  :title ~(t "command.applySettingsJson") :category ~(t "category.preferences") :icon "check" []
  (if (= (type (parseJson state.settingsJson)) "object")
    (do (set "state.settings" (parseJson state.settingsJson)) (set "state.settingsJsonError" ""))
    (set "state.settingsJsonError" "Not applied: the text isn't a JSON object of settings.")))

(defcommand openKeybindingsJson "The keys every feature contributes, as JSON (read-only: a feature's keys section changes them)."
  :title ~(t "command.openKeybindingsJson") :category ~(t "category.preferences") :icon "braces"
  :description ~(t "command.openKeybindingsJson.about") []
  (openPage "forge:keybindings.json" "keybindings.json" "keybindingsJson"))

(defn outlineFollowsCaret "TODO" []
  (debounce 300 outlineFollowsCaretLater))

(defn outlineFollowsCaretLater "What outlineFollowsCaret does when its time comes." []
  (refreshOutline))
