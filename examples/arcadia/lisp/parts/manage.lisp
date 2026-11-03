;;; manage.lisp --- managing a game: its menu, its properties, and uninstalling it
;;;
;;; A game's menu opens from the library (right click, Shift+F10) or the game page's gear. Its properties (launch
;;; options, the overlay, cloud saves, updates, installed files) are kept on the game's record; uninstalling asks first.

;;; What it keeps

(defstate manage nil)

(defstate gameMenu {:open false :x 0 :y 0})

(defstate properties {:open false :tab general :verify ""})

(defstate uninstall {:open false})

(defcomputed manageGame "The game being managed." (find (fn [it] (= it.id state.manage)) data.games))

;;; The game's menu

(defcomputed gameMenuItems "The game menu's items, for the game it is open on: what it can do now first."
  (if (nil? computed.manageGame)
    []
    [{:title (t (cond (= computed.manageGame.status "ready") "menu.game.play"
                      (= computed.manageGame.status "running") "menu.game.stop"
                      (= computed.manageGame.status "update") "menu.game.update"
                      (= computed.manageGame.status "downloading") "menu.game.downloading"
                      :else "menu.game.install"))
      :enabled (not= computed.manageGame.status "downloading")
      :run "primary"
      :with {:game computed.manageGame}}
     {:separator true}
     {:title (t (if computed.manageGame.favourite "menu.game.unfavourite" "menu.game.favourite"))
      :run "toggleFavourite"
      :with {:id computed.manageGame.id}}
     {:title (t "menu.game.page") :run "openGame" :with {:id computed.manageGame.id}}
     {:title (t "menu.game.properties") :run "openProperties" :with {:id computed.manageGame.id :tab "general"}}
     {:separator true}
     {:title (t "menu.game.uninstall")
      :enabled (and computed.manageGame.installed (not= computed.manageGame.status "running"))
      :run "askUninstall"
      :with {:id computed.manageGame.id}}]))

(defcomponent gameMenu
  "A game's menu (right click, Shift+F10 or the gear): play or install, favourite, properties, uninstall."
  {}
  `(node#game-menu :use contextMenu
     :with {:open state.gameMenu.open
            :width 230
            :x ~(min state.gameMenu.x (- window.width 238))
            :y ~(min state.gameMenu.y (- window.height 190))
            :color @color.raised
            :textColor @color.text
            :dimColor @color.dim
            :highlight @color.hover
            :items ~computed.gameMenuItems}))

;;; Properties

(defcomponent propertiesDialog
  "A game's properties: launch options and in-game settings, updates, installed files, extra content."
  {}
  `(node :use dialog
     :with {:open state.properties.open
            :width 640
            :height 400
            :title ~(t "properties.title" {:game (or-else computed.manageGame.title "")})
            :color @color.panel
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :buttons [{:title ~(t "dialog.close") :default true}]}
     (row#properties-body :anchors {:left 20 :right 20 :top 52 :bottom 60} :gap 20
       ~(properties-tabs)
       (panel :width 1 :alignSelf stretch :color @color.border)
       (node#properties-page :grow 1 :alignSelf stretch :color transparent
         ~(general-page)
         ~(updates-page)
         ~(files-page)
         ~(extras-page)))))

(defview properties-tabs "The properties' pages down the left side, the one shown lit." []
  `(column#properties-tabs :width 150 :alignSelf stretch :gap 2 :role tablist :arrows vertical
     ~(repeat [tab ["general" "updates" "files" "dlc"]]
        `(label :id ~(str "properties-tab-" tab)
           :anchors {:left 0 :right 0}
           :height 30
           :padding 10
           :radius 3
           :role tab
           :focusable true
           :cursor hand
           :selected ~(= state.properties.tab tab)
           :text ~(t (str "properties.tab." tab))
           :color ~(if (= state.properties.tab tab) "@color.select" "transparent")
           :textColor ~(if (= state.properties.tab tab) "@color.bright" "@color.muted")
           :hover {:textColor @color.bright}
           :onClick ~(handler showPropertiesTab tab)))))

(defview caps-title "A section's title in caps, at a height of the page." [key top]
  `(label :font caps
     :text ~(upper-case (t key))
     :textColor @color.muted
     :anchors {:left 0 :right 0 :top ~top}
     :height 18))

(defview general-page "The general page: the launch options, and the overlay and cloud saves." []
  `(node :when ~(= state.properties.tab "general") :fill true :color transparent
     ~(caps-title "properties.launch" 0)
     (textarea#properties-launch :anchors {:left 0 :right 0 :top 24}
       :height 32
       :padding 8
       :radius 3
       :borderWidth 1
       :borderColor @color.border
       :focused {:borderColor @color.accent}
       :color @color.field
       :textColor @color.text
       :placeholderColor @color.dim
       :font small
       :maxLength 200
       :placeholder ~(t "properties.launchHint")
       :label ~(t "properties.launch")
       :text ~(or-else computed.manageGame.launchOptions "")
       :onChange ~(handler setLaunchOptions event.value))
     (label :font small
       :text ~(t "properties.launchNote")
       :textColor @color.dim
       :anchors {:left 0 :right 0 :top 62}
       :height 18)
     ~(caps-title "properties.inGame" 100)
     (row#properties-overlay :anchors {:left 0 :right 0 :top 124} :height 30 :alignItems center
       (label :grow 1 :height 22 :text ~(t "properties.overlay") :textColor @color.text)
       (toggle#properties-overlay-toggle :width 40
         :height 22
         :label ~(t "properties.overlay")
         :checked ~(not= computed.manageGame.overlay false)
         :activeColor @color.accent
         :onChange ~(handler setOverlay event.value)))
     (row#properties-cloud :anchors {:left 0 :right 0 :top 156} :height 30 :alignItems center
       (label :grow 1 :height 22 :text ~(t "properties.cloud") :textColor @color.text)
       (toggle#properties-cloud-toggle :width 40
         :height 22
         :label ~(t "properties.cloud")
         :checked ~(not= computed.manageGame.cloud false)
         :activeColor @color.accent
         :onChange ~(handler setCloud event.value)))))

(defview updates-page "The updates page: when the game updates, one of three." []
  `(node :when ~(= state.properties.tab "updates") :fill true :color transparent
     ~(caps-title "properties.automatic" 0)
     ~(update-choice "auto" 24)
     ~(update-choice "launch" 56)
     ~(update-choice "high" 88)
     (label :font small
       :text ~(t "properties.updatesNote")
       :textColor @color.dim
       :wrap true
       :anchors {:left 0 :right 0 :top 128}
       :height 40)))

(defview update-choice "One way to update, a radio button: always, only on launch, or first in the queue." [way top]
  `(row :id ~(str "properties-updates-" way)
     :anchors {:left 0 :right 0 :top ~top}
     :height 28
     :gap 10
     :alignItems center
     :focusable true
     :role radio
     :selected ~(= (or-else computed.manageGame.updates "auto") way)
     :cursor hand
     :label ~(t (str "properties.updates." way))
     :radius 3
     :hover {:color @color.hover}
     :onClick ~(handler setUpdates way)
     (panel :color transparent :width 2 :height 1)
     (panel :width 14
       :height 14
       :radius 7
       :borderWidth 2
       :borderColor ~(if (= (or-else computed.manageGame.updates "auto") way) "@color.accent" "@color.dim")
       :color transparent
       (node :when ~(= (or-else computed.manageGame.updates "auto") way)
         :anchors {:left 3 :top 3}
         :width 4
         :height 4
         :radius 2
         :color @color.accent))
     (label :grow 1 :height 22 :text ~(t (str "properties.updates." way)) :textColor @color.text)))

(defview files-page "The installed files page: the size, the folder, and checking the files." []
  `(node :when ~(= state.properties.tab "files") :fill true :color transparent
     ~(caps-title "properties.size" 0)
     (label#properties-size :text ~(t "properties.sizeValue"
                                      {:size (formatNumber (or-else computed.manageGame.sizeGb 0))})
       :textColor @color.text
       :font default
       :anchors {:left 0 :right 0 :top 24}
       :height 22)
     ~(caps-title "properties.folder" 60)
     (label#properties-folder :text ~(t "properties.folderPath" {:game computed.manageGame.title})
       :textColor @color.text
       :font small
       :anchors {:left 0 :right 0 :top 84}
       :height 22)
     (node#properties-verify :use quietButton
       :anchors {:left 0 :top 128}
       :height 32
       :with {:title ~(t "properties.verify") :icon shield-check}
       :disabled ~(= state.properties.verify "running")
       :onClick verifyFiles)
     (label#properties-verify-result :live polite
       :font small
       :anchors {:left 0 :right 0 :top 168}
       :height 20
       :text ~(cond (= state.properties.verify "running") (t "properties.verifying")
                    (= state.properties.verify "done") (t "properties.verified")
                    :else "")
       :textColor ~(if (= state.properties.verify "done") "@color.play" "@color.muted"))))

(defview extras-page "The extra content page: none yet." []
  `(node :when ~(= state.properties.tab "dlc") :fill true :color transparent
     ~(caps-title "properties.dlcTitle" 0)
     (label#properties-dlc-empty :text ~(t "properties.dlcNone")
       :textColor @color.muted
       :anchors {:left 0 :right 0 :top 24}
       :height 22)))

;;; Uninstalling

(defcomponent uninstallDialog "The question before a game's files are removed; the game stays in the library."
  {}
  `(node :use dialog
     :with {:open state.uninstall.open
            :width 440
            :height 170
            :title ~(t "uninstall.title" {:game (or-else computed.manageGame.title "")})
            :message ~(t "uninstall.message"
                         {:game (or-else computed.manageGame.title "")
                          :size (formatNumber (or-else computed.manageGame.sizeGb 0))})
            :color @color.panel
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :buttons [{:title ~(t "uninstall.confirm") :run uninstall :with {:id ~state.manage} :default true}
                      {:title ~(t "dialog.cancel")}]}))
