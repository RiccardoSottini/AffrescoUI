;;; index.screen.lisp --- TODO

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "models.lisp"
         "state.lisp"
         "computed.lisp"
         "look.lisp"
         "keys.lisp"
         "sidebar.lisp"
         "library.lisp"
         "queue.lisp"
         "player.lisp"
         "pages.lisp"
         "nowplaying.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "tempo.lisp")

(defscreen main
  :window {:title "Tempo: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
  :strings {:default en :locale ~settings.ui.language}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [iconsSmall]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 17 :fallback [icons]}
          :large {:file "../../shared/fonts/Roboto-Regular.ttf" :size 32}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 44}
          :brand {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 36}}
  :persist [state.theme state.settings state.eq state.eqOn state.volume state.shuffle state.repeat state.liked]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#statusBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~state.theme
  (node#panel-root :color @background :every {:ms 1000 :do tick}
    ~(sidebar-view)
    ~(main-view)
    ~(player-view)
    (node#statusBar :when ~(not state.mini) :anchors {:left 0 :right 0 :bottom 0} :height 32 :color @sidebar
      {:id panel-status-bar :anchors {:left 0 :right 0 :top 0} :height 1 :color @border}
      (label#statusText :text ~(str "Revision " revision.number "  ·  " revision.author ": " revision.message)
        :font small
        :textColor @muted
        :anchors {:left 24 :top 0 :bottom 0 :right 200}
        :align {:horizontal start :vertical center})
      (label#statusSize :text ~(str window.width " × " window.height)
        :font small
        :textColor @muted
        :anchors {:right 24 :top 0 :bottom 0}
        :width 160
        :align {:horizontal end :vertical center}))
    (node#miniView :when ~state.mini :use miniPlayer :fill true)
    (node#songMenu :use contextMenu
      :with {:open state.songMenu.open
             :items ~computed.songMenuItems
             :x ~state.songMenu.x
             :y ~state.songMenu.y
             :width 240
             :color @menu
             :textColor @text
             :dimColor @faint
             :highlight @accentSoft})
    (node#listMenu :use contextMenu
      :with {:open state.listMenu.open :items ~computed.listMenuItems :x ~state.listMenu.x :y ~state.listMenu.y :width 180})
    (node#deleteDialog :use dialog
      :with {:open state.deleting
             :title ~(t "playlist.deleteTitle")
             :message ~(t "playlist.deleteMessage" {:name (or-else computed.list.name "")})
             :primaryLast true
             :buttons [{:title ~(t "dialog.cancel")}
                       {:title ~(t "playlist.delete") :default true :run deleteList :with {:list ~state.current}}]})
    (node#quickPick :when ~state.quickPick.open
      :use quickPick
      :with {:color @menu
             :textColor @text
             :dimColor @muted
             :accent @accent
             :highlight @accentSoft
             :field @field
             :providers [{:items ~computed.picks :run playFromPick :placeholder ~(t "pick.songs")}
                         {:prefix ">" :items ~computed.palette :placeholder ~(t "pick.commands")}]})))
