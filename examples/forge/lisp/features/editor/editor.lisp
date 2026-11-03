;;; editor.lisp --- Forge's editor: the open files, their tabs, and saving them
;;;
;;; Each open file's text is in state.docs (`text`, and `saved`: what is on disk), read and written through the
;;; files service and shown by the editors of every group (views.lisp). A click in the explorer opens a preview tab,
;;; which the next preview replaces; a double click, an edit or Keep Editor pins it. A dirty file's tab has a
;;; marker; Save, Save All and Revert File write it or read it again. The shell and the panels use openPath and
;;; closeTab.

;;; Opening files

(defn openPath "Opens a file in a pinned tab of the active group: its path in the folder, and its label." [path label]
  (openBuffer path label false))

(defn openPreview "Opens a file in the preview tab, which the next preview replaces." [path label]
  (openBuffer path label true))

(defn openBuffer "Shows a file already open, or reads it first." [path label preview]
  (if (contains computed.docIds path)
    (showFileTab path label preview)
    (invoke "files.read"
            :with {:path path}
            :onError [openBufferFailed label]
            :onDone [openBufferDone path label preview])))

(defn openBufferDone "A file just read: kept as the editor shows it, its language told by its name, then shown."
  [path label preview result]
  (push "state.docs"
        {:id path
         :label label
         :kind "file"
         :text (asShown result.text)
         :saved (asShown result.text)
         :eol (lineEndings result.text)
         :savedEol (lineEndings result.text)
         :dirty false
         :language (if (= label "CMakeLists.txt")
                     "CMake"
                     (or-else (get state.languages (lower-case (get (reverse (split label ".")) 0))) ""))})
  (showFileTab path label preview))

(defn openBufferFailed "Says why a file couldn't be opened." [label error]
  (notify "error" (t "notify.cannotOpen" {:file label :error error.message})))

(defn asShown "A file's text as the editor shows it: no carriage returns, and a tab as the tab size's spaces." [text]
  (join (pad "" (or-else settings.editor.tabSize 4) " ") (split (join "" (split text "\r")) "\t")))

(defn lineEndings "Which line endings a file's text has: CRLF or LF." [text]
  (if (contains text "\r\n") "CRLF" "LF"))

(defn previewSelectedFile "The explorer's selection, a file, opens in the preview tab." []
  (when (= (:kind (or-else (find (fn [it] (= it.path event.value)) state.explorerTree.entries) {})) "file")
    (openPreview event.value (baseName event.value))))

(defpart follow-explorer "A file chosen in the explorer opens in the preview tab."
  :watch {:state.explorer.selected previewSelectedFile})

;;; Tabs

(defn showFileTab "Shows an open file's tab in the active group: pinned, or as the preview, which replaces the last one."
  [id label preview]
  (forgeRecordNav id)
  (cond
    (some (fn [it] (= it.id id)) (activeTabs)) (when (not preview) (pinTab id))
    (and preview (some (fn [it] it.preview) (activeTabs)))
      (replacePreview id label (:id (find (fn [it] it.preview) (activeTabs))))
    :else (push (str "state.groups[" state.activeGroup "].tabs") {:id id :label label :preview preview}))
  (set (str "state.groups[" state.activeGroup "].active") id)
  (rememberRecent id label))

(defn replacePreview "The preview tab becomes another file's; the file it showed is let go." [id label old]
  (set (str "state.groups[" state.activeGroup "].tabs")
       (map (fn [it] (if it.preview {:id id :label label :preview true} it))
            (get-in state.groups [state.activeGroup :tabs])))
  (releaseBuffer old))

(defn pinTab "A preview tab stays: the next preview opens beside it." [id]
  (set "state.groups"
       (map (fn [it] (merge it {:tabs (map (fn [it] (if (= it.id id) (merge it {:preview false}) it)) it.tabs)}))
            state.groups)))

(defcommand keepEditor "Pins the preview tab, so the next preview opens beside it."
  :title ~(t "command.keepEditor") :category ~(t "category.view") :icon "pin"
  :description ~(t "command.keepEditor.about")
  :enablement ~(some (fn [it] it.preview) (get-in state.groups [state.activeGroup :tabs])) []
  (pinTab (activeTabId)))

(defn releaseBuffer "Lets go of a file no tab shows any more, unless it has unsaved edits." [id]
  (when (and (not (contains (map (fn [it] (contains (map (fn [it] it.id) it.tabs) id)) state.groups) true))
             (some (fn [it] (and (= it.id id) (= it.kind "file") (not it.dirty))) state.docs))
    (set "state.docs" (filter (fn [it] (not= it.id id)) state.docs))
    (lspClosed id)))

(defn focusGroup "A click in an editor group makes it the active one." [gi]
  (set "state.activeGroup" gi))

(defn closeTab "Closes a tab of a group; a group left empty closes too, unless it is the last." [gi id]
  (set (str "state.groups[" gi "].tabs") (filter (fn [it] (not= it.id id)) (get-in state.groups [gi :tabs])))
  (when (= (get-in state.groups [gi :active]) id)
    (set (str "state.groups[" gi "].active")
         (if (> (count (get-in state.groups [gi :tabs])) 0) (get-in state.groups [gi :tabs 0 :id]) nil)))
  (when (and (= (count (get-in state.groups [gi :tabs])) 0) (> (count state.groups) 1))
    (remove-state "state.groups" :at gi)
    (set "state.activeGroup" 0)
    (set "state.layout.groupSizes" []))
  (releaseBuffer id))

;;; Editing and saving

(defn textChanged "A file's editor changed its text (`event.value`): it is dirty while the text isn't the one saved."
  [id]
  (doseq [doc (filter (fn [it] (= it.id id)) state.docs)]
    (let [dirty (or (not= event.value doc.saved) (not= doc.eol doc.savedEol))]
      (when (not= dirty doc.dirty) (markDirty id dirty))))
  (lspChanged id))

(defn markDirty "Marks a file as edited or saved; an edit pins its tab." [id dirty]
  (set "state.docs" (map (fn [it] (if (= it.id id) (merge it {:dirty dirty}) it)) state.docs))
  (when dirty (pinTab id)))

(defcommand save "Writes the active file."
  :title ~(t "command.save") :category ~(t "category.file") :icon "save"
  :enablement ~(= computed.activeDoc.kind "file") []
  (saveFile computed.activeDoc))

(defcommand saveAll "Writes every file with unsaved edits."
  :title ~(t "command.saveAll") :category ~(t "category.file") :icon "save-all"
  :enablement ~(> (count computed.dirtyIds) 0) []
  (doseq [doc (filter (fn [it] (and (= it.kind "file") it.dirty)) state.docs)] (saveFile doc)))

(defn saveFile "Writes an open file's text, with its line endings, and marks it saved." [doc]
  (invoke "files.write"
          :with {:path doc.id :text (if (= doc.eol "CRLF") (join "\r\n" (split doc.text "\n")) doc.text)}
          :onError [saveFileFailed doc]
          :onDone [saveFileDone doc]))

(defn saveFileDone "The text written is the file's saved text; edits made since keep it dirty." [doc]
  (set "state.docs"
       (map (fn [it]
              (if (= it.id doc.id)
                (merge it
                       {:saved doc.text :savedEol doc.eol :dirty (or (not= it.text doc.text) (not= it.eol doc.eol))})
                it))
            state.docs)))

(defn saveFileFailed "Says why a file couldn't be saved." [doc error]
  (notify "error" (t "notify.cannotSave" {:file doc.label :error error.message})))

(defcommand revertFile "Reads the file again, dropping the edits."
  :title ~(t "command.revertFile") :category ~(t "category.file") :icon "undo"
  :description ~(t "command.revertFile.about")
  :enablement ~(and (= computed.activeDoc.kind "file") computed.activeDoc.dirty) []
  (invoke "files.read" :with {:path computed.activeDoc.id} :onError revertFileFailed :onDone revertFileDone))

(defn revertFileDone "The file as it is on disk replaces the edited text." [result]
  (reloaded computed.activeDoc.id result.text))

(defn revertFileFailed "Says why the file couldn't be read again." [error]
  (notify "error" (t "notify.cannotRevert" {:error error.message})))

(defn reloaded "An open file's text as it is now on disk (`text`), kept as the editor shows it, so undoing back to it is clean."
  [id text]
  (set "state.docs"
       (map (fn [it]
              (if (= it.id id)
                (merge it
                       {:text (asShown text) :saved (asShown text) :eol (lineEndings text) :savedEol (lineEndings text)
                        :dirty false})
                it))
            state.docs)))

(defkey "mod+s" save)

(defkey "mod+alt+s" saveAll)

;;; Problems

(defcommand scanProblems "Lists the TODO and FIXME lines of the open files in Problems, after the language server's."
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
  (log (t "log.problems" {:n (count state.problems)})))
