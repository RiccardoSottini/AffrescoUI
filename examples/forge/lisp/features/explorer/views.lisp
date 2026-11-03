;;; views.lisp --- the explorer: its title and buttons, the folder's tree, its menu and its dialogs

;;; The title

(defcomponent forgeExplorerTitle "The explorer's title, and its buttons once a folder is open."
  {}
  `(row#side-bar-title :width "100%" :height 35 :alignItems center :gap 2
     (label#side-bar-title-text :text ~(t "side-bar-title-text") :textColor @color.muted :grow 1 :height 35 :padding 12)
     ~(explorer-action "explorer-new-file" "file-plus" "explorerNewFile")
     ~(explorer-action "explorer-new-folder" "folder-plus" "explorerNewFolder")
     ~(explorer-action "explorer-refresh" "refresh-cw" "explorerRefresh")
     (node#explorer-collapse :when ~(not (empty? state.folder.root))
       :use forgeExplorerAction
       :with {:icon copy-minus
              :tip ~(t "explorer-collapse.tip")
              :run explorerCollapseAll
              :enabled ~(not (empty? state.explorer.expanded))})
     (node#side-bar-title-end :width 6)))

(defview explorer-action "A button of the explorer's title, shown once a folder is open." [id icon run]
  `(node :id ~id
     :when ~(not (empty? state.folder.root))
     :use forgeExplorerAction
     :with {:icon ~icon :tip ~(t (str id ".tip")) :run ~run}))

(defcomponent forgeExplorerAction "A button of an icon that runs a command, `run`; dimmed while it can't."
  {:icon "" :tip "" :run "" :enabled true}
  `(label :role button
     :label ~tip
     :icon ~icon
     :width 26
     :height 26
     :align center
     :radius 6
     :focusable ~enabled
     :disabled ~(not enabled)
     :cursor ~(if enabled "hand" "arrow")
     :tooltip ~tip
     :textColor ~(if enabled "@color.muted" "@color.border")
     :hover {:textColor @color.text :color @color.hover}
     :onClick {:run ~run}))

;;; The tree

(defcomponent forgeExplorer "The folder's tree, its right-click menu, and the name and delete dialogs."
  {}
  `(column#explorer-view :width "100%" :grow 1
     ~(explorer-tree)
     (node#explorer-menu :use contextMenu
       :with {:open state.explorerEdit.menu :items ~menus.explorer :x ~state.explorerEdit.x :y ~state.explorerEdit.y})
     ~(name-dialog)
     ~(delete-dialog)))

(defview explorer-tree "The folder's files and folders: a folder opens on a click, a file in a tab; entries drag." []
  `(tree#explorer :role tree
     :label ~(t "explorer.label")
     :width "100%"
     :grow 1
     :drag {}
     :items ~computed.explorerItems
     :expanded ~state.explorer.expanded
     :bind state.explorer.selected
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onExpand explorerExpand
     :onOpen openExplorerItem
     :onDrop explorerDrop
     :onContextMenu explorerMenu))

;;; The dialogs

(defview name-dialog "Asks for a name: a new file's, a new folder's, or an entry's new one." []
  `(node#explorer-name-dialog :use dialog
     :with {:open state.explorerEdit.naming
            :height 150
            :title ~(cond (= state.explorerEdit.mode "rename") (t "explorer-name-dialog.rename")
                          (= state.explorerEdit.mode "newFolder") (t "explorer-name-dialog.newFolder")
                          :else (t "explorer-name-dialog.newFile"))
            :message ""
            :buttons [{:title ~(t "explorer-name-dialog.ok") :run explorerSubmitName}
                      {:title ~(t "explorer-name-dialog.cancel")}]}
     (textarea#explorer-name :label ~(t "explorer-name.label")
       :bind state.explorerEdit.name
       :autofocus true
       :anchors {:left 20 :right 20 :top 48}
       :height 28
       :padding 7
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "explorer-name.placeholder")
       :onSubmit explorerSubmitName)))

(defview delete-dialog "Asks before an entry is deleted, saying it can't be undone." []
  `(node#explorer-delete-dialog :use dialog
     :with {:open state.explorerEdit.confirm
            :title ~(t "explorer-delete-dialog.title" {:name state.explorerEdit.name})
            :message ~(if (= state.explorerEdit.mode "folder")
                          (t "explorer-delete-dialog.folder")
                          (t "explorer-delete-dialog.file"))
            :buttons [{:title ~(t "explorer-delete-dialog.delete") :run explorerDeleteConfirmed}
                      {:title ~(t "explorer-delete-dialog.cancel")}]}))
