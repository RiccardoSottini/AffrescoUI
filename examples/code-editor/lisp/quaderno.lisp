;;; quaderno.lisp --- what Quaderno does: its files and tabs, the console, the search results, and its editing commands
;;;
;;; The screen (index.screen.lisp) says what is shown, and keeps the one-step actions and the text scans (the problems,
;;; the symbols, Find in Files), which the expressions run fastest. This file holds the rest. A function reads the
;;; running app through paths (`state.tabs`, `computed.resultsFile`, `event.value`) and changes it only through steps
;;; (`set`, `push`, `edit`...). A handler runs one by name, as it would an action: {:run open :with {:file 2}}.

;;; A form the others use

(defmacro each "Runs the body once for each item of a list, the item bound to a name: (each [f files] (close f))."
  [[item items] & body]
  `(reduce (fn [_ ~item] ~@body nil) nil ~items))

;;; Files and tabs

(defn file-at "The file at an index of state.files." [f]
  (get state.files f))

(defn place "The path of a key of a file in the state, for a step: (place 2 \"text\") is state.files[2].text." [f key]
  (str "state.files[" f "]." key))

(defn filled? "Whether a value is there and isn't empty text, as an expression's truth has it." [v]
  (and (some? v) (not= v "")))

(defn either "A value, or another when it is nil: an expression's ??." [v otherwise]
  (if (nil? v) otherwise v))

(defn path-of "A file's path in the project: its folder, then its name." [file]
  (str (if (filled? (:folder file)) (str (:folder file) "/") "") (:name file)))

(defn open "Shows a file in a tab, giving it one if it has none." [file]
  (when (not (contains state.tabs file)) (push "state.tabs" file))
  (set "state.active" file))

(defn closeNow "Closes a file's tab: the next tab takes its place, and Reopen Closed File brings it back." [file]
  (set "state.closing.open" false)
  (push "state.closed" file)
  (remove-state "state.tabs" :at (indexOf state.tabs file))
  (when (not (contains state.tabs state.active))
    (let [tabs state.tabs]
      (set "state.active"
           (if (> (count tabs) 0) (get tabs (min (dec (count tabs)) (max 0 (indexOf tabs state.active)))) -1)))))

(defn close "Closes a file's tab; an unsaved file asks first." [file]
  (if (:dirty (file-at file))
    (set "state.closing" {:open true :file file})
    (closeNow file)))

(defn closeSaved "Closes the tabs of those files that are saved." [files]
  (map (fn [f] (when (not (:dirty (file-at f))) (closeNow f))) files))

(defn applySettings
  "Saves the Settings file and applies its values; a broken one stays unsaved, with an error in the console."
  [file]
  (let [values (parseJsonc (:text (file-at file)))]
    (if (or (nil? values) (not= (type values) "object"))
      (do (push "state.terminal" {:t (t "cmd.settingsJsonNotValidJson") :c "error"})
          (set "state.panelOpen" true)
          (set "state.panelTab" "terminal"))
      (do (set "state.theme" (if (contains ["dark" "light" "system"] (:theme values)) (:theme values) state.theme))
          (set "state.wrap" (either (:wordWrap values) state.wrap))
          (set "state.ruler" (either (:ruler values) state.ruler))
          (set "state.minimap" (either (:minimap values) state.minimap))
          (set "state.sidebarOpen" (either (:sideBar values) state.sidebarOpen))
          (set (place file "saved") (:text (file-at file)))
          (set (place file "dirty") false)))))

(defn saveFile "Saves a file; the Settings file applies its values instead." [file]
  (if (:settings (file-at file))
    (applySettings file)
    (do (set (place file "saved") (:text (file-at file)))
        (set (place file "dirty") false))))

(defn saveAndClose "Saves the file being closed, then closes it." []
  (saveFile state.closing.file)
  (run "findProblems")
  (closeNow state.closing.file))

(defn discardAndClose "Throws away the changes to the file being closed, then closes it." []
  (let [f state.closing.file]
    (set (place f "text") (:saved (file-at f)))
    (set (place f "dirty") false)
    (closeNow f)))

(defn closeOthersOf "Shows a tab and closes the other saved ones." [file]
  (set "state.active" file)
  (closeSaved (filter (fn [f] (not= f state.active)) state.tabs)))

(defn closeToTheRight "Closes the saved tabs after a file's." [file]
  (closeSaved (drop (inc (indexOf state.tabs file)) state.tabs)))

(defn openVirtual "Opens the file of that name outside the project, made with the text the first time."
  [name text readOnly]
  (let [found (fn [] (let [files state.files]
                       (find (fn [f] (and (= (:name (get files f)) name) (nil? (:folder (get files f)))))
                             (range (count files)))))]
    (when (nil? (found))
      (push "state.files" {:name name :folder nil :language "JSON" :dirty false :text text :saved text
                           :readOnly readOnly :settings (not readOnly)}))
    (open (found))))

(defn pickTreeRow "A file row opens the file, a folder row opens or closes the folder." []
  (let [row event.value]
    (if (= (slice row 0 2) "d:")
      (set "state.expanded" (if (contains state.expanded row)
                              (filter (fn [r] (not= r row)) state.expanded)
                              (conj state.expanded row)))
      (open (number (slice row 1))))))

;;; Places in a file

(defn goToPlace "Goes to a row with a file (opened) and a line, and selects its column and length if it has them."
  [item]
  (cond (and (some? (:file item)) (>= (:file item) 0) (not= (:file item) state.active))
        (do (set "state.place" item)
            (open (:file item)))

        (nil? (:line item)) nil

        :else
        (let [column (either (:column item) 1)]
          (focus "#editor")
          (edit "select" :in "#editor" :line (:line item) :column column
                :toLine (:line item) :toColumn (+ column (either (:length item) 0))))))

(defn openProblem "A problem row chosen: its id is 'file:line'." []
  (let [id (either event.id event.value)
        at (split id ":")]
    (set "state.problemPick" id)
    (goToPlace {:file (number (get at 0)) :line (number (get at 1)) :column 1 :length 0})))

(defn openResult
  "A line of the Search Results buffer: a match line ('   12: text') opens its file, the path line above it, there."
  [line]
  (let [lines (split (:text (file-at computed.resultsFile)) "\n")]
    (when (contains computed.resultLines line)
      (set "state.results.at" line)
      (let [match (get lines (dec line))
            heading (find (fn [l] (and (not= (slice l 0 1) " ") (= (slice l -1) ":")))
                          (reverse (take (dec line) lines)))
            path (slice heading 0 -1)
            found (slice match (+ (indexOf match ": ") 2))
            files state.files]
        (goToPlace {:file (either (find (fn [f] (= (path-of (get files f)) path)) (range (count files))) -1)
                    :line (number (trim (get (split match ":") 0)))
                    :column (inc (indexOf (lower found) (lower state.results.query)))
                    :length (length state.results.query)})))))

;;; The console

(defn runCommand "Runs what the console's prompt holds." []
  (when (filled? (trim state.command))
    (push "state.terminal" {:t (str "$ " state.command) :c "prompt"})
    (if (= state.command "clear")
      (set "state.terminal" [])
      (map (fn [out] (push "state.terminal" out))
           (either (get state.outputs state.command)
                   [{:t (str state.command ": command not found (try help)") :c "string"}])))
    (set "state.command" "")))

(defn typeCommand "Runs a console command as if typed." [name]
  (set "state.panelOpen" true)
  (set "state.panelTab" "terminal")
  (set "state.command" name)
  (runCommand))

(defcommand build "Runs the build in the console."
  :description ~(t "command.build.about") :title ~(t "command.build") :category ~(t "category.tools") :icon hammer []
  (typeCommand "build"))

(defcommand runApp "Runs the program in the console."
  :description ~(t "command.runApp.about") :title ~(t "command.runApp") :category ~(t "category.tools") :icon play []
  (typeCommand "run"))

;;; At the start

(defn sensor-line "A line of the generated file: a function every 25 lines, a TODO every 1,000." [i]
  (let [sensor (/ (- i (rem i 25)) 25)]
    (cond (= i 0) "// Calibration tables for the station sensors (generated, 5,000 lines)"
          (= (rem i 25) 0) ""
          (= (rem i 25) 1) (str "double sensor" sensor "(double raw) {")
          (= (rem i 25) 23) "    return raw;"
          (= (rem i 25) 24) "}"
          (= (rem i 1000) 507) (str "    // TODO: recalibrate sensor " sensor " after the winter readings")
          :else (str "    raw += " (rem (* i 37) 1000) " / 1000.0;"))))

(defn indexAll "Finds the symbols of every file, then the problems." []
  (map (fn [f] (run "indexFile" :with {:file f})) (range (count state.files)))
  (run "findProblems"))

(defn prepare
  "Generates the large file once (5,000 lines), saves every file as it is, then finds the symbols and the problems."
  []
  (set (place 3 "text") (join "\n" (map sensor-line (range 5000))))
  (map-indexed (fn [f text] (set (place f "saved") text)) (map (fn [x] (:text x)) state.files))
  (set "state.symbolIndex" (map (fn [f] []) state.files))
  (indexAll))

;;; File commands

(def file-open "When a command needs a file open." "{{state.active >= 0}}")

(defcommand save "Saves this file."
  :description ~(t "command.save.about") :title ~(t "command.save") :category ~(t "category.file") :icon save
  :enablement ~file-open []
  (saveFile state.active)
  (run "findProblems"))

(defcommand saveAll "Saves every file."
  :description ~(t "command.saveAll.about") :title ~(t "command.saveAll") :category ~(t "category.file") :icon save []
  (map saveFile (range (count state.files)))
  (run "findProblems"))

(defcommand closeTab "Closes this file."
  :description ~(t "command.closeTab.about") :title ~(t "command.closeTab") :category ~(t "category.file") :icon x
  :enablement ~file-open []
  (close state.active))

(defcommand closeAll "Closes every saved file, then asks about the first unsaved one."
  :description ~(t "command.closeAll.about") :title ~(t "command.closeAll")
  :category ~(t "category.file") :enablement "{{length(state.tabs) > 0}}"
  []
  (closeSaved state.tabs)
  (when (> (count state.tabs) 0) (close (first state.tabs))))

(defcommand closeOthers "Closes the other saved files."
  :description ~(t "command.closeOthers.about") :title ~(t "command.closeOthers") :category ~(t "category.file")
  :enablement "{{length(state.tabs) > 1}}"
  []
  (closeSaved (filter (fn [f] (not= f state.active)) state.tabs)))

(defcommand reopenClosed "Opens the file closed last."
  :description ~(t "command.reopenClosed.about") :title ~(t "command.reopenClosed") :category ~(t "category.file")
  :enablement "{{length(state.closed) > 0}}"
  []
  (open (last state.closed))
  (remove-state "state.closed" :at (dec (count state.closed))))

(defcommand refreshFolders "Finds the symbols and the problems of every file again."
  :description ~(t "command.refreshFolders.about") :title ~(t "command.refreshFolders")
  :category ~(t "category.project")
  []
  (indexAll))

(defcommand openSettings "Opens the settings, as a JSON file."
  :description ~(t "command.openSettings.about") :title ~(t "command.openSettings")
  :category ~(t "category.preferences") :icon settings []
  (openVirtual "Settings.json" computed.settingsText false))

(defcommand openKeyBindings "Opens the key bindings, as a JSON file to read."
  :description ~(t "command.openKeyBindings.about") :title ~(t "command.openKeyBindings")
  :category ~(t "category.preferences")
  :icon keyboard
  []
  (openVirtual "Key Bindings.json" computed.keysText true))

;;; Find

(defn findStep "Asks the editor's find for a step: next, previous, replace, replaceAll or first." [step]
  (set "state.findBar.step" step)
  (increment "state.findBar.request" :by 1))

(defn toggleFind "Turns one of the find panel's options over, and finds again from the start." [option]
  (let [where (str "state.findBar." option)]
    (set where (not (get state.findBar option)))
    (findStep "first")))

(def find-open "When the find panel is open." "{{state.findBar.open}}")

(defcommand findNext ""
  :title ~(t "command.findNext") :category ~(t "category.find") :enablement ~find-open [] (findStep "next"))
(defcommand findPrevious ""
  :title ~(t "command.findPrevious") :category ~(t "category.find") :enablement ~find-open [] (findStep "previous"))
(defcommand replaceOne ""
  :title ~(t "command.replaceOne") :category ~(t "category.find") :enablement ~find-open [] (findStep "replace"))
(defcommand replaceAllInFile ""
  :title ~(t "command.replaceAllInFile") :category ~(t "category.find")
  :enablement ~find-open [] (findStep "replaceAll"))
(defcommand toggleFindRegex ""
  :title ~(t "command.toggleFindRegex") :category ~(t "category.find") [] (toggleFind "regex"))
(defcommand toggleFindCase ""
  :title ~(t "command.toggleFindCase") :category ~(t "category.find") [] (toggleFind "caseSensitive"))
(defcommand toggleFindWord ""
  :title ~(t "command.toggleFindWord") :category ~(t "category.find") [] (toggleFind "wholeWord"))
(defcommand toggleFindInSelection ""
  :title ~(t "command.toggleFindInSelection") :category ~(t "category.find") [] (toggleFind "inSelection"))

;;; The editor's own commands

(defn bound "A form as the binding ~form, in a template a macro writes." [form]
  (list 'unquote form))

(defmacro defedit
  "A command that is the editor's own edit of the same name, while a file is open; its title and category are keys."
  [name category icon]
  `(defcommand ~name "" :title ~(bound (list 't (str "command." name)))
     :category ~(bound (list 't (str "category." category)))
     :icon ~icon :enablement ~(bound '(>= state.active 0)) []
     (edit ~(str name) :in "#editor")))

(defedit undo edit undo-2)
(defedit redo edit redo-2)
(defedit cut edit scissors)
(defedit copy edit copy)
(defedit paste edit clipboard-paste)
(defedit selectAll selection text-select)
(defedit indentLines edit indent)
(defedit outdentLines edit outdent)
(defedit unfoldAll view unfold-vertical)
(defedit duplicateLines edit copy-plus)
(defedit deleteLines edit trash-2)
(defedit moveLinesUp edit arrow-up)
(defedit moveLinesDown edit arrow-down)
(defedit joinLines edit merge)
(defedit insertLineAfter edit corner-down-left)
(defedit insertLineBefore edit corner-up-left)
(defedit transpose edit repeat)
(defedit sortLines edit arrow-down-a-z)
(defedit reverseLines edit arrow-up-down)
(defedit uniqueLines edit list-filter)
(defedit upperCase convertcase case-upper)
(defedit lowerCase convertcase case-lower)
(defedit titleCase convertcase case-sensitive)
(defedit swapCase convertcase case-sensitive)
(defedit expandToLine selection text-select)
(defedit expandToWord selection text-select)
(defedit splitIntoLines selection text-cursor)
(defedit selectAllOccurrences selection text-cursor)

(defcommand toggleComment "Comments the lines out, or back in, with the language's mark."
  :description ~(t "command.toggleComment.about") :title ~(t "command.toggleComment")
  :category ~(t "category.edit") :icon message-square :enablement ~file-open
  []
  (edit "toggleComment" :in "#editor" :prefix computed.commentMark))

;;; What the views do, the scans, and the commands

(defn findAsTyped "Finds the text in the file as it is typed, unless the panel searches the files." []
  (when (not state.findBar.inFiles)
    (findStep "first")))

(defn submitFind "Enter in the find field: searches the files, or finds the next match in the file." []
  (if state.findBar.inFiles (searchFiles) (findNext)))

(defn submitReplace "Enter in the replace field: replaces in the files, or once in the file." []
  (if state.findBar.inFiles (replaceInFiles) (replaceOne)))

(defn showFile "Shows a file's tab." [file]
  (set "state.active" file))

(defn expandFolder "Opens a folder of the tree, or closes it." [id expanded]
  (set "state.expanded" (if expanded (concat state.expanded [id]) (filter (fn [it] (not= it id)) state.expanded))))

(defn openTabMenu "Opens a tab's menu at the pointer; from the keyboard, the active tab's." [x y id]
  (set "state.tabMenu" {:open true :x x :y y :file (if (some? id) (number id) state.active)}))

(defn moveTab "Moves a dragged tab from one place in the tab bar to another." [from to file]
  (remove-state "state.tabs" :at from)
  (set "state.tabs" (concat (slice state.tabs 0 to) [file] (slice state.tabs to))))

(defn editorShown "A file shown in the editor: the caret at its start, or at the place a search asked for." []
  (set "state.caret" {:line 1 :column 1 :selected 0})
  (when (some? state.place)
    (let [place state.place]
      (set "state.place" nil)
      (goToPlace place))))

(defn markDirty "Marks a file as changed since it was saved, unless it is a scratch buffer." [file]
  (set (place file "dirty") (not (:scratch (file-at file)))))

(defn openEditorMenu "Opens the editor's menu at the pointer, kept inside the editor." [x y left top width height]
  (set "state.editorMenu" {:open true :x (min x (- (+ (- x left) width) 240)) :y (min y (- (+ (- y top) height) 204))}))

(defn resultClicked "A double click on a line of the search results opens the file at that line." [file line]
  (when (:scratch (file-at file))
    (openResult line)))

(defn liftAssistant "Keeps the assistant's button above the find panel." []
  (set "state.assistant.lift" computed.findHeight))

(defn line-problems "A file's problems: each line with TODO, a warning, or FIXME, an error." [f]
  (let [lines (split (or-else (get-in state.files [f :saved]) (get-in state.files [f :text])) "\n")]
    (map (fn [at]
           (let [line (get lines at)
                 fixme (contains line "FIXME")]
             {:id (str f ":" (inc at))
              :kind (if fixme "error" "warning")
              :cells [(t (if fixme "problem.error" "problem.warning")) (get-in state.files [f :name]) (inc at)
                      (slice line (indexOf line (if fixme "FIXME" "TODO")))]}))
         (filter (fn [at] (or (contains (get lines at) "TODO") (contains (get lines at) "FIXME")))
                 (range (count lines))))))

(defn findProblems "Every line with TODO or FIXME in the saved files, as rows of the problems table." []
  (set "state.problems" (vec (mapcat line-problems (range (count state.files))))))

(defn symbol-kind "What a line declares: a heading (#), a class or a struct (c), else a function (f)." [text]
  (cond (= (slice text 0 1) "#") "#"
        (contains " class struct " (str " " (first (split text " ")) " ")) "c"
        :else "f"))

(defn symbol-title "The name a line of a kind declares." [text kind]
  (cond (= kind "#") (trim (join "" (split text "#")))
        (= kind "c") (get (split (trim (first (split text "{"))) " ") 1)
        :else (last (split (trim (first (split text "("))) " "))))

(defn declares? "Whether a line declares a symbol: a Markdown heading, or a C++ class, struct or function definition."
  [language text]
  (if (= language "Markdown")
    (= (slice text 0 1) "#")
    (and (= language "C++")
         (< (indexOf " \t#/}" (slice text 0 1)) 0)
         (or (= (slice text 0 6) "class ")
             (= (slice text 0 7) "struct ")
             (and (contains text "(") (not (contains text ";")))))))

(def badge-colors "The colour of each kind of symbol's badge."
  {"#" "@badgeHeading" "c" "@badgeClass" "f" "@badgeFunction"})

(defn indexFile "Finds a file's symbols, for Find Symbol (@) and Find Symbol in Project; a scratch file has none."
  [file]
  (let [it (file-at file)]
    (when (not it.scratch)
      (set (str "state.symbolIndex[" file "]")
           (map (fn [line]
                  (let [kind (symbol-kind line.t)]
                    {:title (symbol-title line.t kind) :badge kind :badgeColor (get badge-colors kind)
                     :detail (str (path-of it) ":" line.line) :file file :line line.line}))
                (filter (fn [line] (declares? it.language line.t))
                        (map-indexed (fn [index text] {:t text :line (inc index)}) (split it.text "\n"))))))))

(defcommand newFile "Opens a new, empty Markdown file."
  :description ~(t "command.newFile.about") :title ~(t "command.newFile") :category ~(t "category.file")
  :icon "file-plus" []
  (push "state.files"
        {:name (str "Untitled-" (- (count state.files) 6) ".md")
         :folder ""
         :language "Markdown"
         :dirty true
         :text ""
         :saved ""})
  (open (- (count state.files) 1)))

(defcommand quickOpen "Opens the Quick Switcher: a file, file@symbol or file:line."
  :description ~(t "command.quickOpen.about") :title ~(t "command.quickOpen") :category ~(t "category.go")
  :icon "search" []
  (when (>= state.active 0)     (indexFile state.active))
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand commandPalette "Opens the palette of every command."
  :description ~(t "command.commandPalette.about") :title ~(t "command.commandPalette")
  :category ~(t "category.view") :icon "command" []
  (set "state.palette.query" "")
  (set "state.palette.open" true))

(defcommand openFind "Opens the find panel on the file."
  :description ~(t "command.openFind.about") :title ~(t "command.openFind") :category ~(t "category.find")
  :icon "search" :enablement ~(>= state.active 0) []
  (set "state.findBar.open" true)
  (set "state.findBar.replacing" false)
  (set "state.findBar.inFiles" false)
  (focus "#findText"))

(defcommand openReplace "Opens the find panel, with Replace."
  :description ~(t "command.openReplace.about") :title ~(t "command.openReplace") :category ~(t "category.find")
  :icon "replace" :enablement ~(>= state.active 0) []
  (set "state.findBar.open" true)
  (set "state.findBar.replacing" true)
  (focus "#findText"))

(defcommand closeFind "Hides the find panel, and gives the editor the keys."
  :description ~(t "command.closeFind.about") :title ~(t "command.closeFind") :category ~(t "category.find")
  :enablement ~state.findBar.open []
  (set "state.findBar.open" false)
  (set "state.findBar.inSelection" false)
  (focus "#editor"))

(defcommand toggleWordWrap "Wraps long lines, or not."
  :description ~(t "command.toggleWordWrap.about") :title ~(t "command.toggleWordWrap")
  :category ~(t "category.view") :icon "wrap" []
  (set "state.wrap" (not state.wrap)))

(defcommand toggleRuler "Shows the ruler at column 80, or hides it."
  :description ~(t "command.toggleRuler.about") :title ~(t "command.toggleRuler") :category ~(t "category.view")
  :icon "ruler" []
  (set "state.ruler" (not state.ruler)))

(defcommand revertFile "Puts the file back as it was last saved."
  :description ~(t "command.revertFile.about") :title ~(t "command.revertFile") :category ~(t "category.file")
  :enablement ~(and (>= state.active 0) (get-in state.files [state.active :dirty])) []
  (set "state.files[state.active].text" (get-in state.files [state.active :saved]))
  (set "state.files[state.active].dirty" false))

(defcommand nextTab "Shows the next tab."
  :description ~(t "command.nextTab.about") :title ~(t "command.nextTab") :category ~(t "category.go")
  :icon "chevron-right" :enablement ~(> (count state.tabs) 1) []
  (set "state.active" (get state.tabs (mod (+ (indexOf state.tabs state.active) 1) (count state.tabs)))))

(defcommand previousTab "Shows the tab before."
  :description ~(t "command.previousTab.about") :title ~(t "command.previousTab") :category ~(t "category.go")
  :icon "chevron-left" :enablement ~(> (count state.tabs) 1) []
  (set "state.active"
       (get state.tabs (mod (- (+ (indexOf state.tabs state.active) (count state.tabs)) 1) (count state.tabs)))))

(defcommand toggleSidebar "Shows the side bar, or hides it."
  :description ~(t "command.toggleSidebar.about") :title ~(t "command.toggleSidebar") :category ~(t "category.view")
  :icon "panel-left" []
  (toggle "state.sidebarOpen"))

(defcommand togglePanel "Shows the bottom panel, or hides it."
  :description ~(t "command.togglePanel.about") :title ~(t "command.togglePanel") :category ~(t "category.view")
  :icon "square-terminal" []
  (toggle "state.panelOpen"))

(defcommand showProblems "Shows the problems in the bottom panel."
  :description ~(t "command.showProblems.about") :title ~(t "command.showProblems") :category ~(t "category.view")
  :icon "alert-triangle" []
  (set "state.panelOpen" true)
  (set "state.panelTab" "problems"))

(defcommand showTerminal "Shows the console in the bottom panel."
  :description ~(t "command.showTerminal.about") :title ~(t "command.showTerminal") :category ~(t "category.view")
  :icon "square-terminal" []
  (set "state.panelOpen" true)
  (set "state.panelTab" "terminal")
  (focus "#command"))

(defcommand clearConsole "Empties the console."
  :description ~(t "command.clearConsole.about") :title ~(t "command.clearConsole") :category ~(t "category.tools")
  :icon "eraser" []
  (set "state.terminal" []))

(defcommand goToSymbol "Finds a symbol of the file (@)."
  :description ~(t "command.goToSymbol.about") :title ~(t "command.goToSymbol") :category ~(t "category.go")
  :icon "at-sign" :enablement ~(>= state.active 0) []
  (indexFile state.active)
  (set "state.quickPick.query" "@")
  (set "state.quickPick.open" true))

(defcommand goToLine "Goes to a line of the file (:)."
  :description ~(t "command.goToLine.about") :title ~(t "command.goToLine") :category ~(t "category.go") :icon "hash"
  :enablement ~(>= state.active 0) []
  (set "state.quickPick.query" ":")
  (set "state.quickPick.open" true))

(defcommand findInFile "Searches the file's lines (#)."
  :description ~(t "command.findInFile.about") :title ~(t "command.findInFile") :category ~(t "category.go")
  :icon "search" :enablement ~(>= state.active 0) []
  (set "state.quickPick.query" "#")
  (set "state.quickPick.open" true))

(defcommand goToSymbolInProject "Finds a symbol in every file of the project."
  :description ~(t "command.goToSymbolInProject.about") :title ~(t "command.goToSymbolInProject")
  :category ~(t "category.go") :icon "at-sign" []
  (indexFile state.active)
  (set "state.symbolPick.query" "")
  (set "state.symbolPick.open" true))

(defcommand showAssistant "Opens the assistant."
  :description ~(t "command.showAssistant.about") :title ~(t "command.showAssistant") :category ~(t "category.tools")
  :icon "message-circle" []
  (set "state.assistant.show" true)
  (run "assistant.open"))

(defcommand findInFiles "Opens the find panel to search every file."
  :description ~(t "command.findInFiles.about") :title ~(t "command.findInFiles") :category ~(t "category.find")
  :icon "folder-search" []
  (set "state.findBar.open" true)
  (set "state.findBar.inFiles" true)
  (set "state.findBar.replacing" true)
  (set "state.findBar.where" (if (filled? state.findBar.where) state.findBar.where "<open folders>"))
  (focus "#findText"))

(defn file-hits
  "A file's lines that hold the text: how many times, and the lines as Find in Files lists them; nil if none."
  [f needle case]
  (let [file (file-at f)
        lines (split (if case file.text (lower-case file.text)) "\n")
        original (split file.text "\n")
        hits (filter (fn [at] (contains (get lines at) needle)) (range (count lines)))]
    (when (> (count hits) 0)
      {:matches (reduce + 0 (map (fn [at] (dec (count (split (get lines at) needle)))) hits))
       :text (str (path-of file) ":\n"
                  (join "\n" (map (fn [at] (str (pad (inc at) 6 " ") ": " (get original at))) hits))
                  "\n\n")})))

(defn searchFiles
  "Writes every matching line of the files 'Where' names into the Search Results buffer, as Find in Files does."
  []
  (when (not (empty? state.findBar.text))
    (scopeFiles)
    (let [query state.findBar.text
          case state.findBar.caseSensitive
          found (filter some? (map (fn [f] (file-hits f (if case query (lower-case query)) case)) state.findBar.scope))
          counts {:searched (count state.findBar.scope)
                  :files (count found)
                  :matches (reduce + 0 (map :matches found))}
          text (str (t "search.header" (merge counts {:query query}))
                    (join "" (map :text found))
                    (t "search.footer" counts))]
      (set "state.results" (merge counts {:query query :at 0 :text text}))
      (if (< computed.resultsFile 0)
        (push "state.files" {:name (t "search.results") :folder nil :language "Search Results" :scratch true
                             :dirty false
                             :text text})
        (set (place computed.resultsFile "text") text))
      (open computed.resultsFile))))

(defn in-scope?
  "Whether Find in Files searches a file, by 'Where': <open files>, <open folders>, or parts of paths; never scratch."
  [f]
  (let [file (file-at f)
        where state.findBar.where]
    (and (not file.scratch)
         (if (= where "<open files>")
           (contains state.tabs f)
           (or (= where "<open folders>")
               (empty? (trim where))
               (some (fn [part] (contains (path-of file) (trim part))) (split where ",")))))))

(defn scopeFiles "Sets the files Find in Files searches, by 'Where'." []
  (set "state.findBar.scope" (vec (filter in-scope? (range (count state.files))))))

(defcommand replaceInFiles
  "Lists the matches in Search Results, then replaces them (plain text, case sensitive) in the files 'Where' names"
  :description ~(t "command.replaceInFiles.about") :title ~(t "command.replaceInFiles")
  :category ~(t "category.find") :enablement ~(and state.findBar.inFiles (not (empty? state.findBar.text))) []
  (searchFiles)
  (each [f (filter (fn [it] (contains (get-in state.files [it :text]) state.findBar.text)) state.findBar.scope)]
        (set (str "state.files[" f "].text")
             (join state.findBar.replace (split (get-in state.files [f :text]) state.findBar.text)))
        (set (str "state.files[" f "].dirty") true)))

(defcommand nextResult "Goes to the next line of the search results."
  :description ~(t "command.nextResult.about") :title ~(t "command.nextResult") :category ~(t "category.find")
  :enablement ~(> (count computed.resultLines) 0) []
  (openResult (or-else (find (fn [it] (> it state.results.at)) computed.resultLines) (first computed.resultLines))))

(defcommand previousResult "Goes to the line of the search results before."
  :description ~(t "command.previousResult.about") :title ~(t "command.previousResult")
  :category ~(t "category.find") :enablement ~(> (count computed.resultLines) 0) []
  (openResult (or-else (find (fn [it] (< it state.results.at)) (reverse computed.resultLines))
                       (get (slice computed.resultLines (- 1)) 0))))

(defcommand toggleMinimap "Shows the minimap, or hides it."
  :description ~(t "command.toggleMinimap.about") :title ~(t "command.toggleMinimap") :category ~(t "category.view")
  :icon "map" []
  (toggle "state.minimap"))

(defcommand toggleZen "Hides everything but the code, or shows it again."
  :description ~(t "command.toggleZen.about") :title ~(t "command.toggleZen") :category ~(t "category.view")
  :icon "maximize" []
  (toggle "state.zen")
  (focus "#editor"))

(defcommand selectTheme "Picks a theme."
  :description ~(t "command.selectTheme.about") :title ~(t "command.selectTheme")
  :category ~(t "category.preferences") :icon "palette" []
  (set "state.themePick"
       {:open true :query "" :index (indexOf (map (fn [it] it.pick) computed.themes) state.theme)}))

(defn setTheme "Takes the theme of a row of the theme picker." [item]
  (set "state.theme" item.pick))

(defcommand toggleTheme "Switches between light and dark."
  :description ~(t "command.toggleTheme.about") :title ~(t "command.toggleTheme")
  :category ~(t "category.preferences") :icon "sun-moon" []
  (set "state.theme" (if (= computed.theme "dark") "light" "dark")))

(defcommand selectSyntax "Picks the syntax of the file."
  :description ~(t "command.selectSyntax.about") :title ~(t "command.selectSyntax") :category ~(t "category.view")
  :icon "code" :enablement ~(>= state.active 0) []
  (set "state.syntaxPick" {:open true :query ""}))

(defn setSyntax "Gives the open file a row of the syntax picker: its language (commands) and its grammar (colours)."
  [item]
  (set "state.files[state.active].language" item.language)
  (set "state.files[state.active].grammar" item.grammar))

(defcommand about "Says what Quaderno is."
  :description ~(t "command.about.about") :title ~(t "command.about") :category ~(t "category.help") :icon "info" []
  (set "state.aboutOpen" true))
