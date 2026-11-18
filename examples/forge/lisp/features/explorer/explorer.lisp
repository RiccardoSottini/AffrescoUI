;;; explorer.lisp --- Forge's explorer: the open folder's tree, and what is done to its files
;;;
;;; The tree is listed a folder at a time through the files service as it is opened, and listed again when the
;;; fileChanges watcher reports a change. New file and folder, rename, delete behind a confirm dialog, copy and paste,
;;; move by drag and drop, and reveal of the active file. state.explorerTree holds what is listed, state.explorerEdit
;;; what the dialogs and the context menu are doing.

(ns forge.features.explorer.explorer)

;;; Listing the tree

(defstate explorerTree {:entries [] :listed []})

(defcomputed explorerShown
  (filter (fn [it]
              (and (not= (indexOf it.name ".") 0)
                   (not (and (= it.kind :folder) (#{"build" "node_modules" "out" "target" "dist" "__pycache__"} it.name)))))
            state.explorerTree.entries))

(defcomputed explorerLooks
  (map (fn [it]
           {:path it.path
            :folder (= it.kind :folder)
            :look (or-else (if settings.workbench.fileIcons
                               (if (= it.kind :folder)
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
  ; style: keep, #(...) can't give a map
  (pathTree (map (fn [it] {:path it.path :icon it.look.icon :iconColor it.look.color :expandable it.folder})
                   computed.explorerLooks)))

(defcommand explorerRefresh "Refresh Explorer"
  :title ~(t "command.explorerRefresh")
  :description ~(t "command.explorerRefresh") :category ~(t "category.files") :icon "refresh-cw" []
  (when (seq state.folder.root)
    (subscribe "fileChanges.changes" :id "explorer-changes" :do explorerWatched)
    (doseq [folder (concat [""] (sortText state.explorerTree.listed))] (explorerList folder))))

(defn explorerWatched "The file watcher's events, as they come." [event]
  (explorerChanged event))

(defn explorerChanged
  "An event of the file watcher (`batch`: changes, rescan): lists again the folders it touched, or everything."
  [batch]
  (if batch.rescan
    (explorerRefresh)
    (explorerRelist (map #(str/join "/" (butlast (str/split (:path %) "/" -1))) batch.changes))))

(defn explorerRelist "Lists again the folders listed among `parents`." [parents]
  (doseq [folder (filter #(contains parents %) (concat [""] state.explorerTree.listed))]
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
    (->> result.entries (filter #(= (:kind %) :folder)) (map :name))))

(defn explorerListFailed "A folder that couldn't be listed: what was listed of it is dropped." [folder]
  (explorerListed folder (str folder "/") [] [])
  (set "state.explorerTree.listed" (remove #(= % folder) state.explorerTree.listed)))

(defn explorerListed
  "A folder's entries in the tree: what is listed under its folders still there stays, the rest goes."
  [folder prefix entries folders]
  (set "state.explorerTree.entries"
       (concat (filter (fn [it]
                         (and (not= it.parent folder)
                              (or (not (str/starts-with? it.path prefix))
                                  (contains folders (first (str/split (subs it.path (count prefix)) "/" -1))))))
                       state.explorerTree.entries)
               ; style: keep, #(...) can't give a map
               (map (fn [it] {:path (str prefix it.name) :name it.name :kind it.kind :parent folder}) entries)))
  (set "state.explorerTree.listed"
       (concat (filter (fn [it]
                         (and (not= it folder)
                              (or (not (str/starts-with? it prefix))
                                  (contains folders (first (str/split (subs it (count prefix)) "/" -1))))))
                       state.explorerTree.listed)
               (if (empty? folder) [] [folder]))))

(defn explorerExpand "A folder opened or closed (`event.expanded`): remembered, and listed the first time it opens."
  []
  (set "state.explorer.expanded"
       (if (true? event.expanded)
         (concat state.explorer.expanded [event.id])
         (remove #(= % event.id) state.explorer.expanded)))
  (when (and (true? event.expanded) (not (contains state.explorerTree.listed event.id))) (explorerList event.id)))

;;; Choosing

(defstate explorerEdit {:mode "" :id "" :name "" :naming false :confirm false :menu false :x 0 :y 0 :clipboard ""})

(defcomputed explorerTarget (or-else state.explorer.selected ""))

(defcomputed explorerTargetName (or-else (get (reverse (split computed.explorerTarget "/")) 0) ""))

(defcomputed explorerTargetKind
  (or-else (:kind (or-else (find #(= (:path %) computed.explorerTarget) state.explorerTree.entries) {})) ""))

(defcomputed explorerTargetFolder
  (if (= computed.explorerTargetKind :folder)
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
  (explorerAskName :newFile ""))

(defcommand explorerNewFolder "New Folder..."
  :title ~(t "command.explorerNewFolder")
  :description ~(t "command.explorerNewFolder") :category ~(t "category.files") :icon "folder-plus"
  :enablement ~(not (empty? state.folder.root)) []
  (explorerAskName :newFolder ""))

(defcommand explorerRename "Rename..."
  :title ~(t "command.explorerRename")
  :description ~(t "command.explorerRename") :category ~(t "category.files") :icon "pencil"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (explorerAskName :rename computed.explorerTargetName))

(defn explorerAskName "Asks for a name: a new file's, a new folder's, or a new name for the entry chosen." [mode name]
  (set "state.layout.sidebar" true)
  (set "state.explorerEdit"
       {:mode mode
        :id (if (= mode :rename) computed.explorerTarget computed.explorerTargetFolder)
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
                (->> (concat (slice (str/split state.explorerEdit.id "/" -1) 0 (if (= state.explorerEdit.mode :rename) -1 1000))
                             (str/split state.explorerEdit.name "/" -1))
                     (remove empty?)
                     (str/join "/"))))

(defn explorerMake
  "Makes what the dialog asked for (`edit`) at a path (`to`): renames, or creates once nothing is there."
  [edit to]
  (cond
    (or (str/blank? edit.name) (some #{".."} (str/split edit.name "/" -1))) (notify "error" (t "notify.nameMissing"))
    (= edit.mode :rename)
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
    (= edit.mode :newFolder)
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
  (let [inside? #(and (some? %) (or (= % from) (str/starts-with? % (str from "/"))))
        moved #(str to (subs % (count from)))
        follow #(if (inside? (:id %)) (assoc % :id (moved (:id %)) :label (baseName (moved (:id %)))) %)]
    (set "state.docs" (map follow state.docs))
    (set "state.groups"
         (map (fn [group]
                (assoc group
                       :active (if (inside? (:active group)) (moved (:active group)) (:active group))
                       :tabs (map follow (:tabs group))))
              state.groups)))
  (explorerList (str/join "/" (butlast (str/split from "/" -1))))
  (explorerShow to))

(defn explorerShow "Lists and opens the folders holding a path, then selects it." [id]
  (let [parts (str/split id "/" -1)
        folders (map #(str/join "/" (take % parts)) (range (count parts)))
        opened (rest folders)]
    (doseq [folder folders] (explorerList folder))
    (set "state.explorer.expanded" (concat (remove (into #{} opened) state.explorer.expanded) opened)))
  (set "state.explorer.selected" id))

;;; Deleting

(defcommand explorerDelete "Delete..."
  :title ~(t "command.explorerDelete")
  :description ~(t "command.explorerDelete") :category ~(t "category.files") :icon "trash-2"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (set "state.layout.sidebar" true)
  (set "state.explorerEdit"
       (assoc state.explorerEdit
              :id computed.explorerTarget
              :name computed.explorerTargetName
              :mode computed.explorerTargetKind
              :menu false
              :confirm true)))

(defn explorerDeleteConfirmed "Deletes the entry, once the dialog said yes." []
  (set "state.explorerEdit.confirm" false)
  (invoke "files.delete"
          :with {:path state.explorerEdit.id}
          :onError explorerDeleteFailed
          :onDone explorerDeleted))

(defn explorerDeleted "An entry deleted: its folder listed again." []
  (set "state.explorer.selected" nil)
  (explorerList (str/join "/" (butlast (str/split state.explorerEdit.id "/" -1)))))

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
                 :to (str/join "/" (remove empty? [computed.explorerTargetFolder (baseName state.explorerEdit.clipboard)]))}
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
                  (if (and (= event.place :inside)
                           (some #(and (= (:path %) event.id) (= (:kind %) :folder)) state.explorerTree.entries))
                    event.id
                    (str/join "/" (butlast (str/split (or-else event.id "") "/" -1)))))))

(defn explorerMove "Moves an entry into a folder, unless it is already there or the folder is inside it." [from into]
  (cond
    (or (= into from) (str/starts-with? into (str from "/")))
      (notify "error" (t "notify.moveIntoItself" {:name (baseName from)}))
    (not= into (str/join "/" (butlast (str/split from "/" -1))))
      (invoke "files.rename"
              :with {:path from :to (str/join "/" (remove empty? [into (baseName from)]))}
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
