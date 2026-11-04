;;; dialogs.lisp --- what is drawn over the pages: the status bar, the song's and the playlist's menus, the dialog that
;;; deletes a playlist, and the quick pick (Cmd+P finds a song, Cmd+Shift+P runs a command)

;;; The status bar

(defview status-bar "The status bar: the revision shown, and the window's size." []
  `(node#statusBar :when ~(not state.mini) :anchors {:left 0 :right 0 :bottom 0} :height 32 :color @sidebar
     (node#panel-status-bar :anchors {:left 0 :right 0 :top 0} :height 1 :color @border)
     (label#statusText
       :text ~(t "status.revision" {:number revision.number :author revision.author :message revision.message})
       :font small
       :textColor @muted
       :anchors {:left 24 :top 0 :bottom 0 :right 200}
       :align {:horizontal start :vertical center})
     (label#statusSize :text ~(str window.width " × " window.height)
       :font small
       :textColor @muted
       :anchors {:right 24 :top 0 :bottom 0}
       :width 160
       :align {:horizontal end :vertical center})))

;;; Over the pages

(defview song-menu "A song's menu, where it was opened." []
  `(node#songMenu :use contextMenu
     :with {:open state.songMenu.open
            :items ~computed.songMenuItems
            :x ~state.songMenu.x
            :y ~state.songMenu.y
            :width 240
            :color @menu
            :textColor @text
            :dimColor @faint
            :highlight @accentSoft}))

(defview list-menu "The playlist's menu: rename, delete." []
  `(node#listMenu :use contextMenu
     :with {:open state.listMenu.open :items ~computed.listMenuItems :x ~state.listMenu.x :y ~state.listMenu.y :width 180}))

(defview delete-dialog "Asks before a playlist is deleted." []
  `(node#deleteDialog :use dialog
     :with {:open state.deleting
            :title ~(t "playlist.deleteTitle")
            :message ~(t "playlist.deleteMessage" {:name (or-else computed.list.name "")})
            :primaryLast true
            :buttons [{:title ~(t "dialog.cancel")}
                      {:title ~(t "playlist.delete") :default true :run deleteList :with {:list ~state.current}}]}))

(defview quick-pick "The quick pick: songs by default, commands after >." []
  `(node#quickPick :when ~state.quickPick.open
     :use quickPick
     :with {:color @menu
            :textColor @text
            :dimColor @muted
            :accent @accent
            :highlight @accentSoft
            :field @field
            :providers [{:items ~computed.picks :run playFromPick :placeholder ~(t "pick.songs")}
                        {:prefix ">" :items ~computed.palette :placeholder ~(t "pick.commands")}]}))
