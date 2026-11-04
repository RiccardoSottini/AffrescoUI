;;; computed.lisp --- what Quaderno works out from its state: the file shown, the tree, the pickers, find

;;; Computed values

(defcomputed file "The file shown, or nil." (if (>= state.active 0) (get state.files state.active) nil))

(defcomputed path "The shown file's path, for the title bar."
  (if computed.file (+ (if computed.file.folder (str computed.file.folder "  /  ") "") computed.file.name) ""))

(defcomputed tree "The project as the explorer's tree: its folders, then the files at its root."
  [{:id "d:root"
      :label state.project
      :children (concat [{:id "d:src"
                          :label "src"
                          :children (map (fn [it] {:id (str "f" it) :label (get-in state.files [it :name])})
                                         (filter (fn [it] (= (get-in state.files [it :folder]) "src"))
                                                 (range (count state.files))))}
                         {:id "d:screens"
                          :label "screens"
                          :children (map (fn [it] {:id (str "f" it) :label (get-in state.files [it :name])})
                                         (filter (fn [it] (= (get-in state.files [it :folder]) "screens"))
                                                 (range (count state.files))))}]
                        (map (fn [it] {:id (str "f" it) :label (get-in state.files [it :name])})
                             (filter (fn [it] (= (get-in state.files [it :folder]) ""))
                                     (range (count state.files)))))}])

(defcomputed tabItems "The tabs: each open file's name, with a dot while it is unsaved."
  (map (fn [it] {:id (text it) :label (get-in state.files [it :name]) :marker (get-in state.files [it :dirty])})
         state.tabs))

(defcomputed errors "How many errors the problems have." (count (filter (fn [it] (= it.kind "error")) state.problems)))

(defcomputed warnings "How many warnings the problems have."
  (count (filter (fn [it] (= it.kind "warning")) state.problems)))

(defcomputed picks "The Quick Switcher's items, by what is typed: files, @symbols, :lines or #text."
  (map (fn [it]
           {:title (get-in state.files [it :name])
            :detail (str state.project
                         "/"
                         (if (get-in state.files [it :folder]) (str (get-in state.files [it :folder]) "/") "")
                         (get-in state.files [it :name]))
            :file it})
         (filter (fn [it] (not (get-in state.files [it :scratch]))) (range (count state.files)))))

(defcomputed gotoMark "Where the Quick Switcher's text names a symbol, a line or a search: @, : or #."
  (min (concat [(count state.quickPick.query)]
                 (filter (fn [it] (>= it 0)) (map (fn [it] (indexOf state.quickPick.query it)) ["@" ":" "#"])))))

(defcomputed gotoKind "Which of them it names." (slice state.quickPick.query computed.gotoMark (+ computed.gotoMark 1)))

(defcomputed gotoFile "The file the Quick Switcher's text names, or the file shown."
  (if (= computed.gotoMark 0)
        state.active
        (or-else (get-in (fuzzySort computed.picks (slice state.quickPick.query 0 computed.gotoMark) "title" 1)
                         [0 :item :file])
                 (- 1))))

(defcomputed gotoLines "That file's lines."
  (if (and (>= computed.gotoFile 0) (not= computed.gotoKind ""))
        (split (get-in state.files [computed.gotoFile :text]) "\n")
        []))

(defcomputed search "What the Quick Switcher searches for, in lower case."
  (lower-case (slice state.quickPick.query (+ computed.gotoMark 1))))

(defcomputed lines "The shown file's lines." (if computed.file (split computed.file.text "\n") []))

(defcomputed commentMark "How the shown file's language starts a comment."
  (if (and computed.file (or (= computed.file.language "CMake") (= computed.file.language "Markdown"))) "#" "//"))

(defcomputed symbols "The symbols of the file the Quick Switcher names."
  (map (fn [it] it.item)
         (fuzzySort (or-else (get state.symbolIndex computed.gotoFile) []) computed.search "title" 50)))

(defcomputed lineItems "Go to Line's item: the line typed, or how to type one."
  (if (< computed.gotoFile 0)
        []
        [(if (number computed.search)
             {:title (t "goto.line" {:n (clamp (number computed.search) 1 (count computed.gotoLines))})
              :detail (get-in state.files [computed.gotoFile :name])
              :file computed.gotoFile
              :line (clamp (number computed.search) 1 (count computed.gotoLines))}
             {:title (t "goto.typeLine" {:last (count computed.gotoLines)})
              :detail (get-in state.files [computed.gotoFile :name])
              :file computed.gotoFile})]))

(defcomputed found "The lines of the file that hold the text searched, at most 50."
  (if (empty? computed.search)
        []
        (slice (map (fn [it]
                      {:title (trim (get computed.gotoLines it))
                       :detail (t "goto.lineN" {:n (+ it 1)})
                       :file computed.gotoFile
                       :line (+ it 1)
                       :column (+ (indexOf (lower-case (get computed.gotoLines it)) computed.search) 1)
                       :length (count computed.search)})
                    (filter (fn [it] (contains (lower-case (get computed.gotoLines it)) computed.search))
                            (range (count computed.gotoLines))))
               0
               50)))

(defcomputed gotoItems "The Quick Switcher's items after its mark."
  (cond (= computed.gotoKind "@") computed.symbols
          (= computed.gotoKind ":") computed.lineItems
          :else computed.found))

(defcomputed projectSymbols "Every symbol of the project." (flat state.symbolIndex))

(defcomputed resultsFile "The Search Results buffer's index, or -1."
  (or-else (find (fn [it] (get-in state.files [it :scratch])) (range (count state.files))) (- 1)))

(defcomputed resultLines "The lines of the Search Results buffer that are matches."
  (if (< computed.resultsFile 0)
        []
        (map (fn [it] it)
             (filter (fn [it] (> it 0))
                     (map-indexed (fn [index it] (if (and (= (slice it 0 1) " ") (contains it ": ")) (+ index 1) 0))
                                  (split (get-in state.files [computed.resultsFile :text]) "\n"))))))

(defcomputed findHeight "How tall the find panel is: closed, find, replace, or in files."
  (if state.findBar.open
        (cond state.findBar.inFiles (if state.findBar.replacing 94 64) state.findBar.replacing 64 :else 34)
        0))

(defcomputed findLeft "Where the find panel's fields start, after their captions."
  (if (or state.findBar.inFiles state.findBar.replacing) 188 128))

(defcomputed replaceTop "Where the replace field is." (if state.findBar.inFiles 65 35))

(defcomputed palette "The command palette's items: every command."
  (map (fn [it] {:title (str it.category ": " it.title) :detail (or it.keyLabel "") :run it.name})
         (filter (fn [it] it.enabled) commands)))

(defcomputed findQuery "What the editor highlights: the find panel's text, or the search results' query."
  (cond (and state.findBar.open (not state.findBar.inFiles) (not (empty? state.findBar.text))) {:text state.findBar.text
           :regex state.findBar.regex
           :wholeWord state.findBar.wholeWord
           :caseSensitive state.findBar.caseSensitive
           :inSelection state.findBar.inSelection
           :replace state.findBar.replace
           :step state.findBar.step
           :request state.findBar.request}
          (and (>= state.active 0) (= state.active computed.resultsFile))
          {:text state.results.query :caseSensitive state.findBar.caseSensitive}
          :else nil))

(defcomputed findMatches "How many matches the editor found." (:matches (merge {:matches 0} state.caret)))

(defcomputed themes "The themes the picker lists."
  [{:title (t "ui.dark") :detail (t "ui.charcoalAndVioletInk") :theme "dark" :pick "dark"}
     {:title (t "ui.light") :detail (t "ui.paperAndVioletInk") :theme "light" :pick "light"}
     {:title (t "ui.system")
      :detail (t "theme.followsSystem" {:scheme window.colorScheme})
      :theme window.colorScheme
      :pick "system"}])

(defcomputed theme "The theme shown: the one the picker previews, the system's, or the one chosen."
  (cond state.themePick.open (:theme (or-else (get computed.themes (or-else state.themePick.index 0))
                                              computed.themes.0))
          (= state.theme "system") window.colorScheme
          :else state.theme))

(defcomputed syntaxes "The syntaxes the picker lists."
  [{:title "Plain Text" :language "" :grammar "plain.txt"}
     {:title "C++" :language "C++" :grammar "cpp"}
     {:title "CMake" :language "CMake" :grammar "CMakeLists.txt"}
     {:title "JavaScript" :language "JavaScript" :grammar "javascript"}
     {:title "JSON" :language "JSON" :grammar "json"}
     {:title "Markdown" :language "Markdown" :grammar "markdown"}
     {:title "Python" :language "Python" :grammar "python"}
     {:title "Shell" :language "Shell" :grammar "script.sh"}
     {:title "TypeScript" :language "TypeScript" :grammar "typescript"}])

(defcomputed settingsText "The settings as a JSON file, which Save applies."
  (str "{\n\t// Quaderno settings: save this file (Cmd+S) to apply them.\n\t// theme: \"dark\", \"light\" or \"system\"\n\t\"theme\": "
         (json state.theme)
         ",\n\t\"wordWrap\": "
         (json state.wrap)
         ",\n\t\"ruler\": "
         (json state.ruler)
         ",\n\t\"minimap\": "
         (json state.minimap)
         ",\n\t\"sideBar\": "
         (json state.sidebarOpen)
         "\n}\n"))

(defcomputed keysText "The key bindings as a JSON file."
  (str "// Quaderno key bindings (read only): the keys section of the screen\n[\n"
         (join ",\n"
               (map (fn [it] (str "\t{ \"keys\": " (json it.key) ", \"command\": " (json it.name) " }"))
                    (filter (fn [it] it.key) commands)))
         "\n]\n"))

(defcomputed findSummary "The find panel's status: how many matches."
  (cond (empty? state.findBar.text) ""
          (= computed.findMatches 0) (t "ui.noResults")
          :else (plural computed.findMatches "match" "matches")))
