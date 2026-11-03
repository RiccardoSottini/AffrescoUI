;;; sidebar.lisp --- Flow's sidebar, included by index.screen.lisp: the workspace, the personal pages, the team's
;;; pages and its boards.

;;; Components

(defcomponent navItem "One row of the sidebar: an icon, a name and a count; the use gives its onClick"
  {:icon circle :text "" :active false :count ""}
  `(label#label-text :font small
     :height 28
     :anchors {:left 0 :right 0}
     :padding 8
     :radius @radius.chip
     :icon ~icon
     :text ~(str "  " text)
     :textColor @color.text
     :color ~(if active "@color.sidebarOn" "transparent")
     :hover {:color ~(if active "@color.sidebarOn" "@color.sidebarHover")}
     :align {:horizontal start :vertical center}
     :cursor hand
     :focusable true
     :role link
     :selected ~active
     (label#navCount :when ~(not= count "")
       :font small
       :text ~count
       :textColor @color.muted
       :anchors {:right 8 :top 0 :bottom 0}
       :width 40
       :align {:horizontal end :vertical center})))

(defcomponent navHeading "A small heading over a group of sidebar rows"
  {:text ""}
  `(label#label-text-2 :font small
     :text ~text
     :textColor @color.muted
     :height 28
     :padding 8
     :anchors {:left 0 :right 0}
     :align {:horizontal start :vertical end}))

(defcomponent sidebar "The left column of the window"
  {}
  `(node#sidebarPanel :dir auto :anchors {:left 0 :top 0 :bottom 0} :width 220 :color @color.sidebar
     {:id sidebarEdge :anchors {:right 0 :top 0 :bottom 0} :width 1 :color @color.border}
     ~(workspace-switcher)
     {:id sidebarSearch
      :style topButton
      :text ""
      :icon search
      :anchors {:right 44 :top 10}
      :height 28
      :width 28
      :padding 0
      :align center
      :hover {:color @color.sidebarHover}
      :tooltip ~(t "goTo.tooltip")
      :onClick goToCardCommand}
     {:id sidebarNew
      :style topButton
      :text ""
      :icon edit
      :anchors {:right 12 :top 10}
      :height 28
      :width 28
      :padding 0
      :align center
      :color @color.card
      :borderWidth 1
      :borderColor @color.border
      :hover {:color @color.sidebarHover}
      :tooltip ~(t "nav.newTooltip")
      :onClick newCardCommand}
     ~(sidebar-nav)
     {:id navHelp
      :style topButton
      :text ""
      :icon circle-help
      :anchors {:left 12 :bottom 12}
      :height 28
      :width 28
      :padding 0
      :align center
      :hover {:color @color.sidebarHover}
      :tooltip ~(t "nav.commands")
      :onClick commandPalette}))

;;; Views

(defview workspace-items "The workspace menu's items." []
  `(column#workspaceItems :anchors {:left 6 :right 6 :top 6 :bottom 6} :gap 2 :role menu :arrows vertical
     {:id settingsButton
      :style menuRow
      :icon settings
      :text ~(str "  " (t "settings.title"))
      :role menuitem
      :label ~(t "settings.title")
      :tooltip ~(t "key.settings")
      :onClick [{:set state.workspaceOpen :to false} ~(handler openSettings)]}
     {:id themeToggle
      :style menuRow
      :icon ~(if (= state.theme "light") "moon" "sun")
      :text ~(str "  " (t "theme.menu"))
      :role menuitem
      :label ~(t "theme.tooltip")
      :tooltip ~(t "key.theme")
      :onClick [{:set state.workspaceOpen :to false} ~(handler toggleTheme)]}
     {:id languageButton
      :style menuRow
      :icon languages
      :text ~(str "  " (t "locale.menu" {:code (upper-case settings.appearance.language)}))
      :role menuitem
      :label ~(t "locale.tooltip")
      :tooltip ~(t "key.language")
      :onClick [{:set state.workspaceOpen :to false} ~(handler changeLanguage)]}))

(defview workspace-switcher "The workspace's name at the top of the sidebar, which opens its menu." []
  `(row#workspace :anchors {:left 12 :top 10}
     :width 132
     :height 28
     :gap 8
     :alignItems center
     :radius @radius.chip
     :color transparent
     :hover {:color @color.sidebarHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "workspace.name")
     :isOpen ~state.workspaceOpen
     :keys {:enter {:toggle state.workspaceOpen} :space {:toggle state.workspaceOpen}}
     :onClick {:toggle state.workspaceOpen}
     (node#workspaceMenu :overlay {:placement below :open state.workspaceOpen :dismiss outside}
       :y 4
       :width 240
       :height 108
       :radius @radius.card
       :shadow {:y 6 :blur 16 :color "#0000002e"}
       :borderWidth 1
       :borderColor @color.border
       :color @color.menu
       ~(workspace-items))
     (label#workspaceMark :font small
       :text ~(t "key.l")
       :textColor "#ffffff"
       :color @color.accent
       :radius 4
       :width 18
       :height 18
       :align center
       :x 6)
     (label#workspaceName :font small
       :text ~(t "workspace.name")
       :textColor @color.text
       :fitText true
       :height 28
       :align {:horizontal start :vertical center})
     (label#workspaceMore :font
                          small
                          :icon
                          chevron-down
                          :textColor
                          @color.muted
                          :fitText
                          true
                          :height
                          28
                          :align
                          center)))

(defview sidebar-nav "The sidebar's rows: the inbox, my issues, the boards, the sprints and the projects." []
  `(column#sidebarNav :anchors {:left 12 :right 12 :top 50 :bottom 48} :gap 1 :scroll vertical :role navigation
     (node#navInbox :use navItem
       :with {:icon inbox
              :text ~(t "nav.inbox")
              :active ~(= state.page "inbox")
              :count ~(if (> computed.unread 0) computed.unread "")}
       :onClick goToInbox)
     (node#navMine :use navItem
       :with {:icon circle-user :text ~(t "nav.mine") :active ~(and (= state.page "issues") (= state.owner state.me))}
       :onClick showMyIssues)
     (node#navHeadingBoards :use navHeading :with {:text ~(t "nav.boards")})
     ~(repeat [b state.boards :key b.id]
       `(node#navBoard :use navItem
          :with {:icon layers-2
                 :text ~b.label
                 :active ~(and (= b.id state.boardId) (not= b.id state.boards.0.id) (= state.page "issues"))}
          :onClick [~(handler showPage "issues") ~(handler switchBoard b.id)]))
     (node#navHeadingTeams :use navHeading :with {:text ~(t "nav.teams")})
     (node#navTeam :use navItem
       :with {:icon square :text "Flow"}
       :onClick [~(handler showPage "issues")
                 ~(handler switchBoard state.boards.0.id)
                 {:set state.owner :to ""}
                 ~(handler showBoard)])
     (node#navIssues :use navItem
       :with {:icon copy
              :text ~(t "nav.issues")
              :active ~(and (= state.page "issues") (not= state.owner state.me) (= state.boardId state.boards.0.id))}
       :padding 26
       :onClick [~(handler showPage "issues")
                 ~(handler switchBoard state.boards.0.id)
                 {:set state.owner :to ""}])
     (node#navCycles :use navItem
       :with {:icon timer :text ~(t "nav.cycles") :active ~(= state.page "cycles")}
       :padding 26
       :onClick goToCycles)
     (node#navProjects :use navItem
       :with {:icon box :text ~(t "nav.projects") :active ~(= state.page "projects")}
       :padding 26
       :onClick goToProjects)))
