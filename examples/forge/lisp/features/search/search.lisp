;;; search.lisp --- Forge's Search view: an editor page (Find in Files, mod+shift+f) that searches the open folder with the search service (search.find, cancellable, one event per file, built into state.search.result), shows the matches as a tree by file, opens a match at its line, and replaces every match after a confirm, one file at a time (search.replace, then files.write)
;;;
;;; Build output and dependencies (state.search.skip) are never searched; files with unsaved edits are skipped, and
;;; the open ones that changed are read again.

;;; State and definitions

(defstate search
  {:query ""
   :replace ""
   :regex false
   :wholeWord false
   :caseSensitive false
   :include ""
   :exclude ""
   :result nil
   :ran nil
   :collapsed []
   :selected nil
   :menu false
   :x 0
   :y 0
   :reload []
   :replacing nil
   :skip "build, node_modules, out, target, dist, __pycache__"
   :busy false
   :confirm false
   :error ""})

(defstate searchEditor {:caret {:line 1 :column 1 :selected 0}})

(defcomputed searchEditorRows
  (concat [{:text (+ (if (empty? computed.searchSummary) "No search yet" computed.searchSummary)
                       (if (empty? state.search.query) "" (str " for " state.search.query)))
              :id ""}
             {:text "" :id ""}]
            (reduce (fn [acc it]
                      (concat acc
                              [{:text (slice it.id 5) :id it.id}]
                              (map (fn [it] {:text (str "  " it.label) :id it.id}) (or-else it.children []))
                              [{:text "" :id ""}]))
                    []
                    (if state.search.result state.search.result.tree []))))

(defcomputed searchEditorText (join "\n" (map (fn [it] it.text) computed.searchEditorRows)))

(defcomputed searchExpanded
  (if state.search.result
        (filter (fn [it] (not (contains state.search.collapsed it))) (map (fn [it] it.id) state.search.result.tree))
        []))

(defcomputed searchSummary
  (cond (not (empty? state.search.error)) state.search.error
          state.search.result (+ (if (= state.search.result.matches 0)
                 "No results"
                 (str (plural state.search.result.matches "result") " in " (plural state.search.result.files "file")))
             (if state.search.result.truncated ", capped" "")
             (if state.search.result.done "" "..."))
          :else ""))

(defkey "mod+shift+f" findInFiles)

;;; Functions and commands

(defcommand findInFiles "Find in Files"
  :title ~(t "command.findInFiles") :category ~(t "category.search") :icon "search" []
  (set "state.sideView" "search")
  (set "state.layout.sidebar" true)
  (after 0 findInFilesLater))

(defn runSearch "TODO" []
  (cancel "forge-search")
  (set "state.search.error" "")
  (set "state.search.collapsed" [])
  (cond
    (empty? (trim state.search.query)) (do (set "state.search.result" nil) (set "state.search.ran" nil))
    (empty? state.folder.root)
      (do (set "state.search.result" nil) (set "state.search.error" "Open a folder to search it."))
    :else (startSearch)))

(defn startSearch "TODO" []
  (set "state.search.ran"
       {:pattern state.search.query
        :regex state.search.regex
        :wholeWord state.search.wholeWord
        :caseSensitive state.search.caseSensitive
        :include state.search.include
        :exclude (str state.search.skip (if (empty? (trim state.search.exclude)) "" (str ", " state.search.exclude)))})
  (set "state.search.result" {:tree [] :hits [] :files 0 :matches 0 :skipped 0 :truncated false :done false})
  (invoke "search.find"
          :with state.search.ran
          :id "forge-search"
          :timeout 600000
          :loading "state.search.busy"
          :onEvent startSearchEvent
          :onError startSearchFailed
          :onDone startSearchDone))

(defn searchFound "found: { file, matches: [{ line (from 1), column, length, text, at }] }, one file's matches: a tree item with a child per match, each naming its place in hits"
  [found]
  (set "state.search.result"
       (merge state.search.result
              {:tree (concat state.search.result.tree
                             [{:id (str "file:" found.file)
                               :label (str (get (reverse (split found.file "/")) 0)
                                           (if (contains found.file "/")
                                             (str "  "
                                                  (slice found.file
                                                         0
                                                         (- (- (count found.file)
                                                               (count (get (reverse (split found.file "/")) 0)))
                                                            1)))
                                             "")
                                           "  ("
                                           (count found.matches)
                                           ")")
                               :icon "file"
                               :children (map-indexed (fn [index it]
                                                        {:id (str "hit:" (+ (count state.search.result.hits) index))
                                                         :label (str it.line ": " (trim it.text))})
                                                      found.matches)}])
               :hits (concat state.search.result.hits
                             (map (fn [it]
                                    {:file found.file
                                     :label (get (reverse (split found.file "/")) 0)
                                     :line it.line
                                     :column it.column})
                                  found.matches))
               :files (+ state.search.result.files 1)
               :matches (+ state.search.result.matches (count found.matches))})))

(defcommand cancelSearch "Cancel Search"
  :title ~(t "command.cancelSearch") :category ~(t "category.search") :icon "close" :enablement ~state.search.busy []
  (cancel "forge-search"))

(defn openSearchHit "TODO" []
  (if (= (slice (or-else event.id "") 0 4) "hit:")
    (openSearchMatch (get state.search.result.hits (number (slice event.id 4))))
    (do
      (when (= (slice (or-else event.id "") 0 5) "file:")
        (openPath (slice event.id 5) (get (reverse (split event.id "/")) 0))))))

(defcommand openSearchEditor "Show the search's results as text in an editor tab; Enter or a double click on a line opens it."
  :title ~(t "command.openSearchEditor") :category ~(t "category.search") :icon "file-text"
  :description ~(t "command.openSearchEditor.about") :enablement ~(some? state.search.result) []
  (openPage "forge:search-results" "Search Results" "searchResults"))

(defn openSearchEditorLine "TODO" []
  (openSearchRow (or-else (get computed.searchEditorRows (- (or-else state.searchEditor.caret.line 1) 1)) {:id ""})))

(defn openSearchRow "TODO" [row]
  (if (= (slice row.id 0 4) "hit:")
    (openSearchMatch (get state.search.result.hits (number (slice row.id 4))))
    (do (when (= (slice row.id 0 5) "file:") (openPath (slice row.id 5) (get (reverse (split row.id "/")) 0))))))

(defcommand searchDismiss "Take the selected file or match out of the results; the files stay as they are."
  :title ~(t "command.searchDismiss") :category ~(t "category.search") :icon "close"
  :description ~(t "command.searchDismiss.about")
  :enablement ~(and (some? state.search.result) state.search.result.done (not (empty? (or-else state.search.selected "")))) []
  (if (= (slice state.search.selected 0 5) "file:")
    (searchDismissFile (find (fn [it] (= it.id state.search.selected)) state.search.result.tree))
    (searchDismissHit
      state.search.selected
      (find (fn [it] (some (fn [it] (= it.id state.search.selected)) it.children)) state.search.result.tree)))
  (set "state.search.selected" nil))

(defn searchDismissFile "TODO" [file]
  (when (some? file)
    (set "state.search.result"
         (merge state.search.result
                {:tree (filter (fn [it] (not= it.id file.id)) state.search.result.tree)
                 :files (- state.search.result.files 1)
                 :matches (- state.search.result.matches (count file.children))}))))

(defn searchDismissHit "TODO" [hit file]
  (if (and (some? file) (<= (count file.children) 1))
    (searchDismissFile file)
    (do
      (when (some? file)
        (set "state.search.result"
             (merge state.search.result
                    {:tree (map (fn [it]
                                  (if (not= it.id file.id)
                                    it
                                    (merge it
                                           {:children (filter (fn [it] (not= it.id hit)) it.children)
                                            :label (str (slice it.label
                                                               0
                                                               (- (count it.label)
                                                                  (count (str "(" (count it.children) ")"))))
                                                        "("
                                                        (- (count it.children) 1)
                                                        ")")})))
                                state.search.result.tree)
                     :matches (- state.search.result.matches 1)}))))))

(defn openSearchMatch "TODO" [hit]
  (when (some? hit) (openPath hit.file hit.label) (goToLine hit.line)))

(defn askReplace "TODO" []
  (when (and state.search.result state.search.result.done (> state.search.result.files 0))
    (set "state.search.confirm" true)))

(defn replaceAll "One file at a time (the search service runs 4 calls at once): its text replaced by search.replace, written by files.write; open files with unsaved edits are skipped"
  []
  (set "state.search.replacing"
       {:queue (filter (fn [it] (not (contains computed.dirtyIds it)))
                       (map (fn [it] (slice it.id 5)) state.search.result.tree))
        :skipped (filter (fn [it] (contains computed.dirtyIds it))
                         (map (fn [it] (slice it.id 5)) state.search.result.tree))
        :replaced 0
        :changed []
        :failed []})
  (replaceNext))

(defn replaceNext "TODO" []
  (if (empty? state.search.replacing.queue)
    (replaceFinished state.search.replacing)
    (replaceFile state.search.replacing.queue.0)))

(defn replaceFile "TODO" [file]
  (set "state.search.replacing.queue" (slice state.search.replacing.queue 1))
  (invoke "search.replace"
          :with {:path file
                 :pattern state.search.ran.pattern
                 :regex state.search.ran.regex
                 :wholeWord state.search.ran.wholeWord
                 :caseSensitive state.search.ran.caseSensitive
                 :replacement state.search.replace}
          :onError [replaceFileFailed file]
          :onDone [replaceFileDone file]))

(defn replaceWrite "TODO" [file text count]
  (invoke "files.write"
          :with {:path file :text text}
          :onError [replaceWriteFailed file]
          :onDone [replaceWriteDone count file]))

(defn replaceFailed "TODO" [file message]
  (set "state.search.replacing.failed" (concat state.search.replacing.failed [{:file file :error message}]))
  (replaceNext))

(defn replaceFinished "TODO" [done]
  (set "state.search.replacing" nil)
  (set "state.search.reload" (filter (fn [it] (contains computed.docIds it)) done.changed))
  (doseq [file state.search.reload] (invoke "files.read" :with {:path file} :onDone [replaceFinishedDone file]))
  (notify (if (or (> (count done.failed) 0) (> (count done.skipped) 0)) "warning" "info")
          (str "Replaced "
               (plural done.replaced "match" "matches")
               " in "
               (plural (count done.changed) "file")
               (if (> (count done.skipped) 0) (str ", skipped " (plural (count done.skipped) "unsaved file")) "")
               (if (> (count done.failed) 0) (str ", " (plural (count done.failed) "file") " failed") "")))
  (runSearch))

(defn findInFilesLater "What findInFiles does when its time comes." []
  (focus "#search-query"))

(defn startSearchEvent "What startSearch does when it gets an event." [event]
  (searchFound event))

(defn startSearchFailed "What startSearch does when it fails." [error]
  (when (not= error.code "cancelled") (set "state.search.result" nil) (set "state.search.error" error.message)))

(defn startSearchDone "What startSearch does when it is done." [result]
  (set "state.search.result"
       (merge state.search.result
              {:tree (sort-by (fn [it] it.id) state.search.result.tree)
               :skipped result.skipped
               :truncated result.truncated
               :done true}))
  (log (str "Search: " (plural state.search.result.matches "result") " in " (plural state.search.result.files "file"))))

(defn replaceFileFailed "What replaceFile does when it fails." [file error]
  (replaceFailed file error.message))

(defn replaceFileDone "What replaceFile does when it is done." [file result]
  (if (= result.replacements 0) (replaceNext) (replaceWrite file result.text result.replacements)))

(defn replaceWriteFailed "What replaceWrite does when it fails." [file error]
  (replaceFailed file error.message))

(defn replaceWriteDone "What replaceWrite does when it is done." [count file]
  (set "state.search.replacing.replaced" (+ state.search.replacing.replaced count))
  (set "state.search.replacing.changed" (concat state.search.replacing.changed [file]))
  (replaceNext))

(defn replaceFinishedDone "What replaceFinished does when it is done." [file result]
  (reloaded file result.text))
