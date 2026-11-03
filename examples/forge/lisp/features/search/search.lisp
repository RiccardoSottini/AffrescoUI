;;; search.lisp --- Forge's search: Find in Files over the open folder, its results, and Replace All
;;;
;;; The search service searches the folder (search.find, cancellable, one event per file, gathered in
;;; state.search.result); the side bar shows the matches as a tree by file, and Open Results in Editor shows them as
;;; text in a tab. A match opens at its line. Replace All asks first, then replaces one file at a time
;;; (search.replace, then files.write): files with unsaved edits are skipped, and the open ones that changed are read
;;; again. Build output and dependencies (state.search.skip) are never searched.

;;; Searching

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

(defcommand findInFiles "Shows the Search view, its query field focused."
  :title ~(t "command.findInFiles") :category ~(t "category.search") :icon "search" []
  (set "state.sideView" "search")
  (set "state.layout.sidebar" true)
  (after 0 findInFilesLater))

(defn findInFilesLater "Focuses the query field once the view is shown." []
  (focus "#search-query"))

(defn runSearch "Searches again for the query, after cancelling the search running; nothing without a query or a folder."
  []
  (cancel "forge-search")
  (set "state.search.error" "")
  (set "state.search.collapsed" [])
  (cond
    (empty? (trim state.search.query)) (do (set "state.search.result" nil) (set "state.search.ran" nil))
    (empty? state.folder.root)
      (do (set "state.search.result" nil) (set "state.search.error" (t "search.noFolder")))
    :else (startSearch)))

(defn startSearch "Starts a search with the view's options; build output and dependencies are left out." []
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

(defn startSearchEvent "A file's matches, as they come." [event]
  (searchFound event))

(defn startSearchDone "The search is over: the files in order, and Output says how many matches." [result]
  (set "state.search.result"
       (merge state.search.result
              {:tree (sort-by (fn [it] it.id) state.search.result.tree)
               :skipped result.skipped
               :truncated result.truncated
               :done true}))
  (log (t "log.search" {:matches state.search.result.matches :files state.search.result.files})))

(defn startSearchFailed "Says why the search failed, unless it was cancelled." [error]
  (when (not= error.code "cancelled") (set "state.search.result" nil) (set "state.search.error" error.message)))

(defn searchFound "One file's matches (`found`: file, matches): a tree item with a child per match, each naming its place in hits."
  [found]
  (set "state.search.result"
       (merge state.search.result
              {:tree (concat state.search.result.tree
                             [{:id (str "file:" found.file)
                               :label (str (baseName found.file)
                                           (if (contains found.file "/")
                                             (str "  "
                                                  (slice found.file
                                                         0
                                                         (- (- (count found.file) (count (baseName found.file))) 1)))
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
                                    {:file found.file :label (baseName found.file) :line it.line :column it.column})
                                  found.matches))
               :files (+ state.search.result.files 1)
               :matches (+ state.search.result.matches (count found.matches))})))

(defcommand cancelSearch "Cancel Search"
  :title ~(t "command.cancelSearch") :category ~(t "category.search") :icon "close" :enablement ~state.search.busy []
  (cancel "forge-search"))

(defkey "mod+shift+f" findInFiles)

;;; The results

(defcomputed searchSummary
  (cond (not (empty? state.search.error)) state.search.error
          state.search.result (str (if (= state.search.result.matches 0)
                                     (t "search.none")
                                     (t "search.found" {:matches state.search.result.matches
                                                        :files state.search.result.files}))
                                   (if state.search.result.truncated (t "search.capped") "")
             (if state.search.result.done "" "..."))
          :else ""))

(defcomputed searchExpanded
  (if state.search.result
        (filter (fn [it] (not (contains state.search.collapsed it))) (map (fn [it] it.id) state.search.result.tree))
        []))

(defn openSearchHit "A row of the results tree, opened: a match at its line, or a file." []
  (cond
    (= (slice (or-else event.id "") 0 4) "hit:")
      (openSearchMatch (get state.search.result.hits (number (slice event.id 4))))
    (= (slice (or-else event.id "") 0 5) "file:") (openPath (slice event.id 5) (baseName event.id))))

(defn openSearchMatch "Opens a match's file at its line." [hit]
  (when (some? hit) (openPath hit.file hit.label) (goToLine hit.line)))

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

(defn searchDismissFile "Takes a file and its matches out of the results." [file]
  (when (some? file)
    (set "state.search.result"
         (merge state.search.result
                {:tree (filter (fn [it] (not= it.id file.id)) state.search.result.tree)
                 :files (- state.search.result.files 1)
                 :matches (- state.search.result.matches (count file.children))}))))

(defn searchDismissHit "Takes a match out of the results, and its file when it was the last." [hit file]
  (cond
    (and (some? file) (<= (count file.children) 1)) (searchDismissFile file)
    (some? file)
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
                   :matches (- state.search.result.matches 1)}))))

(defn searchExpand "A file of the results opened or closed (`event.expanded`): it is remembered." []
  (set "state.search.collapsed"
       (if event.expanded
         (filter (fn [it] (not= it event.id)) state.search.collapsed)
         (concat state.search.collapsed [event.id]))))

(defn searchMenu "The results' menu, opened where the right-click was." []
  (set "state.search.x" event.x)
  (set "state.search.y" event.y)
  (set "state.search.menu" true))

(defn toggleSearchOption "Turns an option of the query on or off: caseSensitive, wholeWord or regex." [option]
  (toggle (str "state.search." option)))

;;; The results in an editor

(defstate searchEditor {:caret {:line 1 :column 1 :selected 0}})

(defcomputed searchEditorRows
  (concat [{:text (str (if (empty? computed.searchSummary) (t "search.notYet") computed.searchSummary)
                         (if (empty? state.search.query) "" (t "search.for" {:query state.search.query})))
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

(defcommand openSearchEditor "Show the search's results as text in an editor tab; Enter or a double click on a line opens it."
  :title ~(t "command.openSearchEditor") :category ~(t "category.search") :icon "file-text"
  :description ~(t "command.openSearchEditor.about") :enablement ~(some? state.search.result) []
  (openPage "forge:search-results" (t "page.searchResults") "searchResults"))

(defn openSearchEditorLine "The results editor's line at the caret, opened." []
  (openSearchRow (or-else (get computed.searchEditorRows (- (or-else state.searchEditor.caret.line 1) 1)) {:id ""})))

(defn openSearchRow "A row of the results, opened: a match at its line, or a file." [row]
  (cond
    (= (slice row.id 0 4) "hit:") (openSearchMatch (get state.search.result.hits (number (slice row.id 4))))
    (= (slice row.id 0 5) "file:") (openPath (slice row.id 5) (baseName row.id))))

;;; Replace All

(defn askReplace "Asks before replacing, when the search found something." []
  (when (and state.search.result state.search.result.done (> state.search.result.files 0))
    (set "state.search.confirm" true)))

(defn replaceAll "Replaces every match, one file at a time; open files with unsaved edits are skipped."
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

(defn replaceNext "Replaces in the next file, or ends." []
  (if (empty? state.search.replacing.queue)
    (replaceFinished state.search.replacing)
    (replaceFile state.search.replacing.queue.0)))

(defn replaceFile "Replaces a file's matches, then writes it." [file]
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

(defn replaceFileDone "A file's text with its matches replaced: written, unless none was." [file result]
  (if (= result.replacements 0) (replaceNext) (replaceWrite file result.text result.replacements)))

(defn replaceFileFailed "Notes a file whose matches couldn't be replaced." [file error]
  (replaceFailed file error.message))

(defn replaceWrite "Writes a file whose matches were replaced." [file text replacements]
  (invoke "files.write"
          :with {:path file :text text}
          :onError [replaceWriteFailed file]
          :onDone [replaceWriteDone replacements file]))

(defn replaceWriteDone "Counts a file written, then goes on." [replacements file]
  (set "state.search.replacing.replaced" (+ state.search.replacing.replaced replacements))
  (set "state.search.replacing.changed" (concat state.search.replacing.changed [file]))
  (replaceNext))

(defn replaceWriteFailed "Notes a file that couldn't be written." [file error]
  (replaceFailed file error.message))

(defn replaceFailed "Notes a file that failed, then goes on." [file message]
  (set "state.search.replacing.failed" (concat state.search.replacing.failed [{:file file :error message}]))
  (replaceNext))

(defn replaceFinished "Replacing is over: the open files that changed read again, a notice of what was done, the search run again."
  [done]
  (set "state.search.replacing" nil)
  (set "state.search.reload" (filter (fn [it] (contains computed.docIds it)) done.changed))
  (doseq [file state.search.reload] (invoke "files.read" :with {:path file} :onDone [replaceFinishedDone file]))
  (notify (if (or (> (count done.failed) 0) (> (count done.skipped) 0)) "warning" "info")
          (str (t "notify.replaced" {:matches done.replaced :files (count done.changed)})
               (if (> (count done.skipped) 0) (t "notify.replacedSkipped" {:n (count done.skipped)}) "")
               (if (> (count done.failed) 0) (t "notify.replacedFailed" {:n (count done.failed)}) "")))
  (runSearch))

(defn replaceFinishedDone "An open file read again after a replace." [file result]
  (reloaded file result.text))
