;;; views.lisp --- what explorer.lisp shows

;;; Views

(defcomponent forgeExplorerAction "The forgeExplorerAction component."
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

(defcomponent forgeExplorerTitle "The forgeExplorerTitle component."
  {}
  `(row#side-bar-title :width "100%" :height 35 :alignItems center :gap 2
     (label#side-bar-title-text :text
                                ~(t "side-bar-title-text")
                                :textColor
                                @color.muted
                                :grow
                                1
                                :height
                                35
                                :padding
                                12)
     (node#explorer-new-file :when ~(not (empty? state.folder.root))
       :use forgeExplorerAction
       :with {:icon file-plus :tip ~(t "explorer-new-file.tip") :run explorerNewFile})
     (node#explorer-new-folder :when ~(not (empty? state.folder.root))
       :use forgeExplorerAction
       :with {:icon folder-plus :tip ~(t "explorer-new-folder.tip") :run explorerNewFolder})
     (node#explorer-refresh :when ~(not (empty? state.folder.root))
       :use forgeExplorerAction
       :with {:icon refresh-cw :tip ~(t "explorer-refresh.tip") :run explorerRefresh})
     (node#explorer-collapse :when ~(not (empty? state.folder.root))
       :use forgeExplorerAction
       :with {:icon copy-minus
              :tip ~(t "explorer-collapse.tip")
              :run explorerCollapseAll
              :enabled ~(not (empty? state.explorer.expanded))})
     {:id side-bar-title-end :width 6}))

(defcomponent forgeExplorer "The forgeExplorer component."
  {}
  `(column#explorer-view :width "100%" :grow 1
     ~(explorer-view)
     (node#explorer-menu :use contextMenu
       :with {:open state.explorerEdit.menu :items ~menus.explorer :x ~state.explorerEdit.x :y ~state.explorerEdit.y})
     ~(explorer-name-dialog-view)
     (node#explorer-delete-dialog :use dialog
       :with {:open state.explorerEdit.confirm
              :title ~(str "Delete " state.explorerEdit.name "?")
              :message ~(if (= state.explorerEdit.mode "folder")
                            "The folder and everything in it will be deleted. This cannot be undone."
                            "The file will be deleted. This cannot be undone.")
              :buttons [{:title ~(t "explorer-delete-dialog.delete") :run explorerDeleteConfirmed}
                        {:title ~(t "explorer-delete-dialog.cancel")}]})))

(defview explorer-view "#explorer." []
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
     :onExpand {:run explorerExpand}
     :onOpen {:run openExplorerItem}
     :onDrop {:run explorerDrop}
     :onContextMenu [{:set state.explorerEdit.x :to ~event.x}
                     {:set state.explorerEdit.y :to ~event.y}
                     {:set state.explorerEdit.menu :to true}]))

(defview explorer-name-dialog-view "#explorer-name-dialog." []
  `(node#explorer-name-dialog :use dialog
     :with {:open state.explorerEdit.naming
            :height 150
            :title ~(cond (= state.explorerEdit.mode "rename") "Rename"
                          (= state.explorerEdit.mode "newFolder") "New Folder"
                          :else "New File")
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
       :onSubmit {:run explorerSubmitName})))
