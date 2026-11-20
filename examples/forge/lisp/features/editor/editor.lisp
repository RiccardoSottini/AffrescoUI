;;; editor.lisp --- Forge's editor: the open files, their tabs, and saving them
;;;
;;; Each open file's text is in state.docs (`text`, and `saved`: what is on disk), read and written through the
;;; files service and shown by the editors of every group (views.lisp). A click in the explorer opens a preview tab,
;;; which the next preview replaces; a double click, an edit or Keep Editor pins it. A dirty file's tab has a
;;; marker; Save, Save All and Revert File write it or read it again. The shell and the panels use openPath and
;;; closeTab.

(ns forge.features.editor.editor)

;;; Opening files

(defn open-path "Opens a file in a pinned tab of the active group: its path in the folder, and its label." [path label]
  (open-buffer path label false))

(defn open-preview "Opens a file in the preview tab, which the next preview replaces." [path label]
  (open-buffer path label true))

(defn open-buffer "Shows a file already open, or reads it first." [path label preview]
  (if (contains computed.docIds path)
    (show-file-tab path label preview)
    (invoke "files.read"
            :with {:path path}
            :onError [open-buffer-failed label]
            :onDone [open-buffer-done path label preview])))

(defn open-buffer-done "A file just read: kept as the editor shows it, its language told by its name, then shown."
  [path label preview result]
  (push "state.docs"
        {:id path
         :label label
         :kind "file"
         :text (as-shown result.text)
         :saved (as-shown result.text)
         :eol (line-endings result.text)
         :savedEol (line-endings result.text)
         :dirty false
         :language (if (= label "CMakeLists.txt")
                     "CMake"
                     (or-else (get state.languages (str/lower-case (last (str/split label "." -1)))) ""))})
  (show-file-tab path label preview))

(defn open-buffer-failed "Says why a file couldn't be opened." [label error]
  (notify "error" (t "notify.cannotOpen" {:file label :error error.message})))

(defn as-shown "A file's text as the editor shows it: no carriage returns, and a tab as the tab size's spaces." [text]
  (-> text (str/replace "\r" "") (str/replace "\t" (pad "" (or-else settings.editor.tabSize 4) " "))))

(defn line-endings "Which line endings a file's text has: CRLF or LF." [text]
  (if (str/includes? text "\r\n") "CRLF" "LF"))

(defn preview-selected-file "The explorer's selection, a file, opens in the preview tab." []
  (when (= (:kind (find #(= (:path %) event.value) state.explorerTree.entries)) :file)
    (open-preview event.value (base-name event.value))))

(defpart follow-explorer "A file chosen in the explorer opens in the preview tab."
  :watch {:state.explorer.selected preview-selected-file})

;;; Tabs

(defn show-file-tab
  "Shows an open file's tab in the active group: pinned, or as the preview, which replaces the last one."
  [id label preview]
  (forge-record-nav id)
  (cond
    (some #(= (:id %) id) (active-tabs)) (when-not preview (pin-tab id))
    (and preview (some :preview (active-tabs))) (replace-preview id label (:id (find :preview (active-tabs))))
    :else (push (str "state.groups[" state.activeGroup "].tabs") {:id id :label label :preview preview}))
  (set (str "state.groups[" state.activeGroup "].active") id)
  (remember-recent id label))

(defn replace-preview "The preview tab becomes another file's; the file it showed is let go." [id label old]
  (set (str "state.groups[" state.activeGroup "].tabs")
       (map #(if (:preview %) {:id id :label label :preview true} %)
            (get-in state.groups [state.activeGroup :tabs])))
  (release-buffer old))

(defn pin-tab "A preview tab stays: the next preview opens beside it." [id]
  (set "state.groups"
       ; style: keep, #(...) can't hold another #(...)
       (map (fn [group] (update group :tabs (partial map #(if (= (:id %) id) (assoc % :preview false) %))))
            state.groups)))

(defcommand keepEditor "Pins the preview tab, so the next preview opens beside it."
  :title ~(t "command.keepEditor") :category ~(t "category.view") :icon "pin"
  :description ~(t "command.keepEditor.about")
  :enablement ~(some :preview (get-in state.groups [state.activeGroup :tabs])) []
  (pin-tab (active-tab-id)))

(defn release-buffer "Lets go of a file no tab shows any more, unless it has unsaved edits." [id]
  ; style: keep, #(...) can't hold another #(...)
  (when (and (not-any? (fn [group] (some #(= (:id %) id) (:tabs group))) state.groups)
             (some #(and (= (:id %) id) (= (:kind %) :file) (not (:dirty %))) state.docs))
    (set "state.docs" (remove #(= (:id %) id) state.docs))
    (lsp-closed id)))

(defn focus-group "A click in an editor group makes it the active one." [gi]
  (set "state.activeGroup" gi))

(defstate editorMenu {:open false :x 0 :y 0})

(defn open-editor-menu
  "The editor's menu, opened where the click was (kept inside the window), for the group `gi`'s active editor."
  [gi]
  (focus-group gi)
  (set "state.editorMenu" {:open true :x (min event.x (- window.width 250)) :y event.y}))

(defn split-from-group "Split Editor, from a group's head: that group's active editor, beside it." [gi]
  (focus-group gi)
  (splitEditor))

(defn close-tab
  "Closes a tab of a group (a file's is remembered for Reopen Closed Editor); a group left empty closes too, unless it
is the last."
  [gi id]
  (doseq [doc (filter #(and (= (:id %) id) (= (:kind %) :file)) state.docs)]
    (remember-closed id doc.label))
  (set (str "state.groups[" gi "].tabs") (remove #(= (:id %) id) (get-in state.groups [gi :tabs])))
  (when (= (get-in state.groups [gi :active]) id)
    (set (str "state.groups[" gi "].active") (get-in state.groups [gi :tabs 0 :id])))
  (when (and (empty? (get-in state.groups [gi :tabs])) (> (count state.groups) 1))
    (remove-state "state.groups" :at gi)
    (set "state.activeGroup" 0)
    (set "state.layout.groupSizes" []))
  (release-buffer id))

;;; Editing and saving

(defn text-changed "A file's editor changed its text (`event.value`): it is dirty while the text isn't the one saved."
  [id]
  (doseq [doc (filter #(= (:id %) id) state.docs)]
    (let [dirty (or (not= event.value doc.saved) (not= doc.eol doc.savedEol))]
      (when (not= dirty doc.dirty) (mark-dirty id dirty))))
  (lsp-changed id))

(defn mark-dirty "Marks a file as edited or saved; an edit pins its tab." [id dirty]
  (set "state.docs" (map #(if (= (:id %) id) (assoc % :dirty dirty) %) state.docs))
  (when dirty (pin-tab id)))

(defcommand save "Writes the active file."
  :title ~(t "command.save") :description ~(t "command.save") :category ~(t "category.file") :icon "save"
  :enablement ~(= computed.activeDoc.kind "file") []
  (saveFile computed.activeDoc))

(defcommand saveAll "Writes every file with unsaved edits."
  :title ~(t "command.saveAll") :description ~(t "command.saveAll") :category ~(t "category.file") :icon "save-all"
  :enablement ~(> (count computed.dirtyIds) 0) []
  (doseq [doc (filter #(and (= (:kind %) :file) (:dirty %)) state.docs)] (saveFile doc)))

; style: keep, named in tools/affrescoui/check.py
(defn saveFile "Writes an open file's text, with its line endings, and marks it saved." [doc]
  (invoke "files.write"
          :with {:path doc.id :text (if (= doc.eol "CRLF") (str/replace doc.text "\n" "\r\n") doc.text)}
          :onError [save-file-failed doc]
          :onDone [save-file-done doc]))

(defn save-file-done "The text written is the file's saved text; edits made since keep it dirty." [doc]
  (set "state.docs"
       (map (fn [it]
              (if (= it.id doc.id)
                (assoc it :saved doc.text :savedEol doc.eol :dirty (or (not= it.text doc.text) (not= it.eol doc.eol)))
                it))
            state.docs)))

(defn save-file-failed "Says why a file couldn't be saved." [doc error]
  (notify "error" (t "notify.cannotSave" {:file doc.label :error error.message})))

(defcommand revertFile "Reads the file again, dropping the edits."
  :title ~(t "command.revertFile") :category ~(t "category.file") :icon "undo"
  :description ~(t "command.revertFile.about")
  :enablement ~(and (= computed.activeDoc.kind "file") computed.activeDoc.dirty) []
  (invoke "files.read" :with {:path computed.activeDoc.id} :onError revert-file-failed :onDone revert-file-done))

(defn revert-file-done "The file as it is on disk replaces the edited text." [result]
  (reloaded computed.activeDoc.id result.text))

(defn revert-file-failed "Says why the file couldn't be read again." [error]
  (notify "error" (t "notify.cannotRevert" {:error error.message})))

(defn reloaded
  "An open file's text as it is now on disk (`text`), kept as the editor shows it, so undoing back to it is clean."
  [id text]
  (set "state.docs"
       (map (fn [it]
              (if (= it.id id)
                (assoc it
                       :text (as-shown text) :saved (as-shown text) :eol (line-endings text) :savedEol (line-endings text)
                       :dirty false)
                it))
            state.docs)))

(defkey "mod+s" save)

(defkey "mod+alt+s" saveAll)

;;; Problems

(defcommand scanProblems "Lists the TODO and FIXME lines of the open files in Problems, after the language server's."
  :title ~(t "command.scanProblems") :category ~(t "category.problems") :icon "search"
  :description ~(t "command.scanProblems.about") []
  (set "state.problems" (filter :server state.problems))
  (doseq [doc (filter #(and (= (:kind %) :file) (<= (count (:text %)) 1000000)) state.docs)]
    (set "state.problems"
         (slice (concat state.problems
                        (->> (str/split doc.text "\n" -1)
                             (map-indexed (fn [index row] {:line (inc index) :row (str/trim row)}))
                             (filter #(or (str/includes? (:row %) "TODO") (str/includes? (:row %) "FIXME")))
                             (map (fn [it]
                                    {:severity (if (str/includes? it.row "FIXME") :warning :info)
                                     :message (slice it.row 0 120)
                                     :file doc.id
                                     :label doc.label
                                     :line it.line}))))
                0
                1000)))
  (log-line (t "log.problems" {:n (count state.problems)})))
