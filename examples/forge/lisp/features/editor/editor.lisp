;;; editor.lisp --- Forge's editor: each open file's text is in state.docs (text, and saved: what is on disk), read and written through the files service and shown by the editors of every group with "bind"
;;;
;;; A click in the explorer opens a preview tab, which the next preview replaces; a double click, an edit or Keep
;;; Editor pins it. A dirty file's tab has a marker; Save, Save All and Revert File write or read it again. It owns
;;; openPath, closeTab, scanProblems and forgeEditors (the shell and the panels use them).

;;; State and definitions

(defkey "mod+s" save)

(defkey "mod+alt+s" saveAll)

(defpart editor "What editor adds." :watch {:state.explorer.selected previewSelectedFile})

;;; Functions and commands

(defn openPath "TODO" [path label]
  (openBuffer path label false))

(defn openPreview "TODO" [path label]
  (openBuffer path label true))

(defn openBuffer "TODO" [path label preview]
  (if (contains computed.docIds path)
    (showFileTab path label preview)
    (invoke "files.read"
            :with {:path path}
            :onError [openBufferFailed label]
            :onDone [openBufferDone path label preview])))

(defn showFileTab "TODO" [id label preview]
  (forgeRecordNav id)
  (cond
    (some (fn [it] (= it.id id)) (get-in state.groups [state.activeGroup :tabs])) (when (not preview) (pinTab id))
    (and preview (some (fn [it] it.preview) (get-in state.groups [state.activeGroup :tabs])))
      (replacePreview id label (:id (find (fn [it] it.preview) (get-in state.groups [state.activeGroup :tabs]))))
    :else (push (str "state.groups[" state.activeGroup "].tabs") {:id id :label label :preview preview}))
  (set (str "state.groups[" state.activeGroup "].active") id)
  (rememberRecent id label))

(defn replacePreview "TODO" [id label old]
  (set (str "state.groups[" state.activeGroup "].tabs")
       (map (fn [it] (if it.preview {:id id :label label :preview true} it))
            (get-in state.groups [state.activeGroup :tabs])))
  (releaseBuffer old))

(defn pinTab "TODO" [id]
  (set "state.groups"
       (map (fn [it] (merge it {:tabs (map (fn [it] (if (= it.id id) (merge it {:preview false}) it)) it.tabs)}))
            state.groups)))

(defn releaseBuffer "TODO" [id]
  (when (and (not (contains (map (fn [it] (contains (map (fn [it] it.id) it.tabs) id)) state.groups) true))
             (some (fn [it] (and (= it.id id) (= it.kind "file") (not it.dirty))) state.docs))
    (set "state.docs" (filter (fn [it] (not= it.id id)) state.docs))
    (lspClosed id)))

(defn closeTab "TODO" [gi id]
  (set (str "state.groups[" gi "].tabs") (filter (fn [it] (not= it.id id)) (get-in state.groups [gi :tabs])))
  (when (= (get-in state.groups [gi :active]) id)
    (set (str "state.groups[" gi "].active")
         (if (> (count (get-in state.groups [gi :tabs])) 0) (get-in state.groups [gi :tabs 0 :id]) nil)))
  (when (and (= (count (get-in state.groups [gi :tabs])) 0) (> (count state.groups) 1))
    (remove-state "state.groups" :at gi)
    (set "state.activeGroup" 0)
    (set "state.layout.groupSizes" []))
  (releaseBuffer id))

(defn markDirty "TODO" [id dirty]
  (set "state.docs" (map (fn [it] (if (= it.id id) (merge it {:dirty dirty}) it)) state.docs))
  (when dirty (pinTab id)))

(defcommand save "Save"
  :title ~(t "command.save") :category ~(t "category.file") :icon "save"
  :enablement ~(= computed.activeDoc.kind "file") []
  (saveFile computed.activeDoc))

(defcommand saveAll "Save All"
  :title ~(t "command.saveAll") :category ~(t "category.file") :icon "save-all"
  :enablement ~(> (count computed.dirtyIds) 0) []
  (doseq [doc (filter (fn [it] (and (= it.kind "file") it.dirty)) state.docs)] (saveFile doc)))

(defcommand revertFile "Read the file again, dropping the edits."
  :title ~(t "command.revertFile") :category ~(t "category.file") :icon "undo"
  :description ~(t "command.revertFile.about")
  :enablement ~(and (= computed.activeDoc.kind "file") computed.activeDoc.dirty) []
  (invoke "files.read" :with {:path computed.activeDoc.id} :onError revertFileFailed :onDone revertFileDone))

(defcommand keepEditor "Pin the preview tab, so the next preview opens beside it."
  :title ~(t "command.keepEditor") :category ~(t "category.view") :icon "pin"
  :description ~(t "command.keepEditor.about")
  :enablement ~(some (fn [it] it.preview) (get-in state.groups [state.activeGroup :tabs])) []
  (pinTab (get-in state.groups [state.activeGroup :active])))

(defcommand scanProblems "List the TODO and FIXME lines of the open files in Problems."
  :title ~(t "command.scanProblems") :category ~(t "category.problems") :icon "search"
  :description ~(t "command.scanProblems.about") []
  (set "state.problems" (filter (fn [it] (or-else it.server false)) state.problems))
  (doseq [doc (filter (fn [it] (and (= it.kind "file") (<= (count it.text) 1000000))) state.docs)]
    (set "state.problems"
         (slice (concat state.problems
                        (map (fn [it]
                               {:severity (if (contains it.row "FIXME") "warning" "info")
                                :message (slice it.row 0 120)
                                :file doc.id
                                :label doc.label
                                :line it.line})
                             (filter (fn [it] (or (contains it.row "TODO") (contains it.row "FIXME")))
                                     (map-indexed (fn [index it] {:line (+ index 1) :row (trim it)})
                                                  (split doc.text "\n")))))
                0
                1000)))
  (log (str "Problems: " (plural (count state.problems) "item"))))

(defn textChanged "id: a file's; event.value: its text. The file is dirty while its text isn't the one saved" [id]
  (doseq [doc (filter (fn [it] (= it.id id)) state.docs)]
    (when (not= (or (not= event.value doc.saved) (not= doc.eol doc.savedEol)) doc.dirty)
      (markDirty id (or (not= event.value doc.saved) (not= doc.eol doc.savedEol)))))
  (lspChanged id))

(defn saveFile "doc: an open file. Writes its text, with its line endings, and marks it saved" [doc]
  (invoke "files.write"
          :with {:path doc.id :text (if (= doc.eol "CRLF") (join "\r\n" (split doc.text "\n")) doc.text)}
          :onError [saveFileFailed doc]
          :onDone [saveFileDone doc]))

(defn reloaded "id: an open file; text: what it now holds on disk. Kept as the editor shows it (tabs as the tab size's spaces, no carriage returns), its line endings noted, so undoing back to it is clean"
  [id text]
  (set "state.docs"
       (map (fn [it]
              (if (= it.id id)
                (merge it
                       {:text (join (pad "" (or-else settings.editor.tabSize 4) " ")
                                    (split (join "" (split text "\r")) "\t"))
                        :saved (join (pad "" (or-else settings.editor.tabSize 4) " ")
                                     (split (join "" (split text "\r")) "\t"))
                        :eol (if (contains text "\r\n") "CRLF" "LF")
                        :savedEol (if (contains text "\r\n") "CRLF" "LF")
                        :dirty false})
                it))
            state.docs)))

(defn previewSelectedFile "TODO" []
  (when (= (:kind (or-else (find (fn [it] (= it.path event.value)) state.explorerTree.entries) {})) "file")
    (openPreview event.value (get (reverse (split event.value "/")) 0))))

(defn openBufferFailed "What openBuffer does when it fails." [label error]
  (notify "error" (str "Cannot open " label ": " error.message)))

(defn openBufferDone "What openBuffer does when it is done." [path label preview result]
  (push "state.docs"
        {:id path
         :label label
         :kind "file"
         :text (join (pad "" (or-else settings.editor.tabSize 4) " ") (split (join "" (split result.text "\r")) "\t"))
         :saved (join (pad "" (or-else settings.editor.tabSize 4) " ")
                      (split (join "" (split result.text "\r")) "\t"))
         :eol (if (contains result.text "\r\n") "CRLF" "LF")
         :savedEol (if (contains result.text "\r\n") "CRLF" "LF")
         :dirty false
         :language (if (= label "CMakeLists.txt")
                     "CMake"
                     (or-else (get state.languages (lower-case (get (reverse (split label ".")) 0))) ""))})
  (showFileTab path label preview))

(defn revertFileFailed "What revertFile does when it fails." [error]
  (notify "error" (str "Cannot revert: " error.message)))

(defn revertFileDone "What revertFile does when it is done." [result]
  (reloaded computed.activeDoc.id result.text))

(defn saveFileFailed "What saveFile does when it fails." [doc error]
  (notify "error" (str "Cannot save " doc.label ": " error.message)))

(defn saveFileDone "What saveFile does when it is done." [doc]
  (set "state.docs"
       (map (fn [it]
              (if (= it.id doc.id)
                (merge it
                       {:saved doc.text :savedEol doc.eol :dirty (or (not= it.text doc.text) (not= it.eol doc.eol))})
                it))
            state.docs)))
