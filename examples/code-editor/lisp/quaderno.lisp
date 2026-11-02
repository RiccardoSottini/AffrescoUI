;;; quaderno.lisp --- what Quaderno does: its files and tabs, the console, the search results, and its editing commands
;;;
;;; The screen (index.screen.lisp) says what is shown, and keeps the one-step actions and the text scans (the problems,
;;; the symbols, Find in Files), which the expressions run fastest. This file holds the rest. A function reads the
;;; running app through paths (`state.tabs`, `computed.resultsFile`, `event.value`) and changes it only through steps
;;; (`set`, `push`, `edit`...). A handler runs one by name, as it would an action: {:run open :with {:file 2}}.

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

(defn applySettings "Saves the Settings file and applies its values; a broken one stays unsaved, with an error in the console."
  [file]
  (let [values (parseJsonc (:text (file-at file)))]
    (if (or (nil? values) (not= (type values) "object"))
      (do (push "state.terminal" {:t "Settings.json: not valid JSON, nothing applied" :c "error"})
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

(defn openVirtual "Opens the file of that name outside the project, made with the text the first time." [name text readOnly]
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

(defn goToPlace "Goes to a row with a file (opened) and a line, and selects its column and length if it has them." [item]
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

(defn openResult "A line of the Search Results buffer: a match line ('   12: text') opens its file there, under the path line above it."
  [line]
  (let [lines (split (:text (file-at computed.resultsFile)) "\n")]
    (when (contains computed.resultLines line)
      (set "state.results.at" line)
      (let [match (get lines (dec line))
            heading (find (fn [l] (and (not= (slice l 0 1) " ") (= (slice l -1) ":"))) (reverse (take (dec line) lines)))
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

(defcommand build "Runs the build in the console." :title "Build" :category "Tools" :icon hammer []
  (typeCommand "build"))

(defcommand runApp "Runs the program in the console." :title "Run" :category "Tools" :icon play []
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

(defn prepare "Generates the large file once (5,000 lines), saves every file as it is, then finds the symbols and the problems."
  []
  (set (place 3 "text") (join "\n" (map sensor-line (range 5000))))
  (map-indexed (fn [f text] (set (place f "saved") text)) (map (fn [x] (:text x)) state.files))
  (set "state.symbolIndex" (map (fn [f] []) state.files))
  (indexAll))

;;; File commands

(def file-open "When a command needs a file open." "{{state.active >= 0}}")

(defcommand save "Saves this file." :title "Save" :category "File" :icon save :enablement ~file-open []
  (saveFile state.active)
  (run "findProblems"))

(defcommand saveAll "Saves every file." :title "Save All" :category "File" :icon save []
  (map saveFile (range (count state.files)))
  (run "findProblems"))

(defcommand closeTab "Closes this file." :title "Close File" :category "File" :icon x :enablement ~file-open []
  (close state.active))

(defcommand closeAll "Closes every saved file, then asks about the first unsaved one." :title "Close All Files"
  :category "File" :enablement "{{length(state.tabs) > 0}}"
  []
  (closeSaved state.tabs)
  (when (> (count state.tabs) 0) (close (first state.tabs))))

(defcommand closeOthers "Closes the other saved files." :title "Close Other Files" :category "File"
  :enablement "{{length(state.tabs) > 1}}"
  []
  (closeSaved (filter (fn [f] (not= f state.active)) state.tabs)))

(defcommand reopenClosed "Opens the file closed last." :title "Reopen Closed File" :category "File"
  :enablement "{{length(state.closed) > 0}}"
  []
  (open (last state.closed))
  (remove-state "state.closed" :at (dec (count state.closed))))

(defcommand refreshFolders "Finds the symbols and the problems of every file again." :title "Refresh Folders"
  :category "Project"
  []
  (indexAll))

(defcommand openSettings "Opens the settings, as a JSON file." :title "Settings" :category "Preferences" :icon settings []
  (openVirtual "Settings.json" computed.settingsText false))

(defcommand openKeyBindings "Opens the key bindings, as a JSON file to read." :title "Key Bindings" :category "Preferences"
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

(defcommand findNext "" :title "Find Next" :category "Find" :enablement ~find-open [] (findStep "next"))
(defcommand findPrevious "" :title "Find Previous" :category "Find" :enablement ~find-open [] (findStep "previous"))
(defcommand replaceOne "" :title "Replace Next" :category "Find" :enablement ~find-open [] (findStep "replace"))
(defcommand replaceAllInFile "" :title "Replace All" :category "Find" :enablement ~find-open [] (findStep "replaceAll"))
(defcommand toggleFindRegex "" :title "Toggle Regular Expression" :category "Find" [] (toggleFind "regex"))
(defcommand toggleFindCase "" :title "Toggle Case Sensitive" :category "Find" [] (toggleFind "caseSensitive"))
(defcommand toggleFindWord "" :title "Toggle Whole Word" :category "Find" [] (toggleFind "wholeWord"))
(defcommand toggleFindInSelection "" :title "Toggle In Selection" :category "Find" [] (toggleFind "inSelection"))

;;; The editor's own commands

(defmacro defedit "A command that is the editor's own edit of the same name, while a file is open." [name title category icon]
  `(defcommand ~name "" :title ~title :category ~category :icon ~icon :enablement "{{state.active >= 0}}" []
     (edit ~(str name) :in "#editor")))

(defedit undo "Undo" "Edit" undo-2)
(defedit redo "Redo" "Edit" redo-2)
(defedit cut "Cut" "Edit" scissors)
(defedit copy "Copy" "Edit" copy)
(defedit paste "Paste" "Edit" clipboard-paste)
(defedit selectAll "Select All" "Selection" text-select)
(defedit indentLines "Indent" "Edit" indent)
(defedit outdentLines "Outdent" "Edit" outdent)
(defedit unfoldAll "Unfold All" "View" unfold-vertical)
(defedit duplicateLines "Duplicate Line" "Edit" copy-plus)
(defedit deleteLines "Delete Line" "Edit" trash-2)
(defedit moveLinesUp "Move Line Up" "Edit" arrow-up)
(defedit moveLinesDown "Move Line Down" "Edit" arrow-down)
(defedit joinLines "Join Lines" "Edit" merge)
(defedit insertLineAfter "Insert Line After" "Edit" corner-down-left)
(defedit insertLineBefore "Insert Line Before" "Edit" corner-up-left)
(defedit transpose "Transpose" "Edit" repeat)
(defedit sortLines "Sort Lines" "Edit" arrow-down-a-z)
(defedit reverseLines "Reverse Lines" "Edit" arrow-up-down)
(defedit uniqueLines "Unique Lines" "Edit" list-filter)
(defedit upperCase "Upper Case" "Convert Case" case-upper)
(defedit lowerCase "Lower Case" "Convert Case" case-lower)
(defedit titleCase "Title Case" "Convert Case" case-sensitive)
(defedit swapCase "Swap Case" "Convert Case" case-sensitive)
(defedit expandToLine "Expand Selection to Line" "Selection" text-select)
(defedit expandToWord "Expand Selection to Word" "Selection" text-select)
(defedit splitIntoLines "Split into Lines" "Selection" text-cursor)
(defedit selectAllOccurrences "Select All Occurrences" "Selection" text-cursor)

(defcommand toggleComment "Comments the lines out, or back in, with the language's mark." :title "Toggle Comment"
  :category "Edit" :icon message-square :enablement ~file-open
  []
  (edit "toggleComment" :in "#editor" :prefix computed.commentMark))
