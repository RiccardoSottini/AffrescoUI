;;; explorer.lisp --- Forge's explorer: the open folder's tree, listed a folder at a time through the files service as it is opened and listed again when the fileChanges watcher reports a change; new file and folder, rename, delete behind a confirm dialog, copy and paste, move by drag and drop, and reveal of the active file
;;;
;;; state.explorerTree holds what is listed, state.explorerEdit what the dialogs and the context menu are doing.

;;; State and definitions

(defstate explorerTree {:entries [] :listed []})

(defstate explorerEdit {:mode "" :id "" :name "" :naming false :confirm false :menu false :x 0 :y 0 :clipboard ""})

(defcomputed explorerTarget (or-else state.explorer.selected ""))

(defcomputed explorerTargetName (or-else (get (reverse (split computed.explorerTarget "/")) 0) ""))

(defcomputed explorerTargetKind
  (or-else (:kind (or-else (find (fn [it] (= it.path computed.explorerTarget)) state.explorerTree.entries) {})) ""))

(defcomputed explorerTargetFolder
  (if (= computed.explorerTargetKind "folder")
        computed.explorerTarget
        (join "/" (slice (split computed.explorerTarget "/") 0 (- 1)))))

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

;;; Functions and commands

(defcommand explorerRefresh "Refresh Explorer"
  :title ~(t "command.explorerRefresh") :category ~(t "category.files") :icon "refresh-cw" []
  (when (not (empty? state.folder.root))
    (subscribe "fileChanges.changes" :id "explorer-changes" :do explorerRefreshEach)
    (doseq [folder (concat [""] (sortText state.explorerTree.listed))] (explorerList folder))))

(defn explorerChanged "batch: an event of the file watcher, {changes: [{path, kind}], rescan}. Lists again the folders it touched"
  [batch]
  (if batch.rescan
    (explorerRefresh)
    (explorerRelist (map (fn [it] (join "/" (slice (split it.path "/") 0 (- 1)))) batch.changes))))

(defn explorerRelist "TODO" [parents]
  (doseq [folder (filter (fn [it] (contains parents it)) (concat [""] state.explorerTree.listed))]
    (explorerList folder)))

(defn explorerList "folder: a folder's path ('' for the root). Lists it, replacing what was listed of it" [folder]
  (invoke "files.list"
          :with {:path (if (empty? folder) "." folder)}
          :onDone [explorerListDone folder]
          :onError [explorerListFailed folder]))

(defn explorerListed "Keeps what is listed under the folders still there, drops the rest"
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

(defn explorerExpand "TODO" []
  (set "state.explorer.expanded"
       (if event.expanded
         (concat state.explorer.expanded [event.id])
         (filter (fn [it] (not= it event.id)) state.explorer.expanded)))
  (when (and event.expanded (not (contains state.explorerTree.listed event.id))) (explorerList event.id)))

(defcommand explorerNewFile "New File..."
  :title ~(t "command.explorerNewFile") :category ~(t "category.files") :icon "file-plus"
  :enablement ~(not (empty? state.folder.root)) []
  (explorerAskName "newFile" ""))

(defcommand explorerNewFolder "New Folder..."
  :title ~(t "command.explorerNewFolder") :category ~(t "category.files") :icon "folder-plus"
  :enablement ~(not (empty? state.folder.root)) []
  (explorerAskName "newFolder" ""))

(defcommand explorerRename "Rename..."
  :title ~(t "command.explorerRename") :category ~(t "category.files") :icon "pencil"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (explorerAskName "rename" computed.explorerTargetName))

(defn explorerAskName "TODO" [mode name]
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

(defn explorerSubmitName "TODO" []
  (set "state.explorerEdit.naming" false)
  (explorerMake state.explorerEdit
                (join "/"
                      (filter (fn [it] (not (empty? it)))
                              (concat (slice (split state.explorerEdit.id "/")
                                             0
                                             (if (= state.explorerEdit.mode "rename") (- 1) 1000))
                                      (split state.explorerEdit.name "/"))))))

(defn explorerMake "edit: state.explorerEdit; to: the path it names" [edit to]
  (cond
    (or (empty? (trim edit.name)) (contains (split edit.name "/") ".."))
      (notify "error" "A file or folder name must be given")
    (= edit.mode "rename")
      (invoke "files.rename"
              :with {:path edit.id :to to}
              :onError explorerMakeFailed
              :onDone [explorerMakeDone edit to])
    :else (invoke "files.stat" :with {:path to} :onDone [explorerMakeDone2 edit to])))

(defn explorerMoved "from, to: a file or folder moved. Tabs follow it; the explorer shows it" [from to]
  (set "state.docs"
       (map (fn [it]
              (if (or (= it.id from) (= (indexOf it.id (str from "/")) 0))
                (merge it
                       {:id (str to (slice it.id (count from)))
                        :label (get (reverse (split (str to (slice it.id (count from))) "/")) 0)})
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
                                             :label (get (reverse (split (str to (slice it.id (count from))) "/")) 0)})
                                     it))
                                 it.tabs)}))
            state.groups))
  (explorerList (join "/" (slice (split from "/") 0 (- 1))))
  (explorerShow to))

(defn explorerShow "id: a path. Lists and opens its folders, then selects it" [id]
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

(defcommand explorerDelete "Delete..."
  :title ~(t "command.explorerDelete") :category ~(t "category.files") :icon "trash-2"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (set "state.layout.sidebar" true)
  (set "state.explorerEdit"
       (merge state.explorerEdit
              {:id computed.explorerTarget
               :name computed.explorerTargetName
               :mode computed.explorerTargetKind
               :menu false
               :confirm true})))

(defn explorerDeleteConfirmed "TODO" []
  (set "state.explorerEdit.confirm" false)
  (invoke "files.delete"
          :with {:path state.explorerEdit.id}
          :onError explorerDeleteConfirmedFailed
          :onDone explorerDeleteConfirmedDone))

(defcommand explorerCopy "Copy"
  :title ~(t "command.explorerCopy") :category ~(t "category.files") :icon "copy"
  :enablement ~(not (empty? computed.explorerTarget)) []
  (set "state.explorerEdit.clipboard" computed.explorerTarget))

(defcommand explorerPaste "Paste"
  :title ~(t "command.explorerPaste") :category ~(t "category.files") :icon "clipboard-paste"
  :enablement ~(not (empty? state.explorerEdit.clipboard)) []
  (invoke "files.copy"
          :with {:path state.explorerEdit.clipboard
                 :free true
                 :to (join "/"
                           (filter (fn [it] (not (empty? it)))
                                   [computed.explorerTargetFolder (baseName state.explorerEdit.clipboard)]))}
          :onError explorerPasteFailed
          :onDone explorerPasteDone))

(defn explorerDrop "event: {data: {ids}, id, place}. Moves the files and folders dragged into the folder dropped on (or the one holding the file)"
  []
  (doseq [from (or-else event.data.ids [])]
    (explorerMove from
                  (if (and (= event.place "inside")
                           (some (fn [it] (and (= it.path event.id) (= it.kind "folder"))) state.explorerTree.entries))
                    event.id
                    (join "/" (slice (split (or-else event.id "") "/") 0 (- 1)))))))

(defn explorerMove "TODO" [from into]
  (cond
    (or (= into from) (= (indexOf into (str from "/")) 0))
      (notify "error" (str "Cannot move " (baseName from) " into itself"))
    (not= into (join "/" (slice (split from "/") 0 (- 1))))
      (invoke "files.rename"
              :with {:path from :to (join "/" (filter (fn [it] (not (empty? it))) [into (baseName from)]))}
              :onError explorerMoveFailed
              :onDone [explorerMoveDone from])))

(defcommand explorerCollapseAll "Collapse Folders in Explorer"
  :title ~(t "command.explorerCollapseAll") :category ~(t "category.files") :icon "copy-minus"
  :enablement ~(not (empty? state.explorer.expanded)) []
  (set "state.explorer.expanded" []))

(defcommand explorerReveal "Reveal Active File in Explorer"
  :title ~(t "command.explorerReveal") :category ~(t "category.files") :icon "locate"
  :enablement ~(and (some? computed.activeDoc) (= computed.activeDoc.kind "file")) []
  (set "state.layout.sidebar" true)
  (set "state.sideView" "explorer")
  (explorerShow computed.activeDoc.id))

(defn explorerRefreshEach "What explorerRefresh does when its time comes." [event]
  (explorerChanged event))

(defn explorerListDone "What explorerList does when it is done." [folder result]
  (explorerListed
    folder
    (if (empty? folder) "" (str folder "/"))
    result.entries
    (map (fn [it] it.name) (filter (fn [it] (= it.kind "folder")) result.entries))))

(defn explorerListFailed "What explorerList does when it fails." [folder]
  (explorerListed folder (str folder "/") [] [])
  (set "state.explorerTree.listed" (filter (fn [it] (not= it folder)) state.explorerTree.listed)))

(defn explorerMakeFailed "What explorerMake does when it fails." [error]
  (notify "error" (str "Cannot rename: " error.message)))

(defn explorerMakeDone "What explorerMake does when it is done." [edit to]
  (explorerMoved edit.id to))

(defn explorerMakeDone2 "What explorerMake does when it is done." [edit to result]
  (cond
    result.exists (notify "error" (str "A file or folder " edit.name " already exists at this location"))
    (= edit.mode "newFolder")
      (invoke "files.mkdir" :with {:path to} :onError explorerMakeDone2Failed :onDone [explorerMakeDone2Done to])
    :else
      (invoke "files.write"
              :with {:path to :text ""}
              :onError explorerMakeDone2Failed2
              :onDone [explorerMakeDone2Done2 to])))

(defn explorerMakeDone2Failed "What explorerMakeDone2 does when it fails." [error]
  (notify "error" (str "Cannot create: " error.message)))

(defn explorerMakeDone2Done "What explorerMakeDone2 does when it is done." [to]
  (explorerShow to))

(defn explorerMakeDone2Failed2 "What explorerMakeDone2 does when it fails." [error]
  (notify "error" (str "Cannot create: " error.message)))

(defn explorerMakeDone2Done2 "What explorerMakeDone2 does when it is done." [to]
  (explorerShow to)
  (openPath to (baseName to)))

(defn explorerDeleteConfirmedFailed "What explorerDeleteConfirmed does when it fails." [error]
  (notify "error" (str "Cannot delete: " error.message)))

(defn explorerDeleteConfirmedDone "What explorerDeleteConfirmed does when it is done." []
  (set "state.explorer.selected" nil)
  (explorerList (join "/" (slice (split state.explorerEdit.id "/") 0 (- 1)))))

(defn explorerPasteFailed "What explorerPaste does when it fails." [error]
  (notify "error" (str "Cannot paste: " error.message)))

(defn explorerPasteDone "What explorerPaste does when it is done." [result]
  (explorerShow result.path))

(defn explorerMoveFailed "What explorerMove does when it fails." [error]
  (notify "error" (str "Cannot move: " error.message)))

(defn explorerMoveDone "What explorerMove does when it is done." [from result]
  (explorerMoved from result.path))
