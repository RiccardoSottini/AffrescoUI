;;; search.lisp --- Forge's search: Find in Files over the open folder, its results, and Replace All
;;;
;;; The search service searches the folder (search.find, cancellable, one event per file, gathered in
;;; state.search.result); the side bar shows the matches as a tree by file, and Open Results in Editor shows them as
;;; text in a tab. A match opens at its line. Replace All asks first, then replaces one file at a time
;;; (search.replace, then files.write): files with unsaved edits are skipped, and the open ones that changed are read
;;; again. Build output and dependencies (state.search.skip) are never searched.

(ns forge.features.search.search)

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
  :title ~(t "command.findInFiles")
  :description ~(t "command.findInFiles") :category ~(t "category.search") :icon "search" []
  (set "state.sideView" :search)
  (set "state.layout.sidebar" true)
  (after 0 findInFilesLater))

(defn findInFilesLater "Focuses the query field once the view is shown." []
  (focus "#search-query"))

(defn runSearch
  "Searches again for the query, after cancelling the search running; nothing without a query or a folder."
  []
  (cancel "forge-search")
  (set "state.search.error" "")
  (set "state.search.collapsed" [])
  (cond
    (str/blank? state.search.query) (do (set "state.search.result" nil) (set "state.search.ran" nil))
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
        :exclude (str state.search.skip (if (str/blank? state.search.exclude) "" (str ", " state.search.exclude)))})
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
       (assoc state.search.result
              :tree (sort-by :id state.search.result.tree)
              :skipped result.skipped
              :truncated result.truncated
              :done true))
  (log-line (t "log.search" {:matches state.search.result.matches :files state.search.result.files})))

(defn startSearchFailed "Says why the search failed, unless it was cancelled." [error]
  (when (not= error.code "cancelled") (set "state.search.result" nil) (set "state.search.error" error.message)))

(defn searchFound
  "One file's matches (`found`: file, matches): a tree item with a child per match, each naming its place in hits."
  [found]
  (let [{:keys [file matches]} found
        {:keys [tree hits]} state.search.result
        folder (str/join "/" (butlast (str/split file "/")))]
    (set "state.search.result"
         (-> state.search.result
             (assoc :tree (concat tree
                                  [{:id (str "file:" file)
                                    :label (str (baseName file)
                                                (if (str/includes? file "/") (str "  " folder) "")
                                                "  ("
                                                (count matches)
                                                ")")
                                    :icon "file"
                                    :children (map-indexed (fn [index match]
                                                             {:id (str "hit:" (+ (count hits) index))
                                                              :label (str match.line ": " (str/trim match.text))})
                                                           matches)}])
                    :hits (concat hits
                                  (map (fn [match] {:file file :label (baseName file) :line match.line :column match.column})
                                       matches)))
             (update :files inc)
             (update :matches + (count matches))))))

(defcommand cancelSearch "Cancel Search"
  :title ~(t "command.cancelSearch") :description ~(t "command.cancelSearch") :category ~(t "category.search")
  :icon "close" :enablement ~state.search.busy []
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
        (filter #(not (contains state.search.collapsed %)) (map :id state.search.result.tree))
        []))

(defn openSearchHit "A row of the results tree, opened: a match at its line, or a file." []
  (cond
    (str/starts-with? (or-else event.id "") "hit:") (openSearchMatch (get state.search.result.hits (number (subs event.id 4))))
    (str/starts-with? (or-else event.id "") "file:") (openPath (subs event.id 5) (baseName event.id))))

(defn openSearchMatch "Opens a match's file at its line." [hit]
  (when-some [{:keys [file label line]} hit] (openPath file label) (goToLine line)))

(defcommand searchDismiss "Take the selected file or match out of the results; the files stay as they are."
  :title ~(t "command.searchDismiss") :category ~(t "category.search") :icon "close"
  :description ~(t "command.searchDismiss.about")
  :enablement ~(and (some? state.search.result) state.search.result.done (not (empty? (or-else state.search.selected "")))) []
  (if (str/starts-with? state.search.selected "file:")
    (searchDismissFile (find #(= (:id %) state.search.selected) state.search.result.tree))
    (searchDismissHit
      state.search.selected
      (find (fn [file] (some #(= (:id %) state.search.selected) (:children file))) state.search.result.tree)))
  (set "state.search.selected" nil))

(defn searchDismissFile "Takes a file and its matches out of the results." [file]
  (when-some [{:keys [id children]} file]
    (set "state.search.result"
         (-> state.search.result
             (update :tree (fn [tree] (remove #(= (:id %) id) tree)))
             (update :files dec)
             (update :matches - (count children))))))

(defn searchDismissHit "Takes a match out of the results, and its file when it was the last." [hit file]
  (when-some [{:keys [id children label]} file]
    (if (<= (count children) 1)
      (searchDismissFile file)
      (let [counted (str "(" (count children) ")")
            shown (assoc file
                         :children (remove #(= (:id %) hit) children)
                         :label (str (subs label 0 (- (count label) (count counted))) "(" (dec (count children)) ")"))]
        (set "state.search.result"
             (-> state.search.result
                 (update :tree (fn [tree] (map #(if (= (:id %) id) shown %) tree)))
                 (update :matches dec)))))))

(defn searchExpand "A file of the results opened or closed (`event.expanded`): it is remembered." []
  (set "state.search.collapsed"
       (if (true? event.expanded)
         (remove #(= % event.id) state.search.collapsed)
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
                              ; style: keep, #(...) can't give a map
                              (map (fn [it] {:text (str "  " it.label) :id it.id}) (or-else it.children []))
                              [{:text "" :id ""}]))
                    []
                    (if state.search.result state.search.result.tree []))))

(defcomputed searchEditorText (join "\n" (map :text computed.searchEditorRows)))

(defcommand openSearchEditor
  "Show the search's results as text in an editor tab; Enter or a double click on a line opens it."
  :title ~(t "command.openSearchEditor") :category ~(t "category.search") :icon "file-text"
  :description ~(t "command.openSearchEditor.about") :enablement ~(some? state.search.result) []
  (openPage "forge:search-results" (t "page.searchResults") "searchResults"))

(defn openSearchEditorLine "The results editor's line at the caret, opened." []
  (openSearchRow (or-else (get computed.searchEditorRows (dec (or-else state.searchEditor.caret.line 1))) {:id ""})))

(defn openSearchRow "A row of the results, opened: a match at its line, or a file." [row]
  (cond
    (str/starts-with? row.id "hit:") (openSearchMatch (get state.search.result.hits (number (subs row.id 4))))
    (str/starts-with? row.id "file:") (openPath (subs row.id 5) (baseName row.id))))

;;; Replace All

(defn askReplace "Asks before replacing, when the search found something." []
  (when (and (some? state.search.result) (true? state.search.result.done) (pos? state.search.result.files))
    (set "state.search.confirm" true)))

(defn replaceAll "Replaces every match, one file at a time; open files with unsaved edits are skipped."
  []
  (let [files (map #(subs (:id %) 5) state.search.result.tree)
        dirty? #(contains computed.dirtyIds %)]
    (set "state.search.replacing" {:queue (remove dirty? files) :skipped (filter dirty? files) :replaced 0 :changed [] :failed []}))
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

(defn replaceFinished
  "Replacing is over: the open files that changed read again, a notice of what was done, the search run again."
  [done]
  (set "state.search.replacing" nil)
  (set "state.search.reload" (filter #(contains computed.docIds %) done.changed))
  (doseq [file state.search.reload] (invoke "files.read" :with {:path file} :onDone [replaceFinishedDone file]))
  (notify (if (or (seq done.failed) (seq done.skipped)) "warning" "info")
          (str (t "notify.replaced" {:matches done.replaced :files (count done.changed)})
               (if (seq done.skipped) (t "notify.replacedSkipped" {:n (count done.skipped)}) "")
               (if (seq done.failed) (t "notify.replacedFailed" {:n (count done.failed)}) "")))
  (runSearch))

(defn replaceFinishedDone "An open file read again after a replace." [file result]
  (reloaded file result.text))
