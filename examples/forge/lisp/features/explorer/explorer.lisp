;;; explorer.lisp --- Forge's explorer: the open folder's tree, and what is done to its files
;;;
;;; The tree is listed a folder at a time through the files service as it is opened, and listed again when the
;;; fileChanges watcher reports a change. New file and folder, rename, delete behind a confirm dialog, copy and paste,
;;; move by drag and drop, and reveal of the active file. state.explorerTree holds what is listed, state.explorerEdit
;;; what the dialogs and the context menu are doing.

;;; Listing the tree

(defstate explorerTree {:entries [] :listed []})

(defcomputed explorerShown
  (filter (fn [it]
              (and (not= (indexOf it.name ".") 0)
                   (not (and (= it.kind "folder")
                             (contains ["build" "node_modules" "out" "target" "dist" "__pycache__"] it.name)))))
            state.explorerTree.entries))

(defcomputed explorerLooks
  (map (fn [it]
           {:path it.path
            :folder (= it.kind "folder")
            :look (or-else (if settings.workbench.fileIcons
                               (if (= it.kind "folder")
                                   state.iconTheme.folder
                                   (or-else (get state.iconTheme.names it.name)
                                            (if (contains it.name ".")
                                                (get state.iconTheme.extensions
                                                     (lower-case (get (reverse (split it.name ".")) 0)))
                                                nil)
                                            state.iconTheme.file))
                               nil)
                           {})})
         computed.explorerShown))

(defcomputed explorerItems
  (pathTree (map (fn [it] {:path it.path :icon it.look.icon :iconColor it.look.color :expandable it.folder})
                   computed.explorerLooks)))

(defcommand explorerRefresh "Refresh Explorer"
  :title ~(t "command.explorerRefresh")
  :description ~(t "command.explorerRefresh") :category ~(t "category.files") :icon "refresh-cw" []
  (when (not (empty? state.folder.root))
    (subscribe "fileChanges.changes" :id "explorer-changes" :do explorerWatched)
    (doseq [folder (concat [""] (sortText state.explorerTree.listed))] (explorerList folder))))

(defn explorerWatched "The file watcher's events, as they come." [event]
  (explorerChanged event))

(defn explorerChanged
  "An event of the file watcher (`batch`: changes, rescan): lists again the folders it touched, or everything."
  [batch]
  (if batch.rescan
    (explorerRefresh)
    (explorerRelist (map (fn [it] (join "/" (slice (split it.path "/") 0 (- 1)))) batch.changes))))

(defn explorerRelist "Lists again the folders listed among `parents`." [parents]
  (doseq [folder (filter (fn [it] (contains parents it)) (concat [""] state.explorerTree.listed))]
    (explorerList folder)))

(defn explorerList "Lists a folder ('' for the root), replacing what was listed of it." [folder]
  (invoke "files.list"
          :with {:path (if (empty? folder) "." folder)}
          :onDone [explorerListRead folder]
          :onError [explorerListFailed folder]))

(defn explorerListRead "A folder's entries, listed." [folder result]
  (explorerListed
    folder
    (if (empty? folder) "" (str folder "/"))
    result.entries
    (map (fn [it] it.name) (filter (fn [it] (= it.kind "folder")) result.entries))))

(defn explorerListFailed "A folder that couldn't be listed: what was listed of it is dropped." [folder]
  (explorerListed folder (str folder "/") [] [])
  (set "state.explorerTree.listed" (filter (fn [it] (not= it folder)) state.explorerTree.listed)))

(defn explorerListed
  "A folder's entries in the tree: what is listed under its folders still there stays, the rest goes."
  [folder prefix entries folders]
  (set "state.explorerTree.entries"
       (concat (filter (fn [it]
                         (and (not= it.parent folder)
                              (or (not= (indexOf it.path prefix) 0)
                                  (contains folders (get (split (slice it.path (count prefix)) "/") 0)))))
                       state.explorerTree.entries)
               (map (fn [it] {:path (str prefix it.name) :name it.name :kind it.kind :parent folder}) entries)))
  (set "state.explorerTree.listed"
       (concat (filter (fn [it]
                         (and (not= it folder)
                              (or (not= (indexOf it prefix) 0)
                                  (contains folders (get (split (slice it (count prefix)) "/") 0)))))
                       state.explorerTree.listed)
               (if (empty? folder) [] [folder]))))

(defn explorerExpand "A folder opened or closed (`event.expanded`): remembered, and listed the first time it opens."
  []
  (set "state.explorer.expanded"
       (if event.expanded
         (concat state.explorer.expanded [event.id])
         (filter (fn [it] (not= it event.id)) state.explorer.expanded)))
  (when (and event.expanded (not (contains state.explorerTree.listed event.id))) (explorerList event.id)))

;;; Choosing

(defstate explorerEdit {:mode "" :id "" :name "" :naming false :confirm false :menu false :x 0 :y 0 :clipboard ""})

(defcomputed explorerTarget (or-else state.explorer.selected ""))

(defcomputed explorerTargetName (or-else (get (reverse (split computed.explorerTarget "/")) 0) ""))

(defcomputed explorerTargetKind
  (or-else (:kind (or-else (find (fn [it] (= it.path computed.explorerTarget)) state.explorerTree.entries) {})) ""))

(defcomputed explorerTargetFolder
  (if (= computed.explorerTargetKind "folder")
        computed.explorerTarget
        (join "/" (slice (split computed.explorerTarget "/") 0 (- 1)))))

(defn explorerMenu "The explorer's menu, opened where the right-click was." []
  (set "state.explorerEdit.x" event.x)
  (set "state.explorerEdit.y" event.y)
  (set "state.explorerEdit.menu" true))

;;; New files and folders, and renaming

(defcommand explorerNewFile "New File..."
  :title ~(t "command.explorerNewFile")
  :description ~(t "command.explorerNewFile") :category ~(t "category.files") :icon "file-plus"
  :enablement ~(not (empty? state.folder.root)) []
  (explorerAskName "newFile" ""))

(defcommand explorerNewFolder "New Folder..."
  :title ~(t "command.explorerNewFolder")
  :description ~(t "command.explorerNewFolder") :category ~(t "category.files") :icon "folder-plus"
  :enablement ~(not (empty? state.folder.root)) []
  (explorerAskName "newFolder" ""))

(defcommand explorerRename "Rename..."
  :title ~(t "command.explorerRename")
  :description ~(t "command.explorerRename") :category ~(t "category.files") :icon "pencil"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (explorerAskName "rename" computed.explorerTargetName))

(defn explorerAskName "Asks for a name: a new file's, a new folder's, or a new name for the entry chosen." [mode name]
  (set "state.layout.sidebar" true)
  (set "state.explorerEdit"
       {:mode mode
        :id (if (= mode "rename") computed.explorerTarget computed.explorerTargetFolder)
        :name name
        :naming true
        :confirm false
        :menu false
        :x 0
        :y 0
        :clipboard state.explorerEdit.clipboard}))

(defn explorerSubmitName "The name typed, made a path in the folder chosen." []
  (set "state.explorerEdit.naming" false)
  (explorerMake state.explorerEdit
                (join "/"
                      (filter (fn [it] (not (empty? it)))
                              (concat (slice (split state.explorerEdit.id "/")
                                             0
                                             (if (= state.explorerEdit.mode "rename") (- 1) 1000))
                                      (split state.explorerEdit.name "/"))))))

(defn explorerMake
  "Makes what the dialog asked for (`edit`) at a path (`to`): renames, or creates once nothing is there."
  [edit to]
  (cond
    (or (empty? (trim edit.name)) (contains (split edit.name "/") ".."))
      (notify "error" (t "notify.nameMissing"))
    (= edit.mode "rename")
      (invoke "files.rename"
              :with {:path edit.id :to to}
              :onError explorerRenameFailed
              :onDone [explorerRenamed edit to])
    :else (invoke "files.stat" :with {:path to} :onDone [explorerNameChecked edit to])))

(defn explorerRenamed "An entry renamed: tabs follow it." [edit to]
  (explorerMoved edit.id to))

(defn explorerRenameFailed "Says why the entry couldn't be renamed." [error]
  (notify "error" (t "notify.cannotRename" {:error error.message})))

(defn explorerNameChecked "Nothing is at the path: the new file or folder is made; else says so." [edit to result]
  (cond
    result.exists (notify "error" (t "notify.nameTaken" {:name edit.name}))
    (= edit.mode "newFolder")
      (invoke "files.mkdir" :with {:path to} :onError explorerCreateFailed :onDone [explorerFolderMade to])
    :else
      (invoke "files.write"
              :with {:path to :text ""}
              :onError explorerCreateFailed
              :onDone [explorerFileMade to])))

(defn explorerFolderMade "A new folder: the explorer shows it." [to]
  (explorerShow to))

(defn explorerFileMade "A new file: the explorer shows it, and it opens." [to]
  (explorerShow to)
  (openPath to (baseName to)))

(defn explorerCreateFailed "Says why the file or folder couldn't be made." [error]
  (notify "error" (t "notify.cannotCreate" {:error error.message})))

(defn explorerMoved "A file or folder moved (`from`, `to`): tabs follow it, and the explorer shows it." [from to]
  (set "state.docs"
       (map (fn [it]
              (if (or (= it.id from) (= (indexOf it.id (str from "/")) 0))
                (merge it
                       {:id (str to (slice it.id (count from))) :label (baseName (str to (slice it.id (count from))))})
                it))
            state.docs))
  (set "state.groups"
       (map (fn [it]
              (merge it
                     {:active (if (or (= it.active from) (= (indexOf (or-else it.active "") (str from "/")) 0))
                                (str to (slice it.active (count from)))
                                it.active)
                      :tabs (map (fn [it]
                                   (if (or (= it.id from) (= (indexOf it.id (str from "/")) 0))
                                     (merge it
                                            {:id (str to (slice it.id (count from)))
                                             :label (baseName (str to (slice it.id (count from))))})
                                     it))
                                 it.tabs)}))
            state.groups))
  (explorerList (join "/" (slice (split from "/") 0 (- 1))))
  (explorerShow to))

(defn explorerShow "Lists and opens the folders holding a path, then selects it." [id]
  (doseq [folder (map (fn [it] (join "/" (slice (split id "/") 0 it))) (range (count (split id "/"))))]
    (explorerList folder))
  (set "state.explorer.expanded"
       (concat (filter (fn [it]
                         (not (contains (map (fn [it] (join "/" (slice (split id "/") 0 (+ it 1))))
                                             (range (- (count (split id "/")) 1)))
                                        it)))
                       state.explorer.expanded)
               (map (fn [it] (join "/" (slice (split id "/") 0 (+ it 1)))) (range (- (count (split id "/")) 1)))))
  (set "state.explorer.selected" id))

;;; Deleting

(defcommand explorerDelete "Delete..."
  :title ~(t "command.explorerDelete")
  :description ~(t "command.explorerDelete") :category ~(t "category.files") :icon "trash-2"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (set "state.layout.sidebar" true)
  (set "state.explorerEdit"
       (merge state.explorerEdit
              {:id computed.explorerTarget
               :name computed.explorerTargetName
               :mode computed.explorerTargetKind
               :menu false
               :confirm true})))

(defn explorerDeleteConfirmed "Deletes the entry, once the dialog said yes." []
  (set "state.explorerEdit.confirm" false)
  (invoke "files.delete"
          :with {:path state.explorerEdit.id}
          :onError explorerDeleteFailed
          :onDone explorerDeleted))

(defn explorerDeleted "An entry deleted: its folder listed again." []
  (set "state.explorer.selected" nil)
  (explorerList (join "/" (slice (split state.explorerEdit.id "/") 0 (- 1)))))

(defn explorerDeleteFailed "Says why the entry couldn't be deleted." [error]
  (notify "error" (t "notify.cannotDelete" {:error error.message})))

;;; Copying, pasting and moving

(defcommand explorerCopy "Copy"
  :title ~(t "command.explorerCopy")
  :description ~(t "command.explorerCopy") :category ~(t "category.files") :icon "copy"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (set "state.explorerEdit.clipboard" computed.explorerTarget))

(defcommand explorerPaste "Paste"
  :title ~(t "command.explorerPaste")
  :description ~(t "command.explorerPaste") :category ~(t "category.files") :icon "clipboard-paste"
  :enablement ~(not (empty? state.explorerEdit.clipboard)) []
  (invoke "files.copy"
          :with {:path state.explorerEdit.clipboard
                 :free true
                 :to (join "/"
                           (filter (fn [it] (not (empty? it)))
                                   [computed.explorerTargetFolder (baseName state.explorerEdit.clipboard)]))}
          :onError explorerPasteFailed
          :onDone explorerPasted))

(defn explorerPasted "A copy pasted: the explorer shows it." [result]
  (explorerShow result.path))

(defn explorerPasteFailed "Says why the copy couldn't be pasted." [error]
  (notify "error" (t "notify.cannotPaste" {:error error.message})))

(defn explorerDrop
  "Entries dropped (`event`: data.ids, id): moved into the folder dropped on, or the one holding the file."
  []
  (doseq [from (or-else event.data.ids [])]
    (explorerMove from
                  (if (and (= event.place "inside")
                           (some (fn [it] (and (= it.path event.id) (= it.kind "folder"))) state.explorerTree.entries))
                    event.id
                    (join "/" (slice (split (or-else event.id "") "/") 0 (- 1)))))))

(defn explorerMove "Moves an entry into a folder, unless it is already there or the folder is inside it." [from into]
  (cond
    (or (= into from) (= (indexOf into (str from "/")) 0))
      (notify "error" (t "notify.moveIntoItself" {:name (baseName from)}))
    (not= into (join "/" (slice (split from "/") 0 (- 1))))
      (invoke "files.rename"
              :with {:path from :to (join "/" (filter (fn [it] (not (empty? it))) [into (baseName from)]))}
              :onError explorerMoveFailed
              :onDone [explorerMovedTo from])))

(defn explorerMovedTo "An entry moved: tabs follow it." [from result]
  (explorerMoved from result.path))

(defn explorerMoveFailed "Says why the entry couldn't be moved." [error]
  (notify "error" (t "notify.cannotMove" {:error error.message})))

;;; Folders and the active file

(defcommand explorerCollapseAll "Collapse Folders in Explorer"
  :title ~(t "command.explorerCollapseAll")
  :description ~(t "command.explorerCollapseAll") :category ~(t "category.files") :icon "copy-minus"
  :enablement ~(not (empty? state.explorer.expanded)) []
  (set "state.explorer.expanded" []))

(defcommand explorerReveal "Reveal Active File in Explorer"
  :title ~(t "command.explorerReveal")
  :description ~(t "command.explorerReveal") :category ~(t "category.files") :icon "locate"
  :enablement ~(and (some? computed.activeDoc) (= computed.activeDoc.kind "file")) []
  (set "state.layout.sidebar" true)
  (set "state.sideView" "explorer")
  (explorerShow computed.activeDoc.id))
