{:note "Flow's sidebar, included by index.screen.json: the workspace, the personal pages, the team's pages and its boards."
  :components {:navItem {:note "One row of the sidebar: an icon, a name and a count; the use gives its onClick"
      :params {:icon circle :text "" :active false :count ""}
      :node (node #label-text
        :type label
        :font small
        :height 28
        :anchors {:left 0 :right 0}
        :padding 8
        :radius @radius.chip
        :icon "{{icon}}"
        :text "  {{text}}"
        :textColor @color.text
        :color "{{active ? '@color.sidebarOn' : 'transparent'}}"
        :hover {:color "{{active ? '@color.sidebarOn' : '@color.sidebarHover'}}"}
        :align {:horizontal start :vertical center}
        :cursor hand
        :focusable true
        :role link
        :selected "{{active}}"
        (node #navCount
          :when "{{count != ''}}"
          :type label
          :font small
          :text "{{count}}"
          :textColor @color.muted
          :anchors {:right 8 :top 0 :bottom 0}
          :width 40
          :align {:horizontal end :vertical center}))}
    :navHeading {:note "A small heading over a group of sidebar rows"
      :params {:text ""}
      :node (node #label-text-2
        :type label
        :font small
        :text "{{text}}"
        :textColor @color.muted
        :height 28
        :padding 8
        :anchors {:left 0 :right 0}
        :align {:horizontal start :vertical end})}
    :sidebar {:note "The left column of the window"
      :params {}
      :node (node #sidebarPanel :dir auto :anchors {:left 0 :top 0 :bottom 0} :width 220 :color @color.sidebar
        (node #sidebarEdge :anchors {:right 0 :top 0 :bottom 0} :width 1 :color @color.border)
        (node #workspace
          :type row
          :anchors {:left 12 :top 10}
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
          :label "{{t('workspace.name')}}"
          :isOpen "{{state.workspaceOpen}}"
          :keys {:enter {:toggle state.workspaceOpen} :space {:toggle state.workspaceOpen}}
          :onClick {:toggle state.workspaceOpen}
          (node #workspaceMenu
            :overlay {:placement below :open state.workspaceOpen :dismiss outside}
            :y 4
            :width 240
            :height 108
            :radius @radius.card
            :shadow {:y 6 :blur 16 :color "#0000002e"}
            :borderWidth 1
            :borderColor @color.border
            :color @color.menu
            (node #workspaceItems
              :type column
              :anchors {:left 6 :right 6 :top 6 :bottom 6}
              :gap 2
              :role menu
              :arrows vertical
              (node #settingsButton
                :style menuRow
                :icon settings
                :text "  {{t('settings.title')}}"
                :role menuitem
                :label "{{t('settings.title')}}"
                :tooltip "Cmd+,"
                :onClick [{:set state.workspaceOpen :to false} {:run openSettings}])
              (node #themeToggle
                :style menuRow
                :icon "{{state.theme == 'light' ? 'moon' : 'sun'}}"
                :text "  {{t('theme.menu')}}"
                :role menuitem
                :label "{{t('theme.tooltip')}}"
                :tooltip "Cmd+J"
                :onClick [{:set state.workspaceOpen :to false} {:run toggleTheme}])
              (node #languageButton
                :style menuRow
                :icon languages
                :text "  {{t('locale.menu', { code: upper(settings.appearance.language) })}}"
                :role menuitem
                :label "{{t('locale.tooltip')}}"
                :tooltip "Cmd+L"
                :onClick [{:set state.workspaceOpen :to false} {:run changeLanguage}])))
          (node #workspaceMark
            :type label
            :font small
            :text L
            :textColor "#ffffff"
            :color @color.accent
            :radius 4
            :width 18
            :height 18
            :align center
            :x 6)
          (node #workspaceName
            :type label
            :font small
            :text "{{t('workspace.name')}}"
            :textColor @color.text
            :fitText true
            :height 28
            :align {:horizontal start :vertical center})
          (node #workspaceMore
            :type label
            :font small
            :icon chevron-down
            :textColor @color.muted
            :fitText true
            :height 28
            :align center))
        (node #sidebarSearch
          :style topButton
          :text ""
          :icon search
          :anchors {:right 44 :top 10}
          :height 28
          :width 28
          :padding 0
          :align center
          :hover {:color @color.sidebarHover}
          :tooltip "{{t('goTo.tooltip')}}"
          :onClick goToCardCommand)
        (node #sidebarNew
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
          :tooltip "{{t('nav.newTooltip')}}"
          :onClick newCardCommand)
        (node #sidebarNav
          :type column
          :anchors {:left 12 :right 12 :top 50 :bottom 48}
          :gap 1
          :scroll vertical
          :role navigation
          (node #navInbox
            :use navItem
            :with {:icon inbox
              :text "{{t('nav.inbox')}}"
              :active "{{state.page == 'inbox'}}"
              :count "{{computed.unread > 0 ? computed.unread : ''}}"}
            :onClick goToInbox)
          (node #navMine
            :use navItem
            :with {:icon circle-user
              :text "{{t('nav.mine')}}"
              :active "{{state.page == 'issues' && state.owner == state.me}}"}
            :onClick [{:run showPage :with {:page issues}}
              {:set state.owner :to "{{state.owner == state.me ? '' : state.me}}"}])
          (node #navHeadingBoards :use navHeading :with {:text "{{t('nav.boards')}}"})
          (node #navBoard
            :repeat {:for b :in state.boards :key "{{b.id}}"}
            :use navItem
            :with {:icon layers-2
              :text "{{b.label}}"
              :active "{{b.id == state.boardId && b.id != state.boards[0].id && state.page == 'issues'}}"}
            :onClick [{:run showPage :with {:page issues}} {:run switchBoard :with (node #"{{b.id}}")}])
          (node #navHeadingTeams :use navHeading :with {:text "{{t('nav.teams')}}"})
          (node #navTeam
            :use navItem
            :with {:icon square :text Flow}
            :onClick [{:run showPage :with {:page issues}}
              {:run switchBoard :with (node #"{{state.boards[0].id}}")}
              {:set state.owner :to ""}
              {:run showBoard}])
          (node #navIssues
            :use navItem
            :with {:icon copy
              :text "{{t('nav.issues')}}"
              :active "{{state.page == 'issues' && state.owner != state.me && state.boardId == state.boards[0].id}}"}
            :padding 26
            :onClick [{:run showPage :with {:page issues}}
              {:run switchBoard :with (node #"{{state.boards[0].id}}")}
              {:set state.owner :to ""}])
          (node #navCycles
            :use navItem
            :with {:icon timer :text "{{t('nav.cycles')}}" :active "{{state.page == 'cycles'}}"}
            :padding 26
            :onClick goToCycles)
          (node #navProjects
            :use navItem
            :with {:icon box :text "{{t('nav.projects')}}" :active "{{state.page == 'projects'}}"}
            :padding 26
            :onClick goToProjects))
        (node #navHelp
          :style topButton
          :text ""
          :icon circle-help
          :anchors {:left 12 :bottom 12}
          :height 28
          :width 28
          :padding 0
          :align center
          :hover {:color @color.sidebarHover}
          :tooltip "{{t('nav.commands')}}"
          :onClick commandPalette))}}}
