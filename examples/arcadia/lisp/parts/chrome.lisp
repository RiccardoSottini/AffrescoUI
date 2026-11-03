;;; chrome.lisp --- the bars around Arcadia's pages
;;;
;;; The menu row (the menus on the left; TV mode, the notifications and the account with its wallet on the right), the
;;; big navigation with back and forward, and the bottom bar (add a game, the download in progress, friends and chat).

;;; What the bars keep

(defstate menus {})

(defstate accountOpen false)

(defstate bellOpen false)

(defstate addOpen false)

(defstate downloads
  [{:id skyward :doneGb 12.4 :status downloading :kind install}
   {:id ember :doneGb 0 :status queued :sizeGb 1.2 :kind update}
   {:id frostline :doneGb 0 :status queued :sizeGb 0.8 :kind update}
   {:id thornwick :doneGb 0.2 :status paused :sizeGb 2.6 :kind update}
   {:id lumen :doneGb 0.3 :status done :sizeGb 0.3 :kind update}
   {:id quietorbit :doneGb 4.3 :status done :kind install}])

(defstate notifications
  [{:icon download :text notice.update :game "Ember Vigil" :unread true}
   {:icon user-plus :text notice.friend :who "NightDrifter" :unread true}
   {:icon tag :text notice.sale :n 2 :unread false}])

(defcomputed download "The download in progress, if one is." (find (fn [it] (= it.status "downloading")) state.downloads))

(defcomputed downloadGame "The game the download in progress is of." (find (fn [it] (= it.id computed.download.id)) data.games))

(defcomputed unread "How many notifications are unread." (count (filter (fn [it] it.unread) state.notifications)))

;;; The bars' looks

(defstyle navTab "A tab of the big navigation."
  (label :font strong
    :fitText true
    :padding 16
    :height 30
    :radius 15
    :cursor hand
    :focusable true
    :role tab
    :hover {:textColor @color.bright :color @color.raised}))

(defstyle barButton "A quiet button of the menu row and the bottom bar."
  (label :font small
    :fitText true
    :padding 8
    :cursor hand
    :focusable true
    :role button
    :textColor @color.muted
    :radius 2
    :hover {:textColor @color.bright :color @color.hover}))

;;; The menu row

(defcomponent menuRow "The top row: the client's menus, then TV mode, notifications and the account on the right."
  {}
  `(row :height 30 :anchors {:left 0 :right 0} :color @color.top :alignItems center
     ~(menu-bar)
     (node#tv-button :style barButton :icon sofa :text "" :tooltip ~(t "menu.tv") :label ~(t "menu.tv") :onClick enterCouch)
     ~(bell)
     ~(account-button)))

(defview menu-bar "The client's menus, in the order a desktop app has them: Arcadia, View, Friends, Games and Help." []
  `(node#menus :use menuBar
     :grow 1
     :with {:height 30
            :width 230
            :open state.menus
            :textColor @color.muted
            :highlight @color.hover
            :menus [~(app-menu) ~(view-menu) ~(friends-menu) ~(games-menu) ~(help-menu)]}))

(defview app-menu "The Arcadia menu: the account, offline mode, updates, the backup and Settings." []
  `{:name app
    :title ~(t "menu.app")
    :items [{:title ~(t "menu.changeAccount") :run signOut}
            {:title ~(t "menu.offline") :run toggleOffline :checked ~state.session.offline}
            {:separator true}
            {:title ~(t "menu.updates") :run checkUpdates}
            {:title ~(t "menu.backup") :run openBackup}
            {:title ~(t "menu.settings") :run openSettings :with {:page interface} :keyLabel "⌘,"}
            {:separator true}
            {:title ~(t "menu.exit") :enabled false}]})

(defview view-menu "The View menu: couch mode, the pages, the theme and the language." []
  `{:name view
    :title ~(t "menu.view")
    :items [{:title ~(t "menu.tv") :run enterCouch}
            {:separator true}
            {:title ~(t "nav.store") :run showStore :keyLabel "⌘1"}
            {:title ~(t "nav.library") :run showLibrary :keyLabel "⌘2"}
            {:title ~(t "nav.community") :run showCommunity :keyLabel "⌘3"}
            {:title ~(t "menu.downloads") :run showDownloads}
            {:separator true}
            {:title ~(t "menu.theme") :run toggleTheme}
            {:title ~(t "menu.language") :run nextLanguage}]})

(defview presence-item "An item of the Friends menu that sets how the person shows to friends, checked when it is so."
  [status]
  `{:title ~(t (str "presence." status)) :checked ~(= state.user.status status) :run setStatus :with {:status ~status}})

(defview friends-menu "The Friends menu: the list, adding a friend, and how the person shows to friends." []
  `{:name friends
    :title ~(t "menu.friends")
    :items [{:title ~(t "menu.friendsList") :run showCommunity}
            {:title ~(t "menu.addFriend") :run addFriendDialog}
            {:separator true}
            ~@(map presence-item ["online" "away" "invisible" "offline"])]})

(defview games-menu "The Games menu: the library, product keys, gift codes and a game from this computer." []
  `{:name games
    :title ~(t "menu.games")
    :items [{:title ~(t "menu.viewLibrary") :run showLibrary}
            {:title ~(t "menu.activate") :run openCode :with {:kind activate}}
            {:title ~(t "menu.redeem") :run openCode :with {:kind redeem}}
            {:title ~(t "menu.addOther") :run openAddGame}]})

(defview help-menu "The Help menu: support, privacy, the system's details and About." []
  `{:name help
    :title ~(t "menu.help")
    :items [{:title ~(t "menu.support") :run showInfo :with {:kind support}}
            {:title ~(t "menu.privacy") :run showInfo :with {:kind privacy}}
            {:title ~(t "menu.system") :run showInfo :with {:kind system}}
            {:separator true}
            {:title ~(t "menu.about") :run showInfo :with {:kind about}}]})

(defview bell "The bell, lit with the number of unread notifications, and their menu." []
  `(node#bell :style barButton
     :icon bell
     :text ~(if (> computed.unread 0) (text computed.unread) "")
     :color ~(if (> computed.unread 0) "@color.play" "transparent")
     :textColor ~(if (> computed.unread 0) "#ffffff" "@color.muted")
     :label ~(t "notice.title" {:n computed.unread})
     :isOpen ~state.bellOpen
     :onClick {:toggle state.bellOpen}
     (node :use menu
       :with {:open state.bellOpen
              :width 330
              :icons true
              :placement below-end
              :items ~(map-indexed (fn [index it]
                                     {:title (t it.text {:game (or-else it.game "") :who (or-else it.who "") :n (or-else it.n 0)})
                                      :icon it.icon
                                      :run "openNotice"
                                      :with {:i index}})
                                   state.notifications)})))

(defview account-button "The account: the avatar, the name and the wallet, and the account's menu." []
  `(row#account :height 30
     :width 168
     :gap 6
     :alignItems center
     :focusable true
     :role button
     :cursor hand
     :isOpen ~state.accountOpen
     :label ~(t "account.label" {:name state.user.name})
     :hover {:color @color.hover}
     :onClick {:toggle state.accountOpen}
     (panel :width 4 :height 1 :color transparent)
     (image#account-avatar :file ~(str "../assets/art/avatars/" state.user.avatar ".svg")
       :width 22
       :height 22
       :fit cover
       :borderWidth 1
       :borderColor @color.online)
     (label#account-name :font small :text ~state.user.name :textColor @color.online :fitText true)
     (label#account-wallet :font small :text ~(formatCurrency state.user.wallet "EUR") :textColor @color.muted :fitText true)
     (label#account-chevron :font small :icon chevron-down :text "" :textColor @color.muted :fitText true)
     ~(account-menu)))

(defview account-menu "The account's menu: the profile, the account's details, the wallet, the language, signing out." []
  `(node :use menu
     :with {:open state.accountOpen
            :width 240
            :placement below-end
            :items [{:title ~(t "account.profile") :run showProfile}
                    {:title ~(t "account.details") :run openSettings :with {:page account}}
                    {:title ~(t "account.wallet" {:amount (formatCurrency state.user.wallet "EUR")})
                     :run openCode
                     :with {:kind redeem}}
                    {:title ~(t "account.language") :run nextLanguage}
                    {:separator true}
                    {:title ~(t "account.signOut") :run signOut}]}))

;;; The navigation

(defcomponent navRow "The navigation: back and forward, then the pages as one segmented group in the middle."
  {}
  `(row :height 50 :anchors {:left 0 :right 0} :color @color.top :alignItems center
     (panel :width 12 :height 1 :color transparent)
     ~(nav-back)
     ~(nav-forward)
     (panel :grow 1 :height 1 :color transparent)
     ~(nav-tabs)
     (panel :grow 1 :height 1 :color transparent)
     (panel :width 80 :height 1 :color transparent)))

(defview nav-back "Back: an arrow, dim when there is no page before this one." []
  `(label#nav-back :font nav
     :icon chevron-left
     :mirror true
     :text ""
     :fitText true
     :padding 6
     :textColor ~(if (> state.at 0) "@color.text" "@color.dim")
     :focusable true
     :role button
     :label ~(t "nav.back")
     :tooltip ~(t "nav.back")
     :cursor hand
     :onClick back
     :hover {:textColor @color.bright}))

(defview nav-forward "Forward: an arrow, dim when there is no page after this one." []
  `(label#nav-forward :font nav
     :icon chevron-right
     :mirror true
     :text ""
     :fitText true
     :padding 6
     :textColor ~(if (< state.at (- (count state.nav) 1)) "@color.text" "@color.dim")
     :focusable true
     :role button
     :label ~(t "nav.forward")
     :tooltip ~(t "nav.forward")
     :cursor hand
     :onClick forward
     :hover {:textColor @color.bright}))

(defview nav-tabs "The store, the library, friends and the profile: one segmented group, the page shown lit." []
  `(row#nav-tabs :height 38 :fitContent width :gap 2 :radius 19 :alignItems center :color @color.side :arrows horizontal
     :role tablist
     (panel :width 2 :height 1 :color transparent)
     ~(repeat [tab ["store" "library" "community" "profile"]]
        `(node :id ~(str "nav-" tab)
           :style navTab
           :text ~(if (= tab "profile") state.user.name (t (str "nav." tab)))
           :textColor ~(if (= computed.here.tab tab) "@color.onAccent" "@color.text")
           :color ~(if (= computed.here.tab tab) "@color.accent" "transparent")
           :hover {:textColor ~(if (= computed.here.tab tab) "@color.onAccent" "@color.bright")
                   :color ~(if (= computed.here.tab tab) "@color.accent" "@color.raised")}
           :selected ~(= computed.here.tab tab)
           :onClick ~(handler go tab "home" nil)))
     (panel :width 2 :height 1 :color transparent)))

;;; The bottom bar

(defcomponent bottomBar
  "The bottom bar: add a game, the download in progress (or a link to the downloads) and friends and chat."
  {}
  `(row :height 40
     :anchors {:left 0 :right 0}
     :alignItems center
     :gradient {:kind linear :stops [@color.sideTop @color.top]}
     :borderWidth 0
     (panel :width 8 :height 1 :color transparent)
     ~(add-game-button)
     (panel :grow 1 :height 1 :color transparent)
     ~(downloads-status)
     (panel :grow 1 :height 1 :color transparent)
     (node#friends-chat :style barButton
       :font default
       :icon message-square
       :text ~(t "bottom.friends")
       :onClick ~(handler showCommunity))
     (panel :width 8 :height 1 :color transparent)))

(defview add-game-button "Add a game, and its menu: a game from this computer, a product key, or the store." []
  `(node#add-game :style barButton
     :font default
     :icon package-plus
     :text ~(t "bottom.add")
     :isOpen ~state.addOpen
     :onClick {:toggle state.addOpen}
     (node :use menu
       :with {:open state.addOpen
              :width 260
              :placement above
              :items [{:title ~(t "menu.addOther") :run openAddGame}
                      {:title ~(t "menu.activate") :run openCode :with {:kind activate}}
                      {:title ~(t "bottom.browse") :run showStore}]})))

(defmacro download-line
  ;; A macro: the line is an expression, shown and read aloud.
  "What the bottom bar says of the downloads: the game in progress and how far it is, or a link to them."
  []
  `(if computed.download
     (t "bottom.downloading"
        {:title computed.downloadGame.title
         :percent (floor (* (/ computed.download.doneGb computed.downloadGame.sizeGb) 100))})
     (t "bottom.manage")))

(defview downloads-status "The download in progress, with its bar, in the middle of the bottom bar: it opens the downloads." []
  `(row#downloads-status :width 340
     :height 34
     :gap 10
     :alignItems center
     :focusable true
     :role button
     :cursor hand
     :radius 2
     :hover {:color @color.hover}
     :onClick showDownloads
     :label ~(download-line)
     (label :icon download :text "" :textColor ~(if computed.download "@color.accent" "@color.muted") :fitText true :padding 6)
     (column :grow 1 :height 30 :gap 5
       (label#downloads-text :font small
         :anchors {:left 0 :right 0}
         :height 15
         :textColor @color.text
         :text ~(download-line))
       ~(download-track))))

(defview download-track "The download's progress, a bar that fills from the left." []
  `(node#downloads-track :when ~(some? computed.download) :anchors {:left 0 :right 0} :height 4 :color @color.raised :radius 2
     (node#downloads-fill :anchors {:left 0 :top 0 :bottom 0}
       :radius 2
       :width ~(str (text (floor (/ (* 100 computed.download.doneGb) computed.downloadGame.sizeGb))) "%")
       :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]})))

;;; A page not made yet

(defcomponent comingSoon "A page of the navigation Arcadia has no page for yet: its name, dimmed." {}
  `(node :fill true :color @color.page
     (label :font heading :text ~(t (str "nav." computed.here.tab)) :textColor @color.muted :align center :fill true)))
