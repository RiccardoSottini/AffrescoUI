;;; palette.lisp --- Forge's palette, and the settings and keyboard shortcuts pages
;;;
;;; One quick pick with providers chosen by the query's start: files by default, '>' commands, ':' go to line,
;;; 'theme ' colour themes, 'task ' the tasks Run Task offers, 'indent ', 'eol ' and 'language ' the status bar's
;;; pickers. The settings page lists every setting the features define, searchable, in groups; the keyboard
;;; shortcuts page lists every key.

(ns forge.features.palette.palette)

;;; The quick pick

(defstate quickPick {:open false :query ""})

(defstate goTo {:line 0 :request 0})

(defcomputed commandItems
  (map (fn [it]
           {:title (if (or (empty? it.category) (= it.category it.title)) it.title (str it.category ": " it.title))
            :detail (or-else it.keyLabel "")
            :run it.name})
         (filter :enabled commands)))

(defcomputed lineItems
  (if (> (or-else (number (slice (or-else state.quickPick.query "") 1)) 0) 0)
        [{:title (t "pick.goToLine" {:line (trim (slice state.quickPick.query 1))})
          :line (number (slice state.quickPick.query 1))}]
        []))

(defcommand quickOpen "Go to File..."
  :title ~(t "command.quickOpen") :description ~(t "command.quickOpen") :category ~(t "category.go") :icon "search" []
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand showCommands "Run a Command..."
  :title ~(t "command.showCommands")
  :description ~(t "command.showCommands") :category ~(t "category.view") :icon "search" []
  (set "state.quickPick.query" ">")
  (set "state.quickPick.open" true))

(defcommand goToLinePick "Go to Line..."
  :title ~(t "command.goToLinePick")
  :description ~(t "command.goToLinePick") :category ~(t "category.go") :icon "hash" []
  (set "state.quickPick.query" ":")
  (set "state.quickPick.open" true))

(defn open-picked "A file picked in the palette opens in a tab." [item]
  (open-path item.id item.title))

(defn go-to-picked "A line picked in the palette: the caret goes there." [item]
  (go-to-line item.line))

(defn go-to-line "The active editor's caret goes to a line, from 1." [line]
  (set "state.goTo.line" line)
  (increment "state.goTo.request" :by 1))

(defkey "mod+p" quickOpen)

(defkey "mod+shift+p" showCommands)

(defkey "mod+g" goToLinePick)

;;; The status bar's pickers

(defcomputed indentItems
  (map (fn [it]
           {:title (t "pick.spaces" {:n it})
            :detail (if (= it (or-else settings.editor.tabSize 4)) (t "pick.current") "")
            :run "setTabSize"
            :with {:spaces it}})
         [2 4 8]))

(defcomputed eolItems
  (map (fn [it]
           {:title it
            :detail (if (= it (or-else computed.activeDoc.eol "LF")) (t "pick.current") "")
            :run "setEol"
            :with {:eol it}})
         ["LF" "CRLF"]))

(defcomputed languageItems
  (map (fn [it]
         {:title (if (empty? it) (t "pick.plainText") it)
          :detail (if (= it (or-else computed.activeDoc.language "")) (t "pick.current") "")
          :run "setLanguage"
          :with {:language it}})
       (concat (filter (fn [it index] (= (indexOf (vals state.languages) it) index)) (vals state.languages)) [""])))

(defcommand changeIndentation "Change Indentation"
  :title ~(t "command.changeIndentation")
  :description ~(t "command.changeIndentation") :category ~(t "category.view") :icon "indent-increase"
  :enablement ~(= computed.activeDoc.kind :file) []
  (set "state.quickPick.query" "indent ")
  (set "state.quickPick.open" true))

(defcommand changeEndOfLine "Change End of Line Sequence"
  :title ~(t "command.changeEndOfLine")
  :description ~(t "command.changeEndOfLine") :category ~(t "category.view") :icon "corner-down-left"
  :enablement ~(= computed.activeDoc.kind "file") []
  (set "state.quickPick.query" "eol ")
  (set "state.quickPick.open" true))

(defcommand changeLanguageMode "Change Language Mode"
  :title ~(t "command.changeLanguageMode")
  :description ~(t "command.changeLanguageMode") :category ~(t "category.view") :icon "braces"
  :enablement ~(= computed.activeDoc.kind "file") []
  (set "state.quickPick.query" "language ")
  (set "state.quickPick.open" true))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setTabSize "The spaces per indentation level, for every editor (a file's text keeps the spaces it has)."
  [spaces]
  (set "settings.editor.tabSize" spaces))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setEol "The line endings the active file is saved with, LF or CRLF: a change to save, like an edit."
  [eol]
  (doseq [doc (filter #(= (:id %) computed.activeDoc.id) state.docs)]
    (mark-dirty doc.id (or (not= doc.text doc.saved) (not= eol doc.savedEol)))
    (set "state.docs" (map #(if (= (:id %) doc.id) (assoc % :eol eol) %) state.docs))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setLanguage "The active file's language, as state.languages names them ('' is plain text)." [language]
  (set "state.docs" (map #(if (= (:id %) computed.activeDoc.id) (assoc % :language language) %) state.docs)))

;;; The settings and keyboard shortcuts pages

(defstate settingsQuery "")

(defstate keysQuery "")

(defcomputed settingsGroups (map #(get (split (:key %) ".") 0) settingsList))

(defcomputed settingsToc
  (concat [{:query "" :label (t "settingsToc.all")}]
            (map (fn [it]
                   {:query (str it ".")
                    :label (case it
                             "editor" (t "settingsToc.editor")
                             "diffEditor" (t "settingsToc.diffEditor")
                             "accounts" (t "settingsToc.accounts")
                             (+ (upper-case (slice it 0 1)) (slice it 1)))})
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

(defcommand openSettings "Open Settings"
  :title ~(t "command.openSettings")
  :description ~(t "command.openSettings") :category ~(t "category.preferences") :icon "settings" []
  (open-page "forge:settings" (t "page.settings") "settings"))

(defcommand openKeybindings "Open Keyboard Shortcuts"
  :title ~(t "command.openKeybindings")
  :description ~(t "command.openKeybindings") :category ~(t "category.preferences") :icon "keyboard" []
  (open-page "forge:keybindings" (t "page.keybindings") "keybindings"))

(defkey "mod+," openSettings)

(defkey "mod+k mod+s" openKeybindings)

;;; Settings

(defn show-settings-group "The settings page shows one group's settings ('' for every one)." [query]
  (set "state.settingsQuery" query))

(defn set-setting "A setting's new value, by its key." [key value]
  (set (str "settings." key) value))

(defn set-number-setting "A number typed for a setting, kept once it reads as one." [key text]
  (when-some [n (number text)] (set-setting key n)))

(defn toggle-setting "A boolean setting turned the other way, from its value now." [key value]
  (set (str "settings." key) (not (true? value))))

(defn reset-setting "A setting back to its default: the value set is taken away." [key]
  (remove-state (str "settings." key)))

(defcustom workbench.statusBar.visible true
  "Show the status bar at the bottom of the window."
  :description ~(t "settings.workbench.statusBar.visible.about")
  :type boolean
  :title ~(t "settings.workbench.statusBar.visible"))

(defcustom workbench.activityBar.visible true
  "Show the activity bar on the left."
  :description ~(t "settings.workbench.activityBar.visible.about")
  :type boolean
  :title ~(t "settings.workbench.activityBar.visible"))

(defcustom workbench.startupEditor "welcome"
  "What the editor area shows when no file is open."
  :description ~(t "settings.workbench.startupEditor.about")
  :type string
  :enum [welcome none]
  :title ~(t "settings.workbench.startupEditor"))

(defcustom editor.lineNumbers true
  "Show line numbers in the editor's gutter."
  :description ~(t "settings.editor.lineNumbers.about")
  :type boolean
  :title ~(t "settings.editor.lineNumbers"))