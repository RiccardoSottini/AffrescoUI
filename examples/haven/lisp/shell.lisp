;;; shell.lisp --- Haven's shell: the sidebar, the pages' frame, the status bar, and what opens over the pages
;;;
;;; The sidebar lists the pages (open, or folded to icons), then Settings, the notifications' bell and the person
;;; signed in. Each page fills the rest of the window above the status bar. Over them: the dialogs, each over a scrim,
;;; and the notes in the corner. The small parts every page uses (a menu, a level bar, a note, a tab) are here too.

;;; What the shell keeps

(defstate sidebarCollapsed false "Whether the sidebar is folded to its icons.")

(defcomputed sideWidth "The sidebar's width, open or folded." (if state.sidebarCollapsed 56 256))

(defcomputed navPages "The pages the sidebar lists above its foot, where Settings is."
  (filter (fn [it] (not= it.id "settings")) state.pages))

;;; The sidebar

(defview sidebar "The sidebar: the menu button and the name, the pages, and its foot." []
  `(node#sidebar :anchors {:left 0 :top 0 :bottom 0} :width ~computed.sideWidth :color @sidebar :role navigation
     (node#sidebarHeader :anchors {:left 0 :right 0 :top 0} :height 56 :color transparent
       ~(fold-button)
       (label#logo :when ~(not state.sidebarCollapsed)
         :text "Haven"
         :font heading
         :textColor @text
         :anchors {:left 64 :top 0 :bottom 0}
         :fitText true
         :align {:horizontal start :vertical center}))
     {:id sidebarRule :anchors {:left 0 :right 0 :top 56} :height 1 :color @divider}
     (stack#nav :anchors {:left 8 :right 8 :top 64} :height 344 :gap 4 :role tablist :arrows vertical
       ~(repeat [p computed.navPages]
         `(node#navItem :use sideItem
            :with {:icon ~p.icon :title ~(t (str "page." p.id)) :selected ~(= p.id state.page) :keyLabel ~p.key}
            :role tab
            :onClick ~(handler goPage p.id))))
     ~(sidebar-foot)
     {:id sidebarEdge :anchors {:right 0 :top 0 :bottom 0} :width 1 :color @divider}))

(defview fold-button "The button that folds the sidebar to its icons, or opens it." []
  `(label#menuButton :font iconsMedium
     :icon menu
     :text ""
     :textColor @muted
     :anchors {:left 8 :top 8}
     :width 40
     :height 40
     :radius 20
     :align center
     :hover {:color @cardHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t (if state.sidebarCollapsed "sidebar.open" "sidebar.fold"))
     :tooltip ~(t (if state.sidebarCollapsed "sidebar.open" "sidebar.fold"))
     :onClick toggleSidebar))

(defview sidebar-foot "The sidebar's foot: Settings, the notifications' bell with its count, and the person." []
  `(stack#sidebarFoot :anchors {:left 8 :right 8 :bottom 8} :height 148 :gap 4
     (node#settingsItem :use sideItem
       :role tab
       :with {:icon settings
              :title ~(t "page.settings")
              :selected ~(and (= state.page "settings") (not= state.settingsView "profile"))
              :keyLabel "Cmd+,"}
       :onClick goSettings)
     (node#bellButton :use sideItem
       :role button
       :isOpen ~state.notificationsOpen
       :with {:icon bell :title ~(t "notify.title") :badge ~(count state.notifications)}
       :label ~(t "notify.count" {:n (count state.notifications)})
       :onClick toggleNotifications)
     {:id footRule :anchors {:left -8 :right -8} :height 1 :color @divider}
     ~(user-item)))

(defview user-item "The person signed in: their initial and name; it opens their profile." []
  `(node#userItem :anchors {:left 0 :right 0}
     :height 48
     :radius 24
     :color ~(if computed.onProfile "@navSelected" "@sidebar")
     :hover {:color ~(if computed.onProfile "@navSelected" "@raised")}
     :selected ~computed.onProfile
     :cursor hand
     :focusable true
     :role button
     :label ~(t "user.profile")
     :tooltip ~(t "user.profile")
     :onClick [{:set state.settingsView :to profile} {:set state.page :to settings}]
     (label#userAvatar :font small
       :text "A"
       :textColor @onAccent
       :color @primary
       :anchors {:left 4 :top 8}
       :width 32
       :height 32
       :radius 16
       :align center)
     (label#userName :when ~(not state.sidebarCollapsed)
       :text "Alex"
       :textColor ~(if computed.onProfile "@onNavSelected" "@text")
       :anchors {:left 52 :top 0 :bottom 0}
       :fitText true
       :align {:horizontal start :vertical center})))

;; The page shown is a filled pill.
(defcomponent sideItem "A sidebar entry: its icon, its title while the sidebar is open, and a count of what waits."
  {:icon "" :title "" :selected false :keyLabel "" :badge 0}
  `(node :anchors {:left 0 :right 0}
     :height 40
     :radius 20
     :color ~(if selected "@navSelected" "@sidebar")
     :hover {:color ~(if selected "@navSelected" "@raised")}
     :cursor hand
     :focusable true
     :selected ~selected
     :label ~title
     :tooltip ~(if state.sidebarCollapsed (+ title (if keyLabel (str " (" keyLabel ")") "")) keyLabel)
     (label :font iconsMedium
       :icon ~icon
       :text ""
       :textColor ~(if selected "@onNavSelected" "@muted")
       :anchors {:left 8 :top 8}
       :width 24
       :height 24
       :align center)
     (label :when ~(not state.sidebarCollapsed)
       :text ~title
       :textColor ~(if selected "@onNavSelected" "@text")
       :anchors {:left 48 :right 40 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (label :when ~(> badge 0)
       :font small
       :text ~(min badge 9)
       :textColor @onAccent
       :color @accent
       :anchors {:left ~(if state.sidebarCollapsed 22 196) :top ~(if state.sidebarCollapsed 2 10)}
       :width 20
       :height 20
       :radius 10
       :align center)))

;;; The pages and the status bar

(defview page "A page of the app, beside the sidebar and above the status bar, while it is the one shown."
  [id page use dir]
  `(node :id ~id :dir ~dir :when ~(= state.page page) :use ~use :anchors {:left ~computed.sideWidth :right 0 :top 0
                                                                          :bottom 32}))

(defview status-bar "The status bar: the revision shown and who made it, and the window's size." []
  `(node#statusBar :dir auto :anchors {:left ~computed.sideWidth :right 0 :bottom 0} :height 32 :color @sidebar
     {:id panel-status-bar :anchors {:left 0 :right 0 :top 0} :height 1 :color @border}
     (label#statusText :text ~(t "status.revision" {:number revision.number :author revision.author
                                                    :message revision.message})
       :font small
       :textColor @muted
       :anchors {:left 32 :top 0 :bottom 0 :right 200}
       :align {:horizontal start :vertical center})
     (label#statusSize :text ~(str window.width " × " window.height)
       :font small
       :textColor @muted
       :anchors {:right 32 :top 0 :bottom 0}
       :width 160
       :align {:horizontal end :vertical center})))

;;; Over the pages

(defview dialogs
  "The dialogs, each over a scrim that dims the page: a light's colour, the details of a control, a card's editor."
  []
  [`(node#scrim-app :use scrim :with {:open state.colourOpen})
   `(node#colourDialog :use colourDialog)
   `(node#scrim-app-2 :use scrim :with {:open state.info.open})
   `(node#moreInfo :use moreInfo)
   `(node#scrim-app-3 :use scrim :with {:open state.cardEdit.open})
   `(node#cardEditor :use cardEditor)])

(defview notes "The notes at the bottom: the last message, and the progress of a device's call while it runs." []
  [`(node#toast :dir auto :use snackbar :with {:open state.toast.open :message ~state.toast.message})
   `(node#busyToast :dir auto
      :when ~(some? computed.busyDevice)
      :use snackbar
      :with {:open true
             :message ~(t "device.busy" {:name (lower-case computed.busyDevice.name)})
             :progress ~computed.busyDevice.progress})])

;;; Parts every page uses

;; Each item has a title, an icon, a run and its with, or is a separator.
(defcomponent popMenu "An overflow menu open at the state path: a raised, rounded list of items under its button."
  {:open "" :items [] :width 200}
  `(node :overlay {:placement below-end :open ~open :dismiss outside :modal true}
     :width ~width
     :height ~(+ 16 (sum items (fn [it] (if (= it.separator true) 17 48))))
     :radius 8
     :color @card
     :shadow {:y 4 :blur 16 :color "#00000047"}
     (column :fill 8 :color transparent :role menu :arrows vertical
       ~(repeat [item items]
         (menu-item open item)))))

(defview menu-item "An item of a menu: its icon and title, which runs its action and closes the menu; or a separator."
  [open item]
  `(node :anchors {:left 0 :right 0} :height ~(if (= item.separator true) 17 48) :color transparent
     {:when ~(= item.separator true) :role separator :anchors {:left 0 :right 0 :top 8} :height 1 :color @divider}
     (node :when ~(not= item.separator true)
       :fill 0
       :color transparent
       :hover {:color @cardHover}
       :focused {:color @cardHover}
       :cursor hand
       :focusable true
       :role menuitem
       :label ~item.title
       :onClick ~(handler pickMenuItem open item.run item.with)
       (label :icon ~item.icon :text "" :textColor @muted :anchors {:left 16 :top 12} :width 24 :height 24
         :align center)
       (label :text ~item.title
         :textColor @text
         :anchors {:left 56 :right 16 :top 0 :bottom 0}
         :align {:horizontal start :vertical center}))))

;; A transparent slider over it drags it.
(defcomponent levelBar "A tile's rounded bar, filled from the start to level (0 to 1), a mark at the fill's end."
  {:level 0 :fill @accent :back @raised}
  `(node :height 40 :radius 12 :color ~back
     (node :when ~(> level 0)
       :anchors {:left 0 :top 0 :bottom 0}
       :width ~(str (* (clamp level 0 1) 100) "%")
       :minWidth 24
       :radius 12
       :color ~fill
       {:anchors {:right 8 :top 12} :width 4 :height 16 :radius 2 :color "#ffffffcc"})))

(defcomponent snackbar "A short message in a dark bar at the bottom centre: Dismiss, or the progress of a call."
  {:open state.toast.open :message "" :progress nil}
  `(node :overlay {:placement window :open ~open}
     :anchors {:bottom 24}
     :x ~(/ (- window.width (min 480 (- window.width 32))) 2)
     :width ~(min 480 (- window.width 32))
     :height 48
     :color @snack
     :radius 4
     :shadow {:y 3 :blur 10 :color "#00000052"}
     :role status
     :live polite
     (label :text ~message
       :textColor "#ffffffde"
       :anchors {:left 16 :right 104 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     {:when ~(nil? progress)
      :style button
      :text ~(t "snack.dismiss")
      :color transparent
      :hover {:color "#ffffff14"}
      :textColor @snackAction
      :anchors {:right 8 :top 6}
      :width 88
      :height 36
      :radius 4
      :onClick ~(handler closeAt open)}
     (node :when ~(some? progress) :anchors {:left 0 :right 0 :bottom 0} :height 3 :color "#ffffff1f"
       {:anchors {:left 0 :top 0 :bottom 0}
        :width ~(str (* (clamp (or-else progress 0) 0 1) 100) "%")
        :color @snackAction})))

(defcomponent viewTab "A view in the top bar: its title in a pill, filled while it is the one shown."
  {:title "" :selected false}
  `(label :text ~title
     :textColor ~(if selected "@onNavSelected" "@onHeaderDim")
     :color ~(if selected "@navSelected" "transparent")
     :anchors {:top 10 :bottom 10}
     :fitText true
     :padding 16
     :radius 18
     :align center
     :hover {:color ~(if selected "@navSelected" "@headerHover")}
     :cursor hand
     :focusable true
     :role tab
     :selected ~selected))
