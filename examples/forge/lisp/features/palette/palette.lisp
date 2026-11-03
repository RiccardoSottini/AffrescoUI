;;; palette.lisp --- Forge's palette: one quick pick with four providers (files by default, '>' commands, ':' go to line, 'theme ' colour themes, 'task ' the tasks Run Task read), and the Settings and Keyboard Shortcuts pages, both generated from what the features contribute (the settings and commands sections and the keys).

;;; State and definitions

(defstate quickPick {:open false :query ""})

(defstate goTo {:line 0 :request 0})

(defstate keysQuery "")

(defstate settingsQuery "")

(defcomputed commandItems
  (map (fn [it]
           {:title (+ (if it.category (str it.category ": ") "") it.title)
            :detail (or-else it.keyLabel "")
            :run it.name})
         (filter (fn [it] it.enabled) commands)))

(defcomputed indentItems
  (map (fn [it]
           {:title (str "Spaces: " (text it))
            :detail (if (= it (or-else settings.editor.tabSize 4)) "Current" "")
            :run "setTabSize"
            :with {:spaces it}})
         [2 4 8]))

(defcomputed eolItems
  (map (fn [it]
           {:title it
            :detail (if (= it (or-else computed.activeDoc.eol "LF")) "Current" "")
            :run "setEol"
            :with {:eol it}})
         ["LF" "CRLF"]))

(defcomputed languageItems
  (map (fn [it]
           {:title it
            :detail (if (= (if (= it "Plain Text") "" it) (or-else computed.activeDoc.language "")) "Current" "")
            :run "setLanguage"
            :with {:language (if (= it "Plain Text") "" it)}})
         (concat (filter (fn [it index] (= (indexOf (vals state.languages) it) index)) (vals state.languages))
                 ["Plain Text"])))

(defcomputed lineItems
  (if (> (or-else (number (slice (or-else state.quickPick.query "") 1)) 0) 0)
        [{:title (str "Go to line " (trim (slice state.quickPick.query 1)))
          :line (number (slice state.quickPick.query 1))}]
        []))

(defcomputed settingsGroups (map (fn [it] (get (split it.key ".") 0)) settingsList))

(defcomputed settingsToc
  (concat [{:query "" :label "All settings"}]
            (map (fn [it]
                   {:query (str it ".")
                    :label (cond (= it "editor") "Text Editor"
                                 (= it "diffEditor") "Diff Editor"
                                 (= it "accounts") "Accounts"
                                 :else (+ (upper-case (slice it 0 1)) (slice it 1)))})
                 (filter (fn [it index] (= (indexOf computed.settingsGroups it) index)) computed.settingsGroups))))

(defcomputed settingsRows
  (filter (fn [it]
              (contains (lower-case (str it.title " " it.key " " (or-else it.description "")))
                        (lower-case state.settingsQuery)))
            settingsList))

(defcomputed keyRows
  (map-indexed (fn [index it]
                   {:id index :cells [(or-else it.title it.command "") it.keyLabel (or-else it.when "") it.scope]})
                 (filter (fn [it]
                           (or (empty? state.keysQuery)
                               (contains (lower-case (str (or-else it.title it.command "") " " it.keyLabel))
                                         (lower-case state.keysQuery))))
                         keybindings)))

(defkey "mod+p" quickOpen)

(defkey "mod+shift+p" showCommands)

(defkey "mod+g" goToLinePick)

(defkey "mod+," openSettings)

(defkey "mod+k mod+s" openKeybindings)

(defcustom workbench.statusBar.visible
  true "Show the status bar at the bottom of the window." :description ~(t "settings.workbench.statusBar.visible.about") :type boolean :title ~(t "settings.workbench.statusBar.visible"))

(defcustom workbench.activityBar.visible
  true "Show the activity bar on the left." :description ~(t "settings.workbench.activityBar.visible.about") :type boolean :title ~(t "settings.workbench.activityBar.visible"))

(defcustom workbench.startupEditor
  "welcome" "What the editor area shows when no file is open." :description ~(t "settings.workbench.startupEditor.about") :type string :enum [welcome none] :title ~(t "settings.workbench.startupEditor"))

(defcustom editor.lineNumbers
  true "Show line numbers in the editor's gutter." :description ~(t "settings.editor.lineNumbers.about") :type boolean :title ~(t "settings.editor.lineNumbers"))

;;; Functions and commands

(defcommand quickOpen "Go to File..."
  :title ~(t "command.quickOpen") :category ~(t "category.go") :icon "search" []
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand showCommands "Run a Command..."
  :title ~(t "command.showCommands") :category ~(t "category.view") :icon "search" []
  (set "state.quickPick.query" ">")
  (set "state.quickPick.open" true))

(defcommand goToLinePick "Go to Line..."
  :title ~(t "command.goToLinePick") :category ~(t "category.go") :icon "hash" []
  (set "state.quickPick.query" ":")
  (set "state.quickPick.open" true))

(defcommand changeIndentation "Change Indentation"
  :title ~(t "command.changeIndentation") :category ~(t "category.view") :icon "indent-increase"
  :enablement ~(= computed.activeDoc.kind "file") []
  (set "state.quickPick.query" "indent ")
  (set "state.quickPick.open" true))

(defcommand changeEndOfLine "Change End of Line Sequence"
  :title ~(t "command.changeEndOfLine") :category ~(t "category.view") :icon "corner-down-left"
  :enablement ~(= computed.activeDoc.kind "file") []
  (set "state.quickPick.query" "eol ")
  (set "state.quickPick.open" true))

(defcommand changeLanguageMode "Change Language Mode"
  :title ~(t "command.changeLanguageMode") :category ~(t "category.view") :icon "braces"
  :enablement ~(= computed.activeDoc.kind "file") []
  (set "state.quickPick.query" "language ")
  (set "state.quickPick.open" true))

(defn setTabSize "spaces: per indentation level, for every editor (a file's text keeps the spaces it has)" [spaces]
  (set "settings.editor.tabSize" spaces))

(defn setEol "eol: 'LF' or 'CRLF', the line endings the active file is saved with. A change to save, like an edit"
  [eol]
  (doseq [doc (filter (fn [it] (= it.id computed.activeDoc.id)) state.docs)]
    (markDirty doc.id (or (not= doc.text doc.saved) (not= eol doc.savedEol)))
    (set "state.docs" (map (fn [it] (if (= it.id doc.id) (merge it {:eol eol}) it)) state.docs))))

(defn setLanguage "language: the active file's, as state.languages names them ('' is plain text)" [language]
  (set "state.docs"
       (map (fn [it] (if (= it.id computed.activeDoc.id) (merge it {:language language}) it)) state.docs)))

(defn openPicked "TODO" [item]
  (openPath item.id item.title))

(defn goToPicked "TODO" [item]
  (goToLine item.line))

(defn goToLine "TODO" [line]
  (set "state.goTo.line" line)
  (increment "state.goTo.request" :by 1))

(defcommand openSettings "Open Settings"
  :title ~(t "command.openSettings") :category ~(t "category.preferences") :icon "settings" []
  (openPage "forge:settings" "Settings" "settings"))

(defcommand openKeybindings "Open Keyboard Shortcuts"
  :title ~(t "command.openKeybindings") :category ~(t "category.preferences") :icon "keyboard" []
  (openPage "forge:keybindings" "Keyboard Shortcuts" "keybindings"))
