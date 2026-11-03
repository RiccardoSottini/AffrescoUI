;;; chrome.lisp --- The bars around Arcadia's pages: the menu row (menus on the left, TV mode, notifications and the account with its wallet on the right), the big navigation with back and forward, and the bottom bar (add a game, the download in progress, friends and chat).

;;; Definitions

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

(defcomputed download (find (fn [it] (= it.status "downloading")) state.downloads))

(defcomputed downloadGame (find (fn [it] (= it.id computed.download.id)) data.games))

(defcomputed unread (count (filter (fn [it] it.unread) state.notifications)))

(defstyle navTab
  (label :font strong
    :fitText true
    :padding 16
    :height 30
    :radius 15
    :cursor hand
    :focusable true
    :role tab
    :hover {:textColor @color.bright :color @color.raised}))

(defstyle barButton
  (label :font small
    :fitText true
    :padding 8
    :cursor hand
    :focusable true
    :role button
    :textColor @color.muted
    :radius 2
    :hover {:textColor @color.bright :color @color.hover}))

(defcomponent menuRow "The top row: the client's menus, then TV mode, notifications and the account on the right."
  {}
  `(row :height 30 :anchors {:left 0 :right 0} :color @color.top :alignItems center
     ~(menus)
     (node#tv-button :style barButton
       :icon sofa
       :text ""
       :tooltip ~(t "menu.tv")
       :label ~(t "menu.tv")
       :onClick enterCouch)
     ~(bell)
     ~(account-view)))

(defcomponent navRow
  "The navigation: back and forward on the left, then discover, my games, friends and the profile as one segmented group in the middle, the current one lit."
  {}
  `(row :height 50 :anchors {:left 0 :right 0} :color @color.top :alignItems center
     (panel :width 12 :height 1 :color transparent)
     (label#nav-back :font nav
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
       :hover {:textColor @color.bright})
     (label#nav-forward :font nav
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
       :hover {:textColor @color.bright})
     (panel :grow 1 :height 1 :color transparent)
     ~(nav-tabs)
     (panel :grow 1 :height 1 :color transparent)
     (panel :width 80 :height 1 :color transparent)))

(defcomponent bottomBar
  "The bottom bar: add a game, the download in progress (or a link to the downloads) and friends and chat."
  {}
  `(row :height 40
     :anchors {:left 0 :right 0}
     :alignItems center
     :gradient {:kind linear :stops [@color.sideTop @color.top]}
     :borderWidth 0
     (panel :width 8 :height 1 :color transparent)
     (node#add-game :style barButton
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
                        {:title ~(t "bottom.browse") :run showStore}]}))
     (panel :grow 1 :height 1 :color transparent)
     ~(downloads-status)
     (panel :grow 1 :height 1 :color transparent)
     (node#friends-chat :style barButton
       :font default
       :icon message-square
       :text ~(t "bottom.friends")
       :onClick ~(handler showCommunity))
     (panel :width 8 :height 1 :color transparent)))

(defcomponent comingSoon "The comingSoon component."
  {}
  `(node :fill true :color @color.page
     (label :font heading :text ~(t (str "nav." computed.here.tab)) :textColor @color.muted :align center :fill true)))

;;; Views

(defview menus "#menus." []
  `(node#menus :use menuBar
     :grow 1
     :with {:height 30
            :width 230
            :open state.menus
            :textColor @color.muted
            :highlight @color.hover
            :menus [{:name app
                     :title ~(t "menu.app")
                     :items [{:title ~(t "menu.changeAccount") :run signOut}
                             {:title ~(t "menu.offline") :run toggleOffline :checked ~state.session.offline}
                             {:separator true}
                             {:title ~(t "menu.updates") :run checkUpdates}
                             {:title ~(t "menu.backup") :run openBackup}
                             {:title ~(t "menu.settings") :run openSettings :with {:page interface} :keyLabel "⌘,"}
                             {:separator true}
                             {:title ~(t "menu.exit") :enabled false}]}
                    {:name view
                     :title ~(t "menu.view")
                     :items [{:title ~(t "menu.tv") :run enterCouch}
                             {:separator true}
                             {:title ~(t "nav.store") :run showStore :keyLabel "⌘1"}
                             {:title ~(t "nav.library") :run showLibrary :keyLabel "⌘2"}
                             {:title ~(t "nav.community") :run showCommunity :keyLabel "⌘3"}
                             {:title ~(t "menu.downloads") :run showDownloads}
                             {:separator true}
                             {:title ~(t "menu.theme") :run toggleTheme}
                             {:title ~(t "menu.language") :run nextLanguage}]}
                    {:name friends
                     :title ~(t "menu.friends")
                     :items [{:title ~(t "menu.friendsList") :run showCommunity}
                             {:title ~(t "menu.addFriend") :run addFriendDialog}
                             {:separator true}
                             {:title ~(t "presence.online")
                              :checked ~(= state.user.status "online")
                              :run setStatus
                              :with {:status online}}
                             {:title ~(t "presence.away")
                              :checked ~(= state.user.status "away")
                              :run setStatus
                              :with {:status away}}
                             {:title ~(t "presence.invisible")
                              :checked ~(= state.user.status "invisible")
                              :run setStatus
                              :with {:status invisible}}
                             {:title ~(t "presence.offline")
                              :checked ~(= state.user.status "offline")
                              :run setStatus
                              :with {:status offline}}]}
                    {:name games
                     :title ~(t "menu.games")
                     :items [{:title ~(t "menu.viewLibrary") :run showLibrary}
                             {:title ~(t "menu.activate") :run openCode :with {:kind activate}}
                             {:title ~(t "menu.redeem") :run openCode :with {:kind redeem}}
                             {:title ~(t "menu.addOther") :run openAddGame}]}
                    {:name help
                     :title ~(t "menu.help")
                     :items [{:title ~(t "menu.support") :run showInfo :with {:kind support}}
                             {:title ~(t "menu.privacy") :run showInfo :with {:kind privacy}}
                             {:title ~(t "menu.system") :run showInfo :with {:kind system}}
                             {:separator true}
                             {:title ~(t "menu.about") :run showInfo :with {:kind about}}]}]}))

(defview bell "#bell." []
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
                                     {:title (t it.text
                                                {:game (or-else it.game "")
                                                 :who (or-else it.who "")
                                                 :n (or-else it.n 0)})
                                      :icon it.icon
                                      :run "openNotice"
                                      :with {:i index}})
                                   state.notifications)})))

(defview account-view "#account." []
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
     ~(account-wallet)
     (label#account-chevron :font small :icon chevron-down :text "" :textColor @color.muted :fitText true)
     (node :use menu
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
                      {:title ~(t "account.signOut") :run signOut}]})))

(defview account-wallet "#account-wallet, in account-view." []
  `(label#account-wallet :font small
     :text ~(formatCurrency state.user.wallet "EUR")
     :textColor @color.muted
     :fitText true))

(defview nav-tabs "#nav-tabs." []
  `(row#nav-tabs :height 38
     :fitContent width
     :gap 2
     :radius 19
     :alignItems center
     :color @color.side
     :arrows horizontal
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

(defview downloads-status-column "#downloads-status-column." []
  `(column :grow 1 :height 30 :gap 5
     (label#downloads-text :font small
       :anchors {:left 0 :right 0}
       :height 15
       :textColor @color.text
       :text ~(if computed.download
                (t "bottom.downloading"
                   {:title computed.downloadGame.title
                    :percent (floor (* (/ computed.download.doneGb computed.downloadGame.sizeGb) 100))})
                (t "bottom.manage")))
     (node#downloads-track :when ~(some? computed.download)
       :anchors {:left 0 :right 0}
       :height 4
       :color @color.raised
       :radius 2
       (node#downloads-fill :anchors {:left 0 :top 0 :bottom 0}
         :radius 2
         :width ~(str (text (floor (/ (* 100 computed.download.doneGb) computed.downloadGame.sizeGb))) "%")
         :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}))))

(defview downloads-status "#downloads-status." []
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
     :label ~(if computed.download
               (t "bottom.downloading"
                  {:title computed.downloadGame.title
                   :percent (floor (* (/ computed.download.doneGb computed.downloadGame.sizeGb) 100))})
               (t "bottom.manage"))
     (label :icon download
       :text ""
       :textColor ~(if computed.download "@color.accent" "@color.muted")
       :fitText true
       :padding 6)
     ~(downloads-status-column)))
