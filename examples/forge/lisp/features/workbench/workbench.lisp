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
                    :icon (cond (nil? it.kind) (if (pos? it.depth) "chevron-right" "box")
                                (#{"function" "method" "constructor"} it.kind) "parentheses"
                                (#{"class" "struct" "interface"} it.kind) "component"
                                (#{"namespace" "module" "package"} it.kind) "braces"
                                (#{"variable" "field" "property" "constant"} it.kind) "variable"
                                (#{"enum" "enumMember"} it.kind) "list-tree"
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
  (if (= computed.activeDoc.kind :file)
    (do
      (when-some [_ computed.lspLanguage] (lspAsk "outline" nil nil "" ""))
      (when (not= state.outlineSymbols computed.activeDoc.id)
        (let [{:keys [language text]} computed.activeDoc
              lines (map-indexed (fn [index row] {:line (inc index) :row row}) (take 20000 (str/split text "\n")))]
          (outlineOf (if (= language "Markdown") [] (map #(assoc % :code (str/trim (:row %))) lines))
                     (if (= language "Markdown") (filter #(str/starts-with? (:row %) "#") lines) [])
                     (= language "Python")))))
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
  (->> headings
       (filter #(#{"# " "## " "### " "#### "} (slice (:row %) 0 (inc (indexOf (:row %) " ")))))
       (map (fn [{:keys [line row]}]
              {:line line :depth (dec (indexOf row " ")) :label (slice (str/trim (slice row (indexOf row " "))) 0 120)}))))

(defn pythonOutline "A Python file's classes and functions, each as deep as it is indented (3 levels at most)." [rows]
  (->> rows
       (filter (fn [{:keys [code]}] (some #(str/starts-with? code %) ["def " "class " "async def "])))
       (map (fn [{:keys [line row code]}]
              {:line line
               :indent (indexOf row code)
               :tab (str/starts-with? row "\t")
               :kind (if (str/starts-with? code "class ") :class :function)
               :rest (str/trim (subs code (cond (str/starts-with? code "async def ") 10 (str/starts-with? code "def ") 4 :else 6)))}))
       (map (fn [{:keys [line indent tab kind] text :rest}]
              (let [paren (indexOf text "(")
                    colon (indexOf text ":")]
                {:line line
                 :depth (min (cond tab indent (>= indent 12) 3 (>= indent 8) 2 (>= indent 1) 1 :else 0) 3)
                 :kind kind
                 :label (slice (slice text 0 (cond (pos? paren) paren (pos? colon) colon :else (count text))) 0 120)})))))

(defn braceOutline
  "A file's lines that open a block with {, at the top or one level in: classes, namespaces and functions."
  [rows]
  (->> rows
       (filter #(and (str/ends-with? (:code %) "{") (not (#{"}" "/" "#"} (slice (:code %) 0 1)))))
       (map (fn [{:keys [line row code]}]
              {:line line
               :indent (indexOf row code)
               :tab (str/starts-with? row "\t")
               :label (slice (str/trim (slice code 0 -1)) 0 120)}))
       (filter (fn [{:keys [label indent tab]}]
                 (and (seq label) (or (zero? indent) (if tab (= indent 1) (<= indent 4))))))
       (map (fn [{:keys [line indent label]}]
              {:line line
               :depth (if (zero? indent) 0 1)
               :label label
               :kind (cond
                       (#{"class " "struct" "interf"} (slice label 0 6)) :class
                       (str/starts-with? label "namespace ") :namespace
                       (and (str/includes? label "(") (not (#{"if " "if(" "for" "whi" "swi"} (slice label 0 3)))) :function
                       :else nil)}))))

(defn outlinePicked "A symbol of the outline, picked: the caret goes to its line." []
  (let [line (number (first (str/split event.value ":")))]
    (when (> line 0) (goToLine line))))

(defn outlineFollowsCaret "The caret moved: the outline is read again once it rests 300 ms." []
  (debounce 300 refreshOutline))

(defpart outline-follows "The outline follows the caret, once it rests."
  :watch {:state.caret outlineFollowsCaret})

;;; Breadcrumbs, the minimap and the diff's layout

(defcomputed symbolTrail
  (filter #(some? %)
            (reduce (fn [acc it]
                      (concat (slice (concat acc [nil nil nil nil nil nil nil nil]) 0 (min it.depth 7)) [it]))
                    []
                    (filter #(<= (:line %) state.caret.line) state.outline))))

(defcustom editor.minimap.enabled false
  "Show a narrow overview of the file at the editor's right edge; press or drag it to scroll."
  :description ~(t "settings.editor.minimap.enabled.about")
  :type boolean
  :title ~(t "settings.editor.minimap.enabled"))

(defcommand toggleMinimap "Toggle Minimap"
  :title ~(t "command.toggleMinimap")
  :description ~(t "command.toggleMinimap") :category ~(t "category.view") :icon "map" []
  (set "settings.editor.minimap.enabled" (not settings.editor.minimap.enabled)))

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
  (let [parts (str/split path "/")]
    (if (< si (dec (count parts)))
      (revealFolder (str/join "/" (take (inc si) parts)))
      (goToSymbol))))

(defn revealFolder "Opens the explorer with a folder and its parents open, the folder chosen." [path]
  (explorerShow path)
  (when-not (contains state.explorer.expanded path)
    (set "state.explorer.expanded" (concat state.explorer.expanded [path]))
    (explorerList path))
  (set "state.sideView" :explorer)
  (set "state.layout.sidebar" true))

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
       (take 10
             (concat [{:id id :title label :detail (str/join "/" (butlast (str/split id "/")))}]
                     (remove #(= (:id %) id) (or-else state.quickPick.recent [])))))
  (refreshOutline))

(defcommand openRecent "Go to File, with the recently opened files first."
  :title ~(t "command.openRecent") :category ~(t "category.file") :icon "history"
  :description ~(t "command.openRecent.about") []
  (quickOpen))

(defcommand clearRecent "Clear Recently Opened"
  :title ~(t "command.clearRecent") :description ~(t "command.clearRecent") :category ~(t "category.file") :icon "trash"
  :enablement ~(pos? (count (or-else state.quickPick.recent []))) []
  (set "state.quickPick.recent" []))

;;; Editor groups and tabs

(defn moveTabToGroup
  "A tab dropped on group `to`'s strip at `at`: it leaves its group, which closes if empty."
  [id to at]
  (when-some [from (holdingTab id to)]
    (let [others (remove #(= (:id %) id) (get-in state.groups [to :tabs]))]
      (set (str "state.groups[" to "].tabs")
           (concat (take at others) [(find #(= (:id %) id) (get-in state.groups [from :tabs]))] (drop at others))))
    (set (str "state.groups[" to "].active") id)
    (closeTab from id)
    (set "state.activeGroup" (indexOf (map #(= (:active %) id) state.groups) true))))

(defn holdingTab "The index of the group other than `to` that has the tab `id`, or nil." [id to]
  (first (filter (fn [g] (and (not= g to) (some #(= (:id %) id) (get-in state.groups [g :tabs]))))
                 (range (count state.groups)))))

(defn without "A list without its item at `at`." [items at]
  (concat (take at items) (drop (inc at) items)))

(defn reorderTab "A tab dragged along its strip, from one place to another." [gi from to]
  (let [tabs (get-in state.groups [gi :tabs])
        others (without tabs from)]
    (set (str "state.groups[" gi "].tabs") (concat (take to others) [(get tabs from)] (drop to others)))))

(defcommand moveEditorToNextGroup "Move the active editor into the group at its right, splitting if there is none."
  :title ~(t "command.moveEditorToNextGroup") :category ~(t "category.view") :icon "arrow-right"
  :description ~(t "command.moveEditorToNextGroup.about")
  :enablement ~(and (not (empty? (get-in state.groups [state.activeGroup :active])))
                    (or (< state.activeGroup (dec (count state.groups)))
                        (> (count (get-in state.groups [state.activeGroup :tabs])) 1)))
  []
  (when (= state.activeGroup (dec (count state.groups))) (push "state.groups" {:tabs [] :active nil}))
  (moveTabToGroup (activeTabId) (inc state.activeGroup) 1000))

(defcommand moveEditorToPreviousGroup "Move Editor into Previous Group"
  :title ~(t "command.moveEditorToPreviousGroup")
  :description ~(t "command.moveEditorToPreviousGroup") :category ~(t "category.view") :icon "arrow-left"
  :enablement ~(and (not (empty? (get-in state.groups [state.activeGroup :active]))) (> state.activeGroup 0)) []
  (moveTabToGroup (activeTabId) (dec state.activeGroup) 1000))

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
  :enablement ~(some #(and (= (:id %) (get-in state.groups [state.activeGroup :active])) (not (:sticky %))) (get-in state.groups [state.activeGroup :tabs])) []
  (let [{:keys [tabs active]} (get state.groups state.activeGroup)
        pinned? #(or (:sticky %) (= (:id %) active))]
    (set (str "state.groups[" state.activeGroup "].tabs")
         (concat (map #(assoc % :sticky true :preview false) (filter pinned? tabs)) (remove pinned? tabs)))))

(defcommand unpinEditor "Unpin Editor"
  :title ~(t "command.unpinEditor") :description ~(t "command.unpinEditor") :category ~(t "category.view")
  :icon "pin-off"
  :enablement ~(some #(and (= (:id %) (get-in state.groups [state.activeGroup :active])) (:sticky %))
                     (get-in state.groups [state.activeGroup :tabs]))
  []
  (let [tabs (activeTabs)
        active (activeTabId)
        others (remove #(= (:id %) active) tabs)]
    (set (str "state.groups[" state.activeGroup "].tabs")
         (concat (filter :sticky others)
                 (map #(assoc % :sticky false) (filter #(= (:id %) active) tabs))
                 (remove :sticky others)))))

(defcommand closeOtherEditors "Close the active group's editors but the active one and the pinned ones."
  :title ~(t "command.closeOtherEditors") :category ~(t "category.view")
  :description ~(t "command.closeOtherEditors.about")
  :enablement ~(some #(and (not= (:id %) (get-in state.groups [state.activeGroup :active])) (not (:sticky %)))
                     (get-in state.groups [state.activeGroup :tabs]))
  []
  (doseq [tab (filter #(and (not= (:id %) (activeTabId)) (not (:sticky %))) (activeTabs))]
    (closeTab state.activeGroup tab.id)))

;;; settings.json and keybindings.json

(defstate settingsJson "")

(defstate settingsJsonError "")

(defcomputed keybindingsJson
  ; style: keep, #(...) can't give a map
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

;;; Keys, in the order the keyboard shortcuts page lists them

(defkey "mod+alt+b" toggleSecondarySidebar)
(defkey "mod+ctrl+right" moveEditorToNextGroup)
(defkey "mod+ctrl+left" moveEditorToPreviousGroup)
(defkey "ctrl+r" openRecent)
(defkey "mod+1" focusFirstGroup)
(defkey "mod+2" focusSecondGroup)
(defkey "mod+k shift+enter" pinEditor)
(defkey "mod+k mod+shift+enter" unpinEditor)
(defkey "mod+k u" closeOtherEditors)
