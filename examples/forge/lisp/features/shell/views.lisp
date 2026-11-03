;;; views.lisp --- what shell.lisp shows

;;; Views

(defcomponent forgeTitleBar "The forgeTitleBar component."
  {}
  `(row#title-bar :role banner
     :label ~(t "title-bar.label")
     :width "100%"
     :height 38
     :color @color.titleBar
     :alignItems center
     (node#menu-bar :when ~(not window.nativeMenuBar)
       :use menuBar
       :width 330
       :with {:open state.menuBar
              :menus ~[{:name "file" :title "File" :items menus.file}
                       {:name "view" :title "View" :items menus.view}
                       {:name "go" :title "Go" :items menus.go}
                       {:name "preferences" :title "Preferences" :items menus.preferences}
                       {:name "help" :title "Help" :items menus.help}]})
     (node#title-left :grow 1)
     (node#title-back :use forgeTitleButton
       :with {:icon arrow-left :tip ~(t "title-back.tip") :on ~(> (count state.nav.back) 0)}
       :onClick navigateBack)
     (node#title-forward :use forgeTitleButton
       :with {:icon arrow-right :tip ~(t "title-forward.tip") :on ~(> (count state.nav.forward) 0)}
       :onClick navigateForward)
     (node#title-centre-gap :width 4)
     ~(command-centre-view)
     (node#title-right :grow 1)
     (node#title-sidebar :use forgeTitleButton
       :with {:icon panel-left :tip ~(t "title-sidebar.tip") :on ~state.layout.sidebar}
       :onClick toggleSidebar)
     (node#title-panel :use forgeTitleButton
       :with {:icon panel-bottom :tip ~(t "title-panel.tip") :on ~state.layout.panel}
       :onClick togglePanel)
     (node#title-secondary :use forgeTitleButton
       :with {:icon panel-right :tip ~(t "title-secondary.tip") :on ~state.layout.secondary}
       :onClick toggleSecondarySidebar)
     (node#title-end :width 8)))

(defcomponent forgeTitleButton "The forgeTitleButton component."
  {:icon "" :tip "" :on false}
  `(label :role button
     :label ~tip
     :icon ~icon
     :font icons
     :width 28
     :height 26
     :align center
     :radius 7
     :cursor hand
     :textColor ~(if on "@color.text" "@color.muted")
     :tooltip ~tip
     :hover {:color @color.hover}))

(defcomponent forgeActivityItem "The forgeActivityItem component."
  {:view "" :icon "" :label "" :tip "" :badge 0}
  `(node :id ~(str "activity-" view)
     :role tab
     :label ~label
     :selected ~(and state.layout.sidebar (= state.sideView view))
     :width 44
     :height 42
     :color @color.activityBar
     :cursor hand
     :tooltip ~tip
     (node :id ~(str "activity-" view "-indicator")
       :anchors {:left 5 :top 4 :right 5 :bottom 4}
       :radius 8
       :color ~(if (and state.layout.sidebar (= state.sideView view)) "@color.selection" "@color.activityBar"))
     (label :icon ~icon
       :font iconsRail
       :anchors {:left 5 :top 4 :right 5 :bottom 4}
       :align center
       :radius 8
       :textColor ~(if (and state.layout.sidebar (= state.sideView view)) "@color.accent" "@color.muted")
       :hover {:textColor ~(if (and state.layout.sidebar (= state.sideView view)) "@color.accent" "@color.text")
               :borderWidth 1
               :borderColor @color.border})
     (label :id ~(str "activity-" view "-badge")
       :when ~(> badge 0)
       :text ~(if (> badge 99) "99+" (text badge))
       :anchors {:right 2 :top 1}
       :height 15
       :padding 4
       :fitText true
       :radius 8
       :align center
       :color @color.accent
       :textColor "#ffffff")))

(defcomponent forgeActivityBar "The forgeActivityBar component."
  {}
  `(column#activity-bar :role tablist :label ~(t "activity-bar.label") :width 44 :height "100%" :color @color.activityBar
     (node#activity-top :height 4)
     (node :use forgeActivityItem
       :with {:view explorer
              :icon folder-tree
              :label ~(t "activityBar.explorer.label")
              :tip ~(t "activityBar.explorerCmdShift.tip")}
       :onClick showExplorer)
     (node :use forgeActivityItem
       :with {:view search
              :icon text-search
              :label ~(t "activityBar.search.label")
              :tip ~(t "activityBar.searchCmdShift.tip")}
       :onClick showSearch)
     (node :use forgeActivityItem
       :with {:view scm
              :icon git-fork
              :label ~(t "activityBar.sourceControl.label")
              :tip ~(t "activityBar.sourceControlCmd.tip")
              :badge ~state.scm.status.count}
       :onClick showSourceControl)
     (node :use forgeActivityItem
       :with {:view debug
              :icon bug-play
              :label ~(t "activityBar.debugger.label")
              :tip ~(t "activityBar.debuggerCmdShift.tip")}
       :onClick showDebug)
     (node :use forgeActivityItem
       :with {:view features
              :icon puzzle
              :label ~(t "activityBar.features.label")
              :tip ~(t "activityBar.featuresCmdShift.tip")}
       :onClick showFeatures)
     (node :use forgeActivityItem
       :with {:view agent :icon sparkles :label ~(t "activityBar.agent.label") :tip ~(t "activityBar.agentCmdAlt.tip")}
       :onClick showAgent)
     (node#activity-gap :grow 1)
     (node :use forgeAccountsStatus)
     ~(activity-settings)
     (node#activity-bottom :height 4)))

(defview activity-settings "#activity-settings, in forgeActivityBar." []
  `(label#activity-settings :role button
     :label ~(t "activity-settings.label")
     :icon settings
     :font iconsRail
     :width 44
     :height 42
     :align center
     :cursor hand
     :tooltip ~(t "activity-settings.tip")
     :textColor @color.muted
     :hover {:textColor @color.text}
     :onClick openSettings))

(defcomponent forgeViewTitle "The forgeViewTitle component."
  {:text ""}
  `(label :text ~text :textColor @color.muted :width "100%" :height 35 :padding 12))

(defcomponent forgeSideBar "The forgeSideBar component."
  {}
  `(column#side-bar :role complementary :label ~(t "side-bar.label") :point sidebar.views :color @color.sideBar
     (node :when ~(= state.sideView "scm") :use forgeScmView)
     (node :when ~(= state.sideView "debug") :use forgeDebugView)
     (node :when ~(= state.sideView "agent") :use forgeAgentView)
     (node :when ~(= state.sideView "features") :use forgeFeaturesView)
     (node :when ~(= state.sideView "search") :use forgeSearchPage)
     (node :when ~(= state.sideView "explorer") :use forgeExplorerTitle)
     (label#side-bar-folder :when ~(and (= state.sideView "explorer") (not (empty? state.folder.root)))
       :icon chevron-down
       :text ~(upper-case computed.folderName)
       :textColor @color.text
       :width "100%"
       :height 22
       :padding 6)
     (node :when ~(and (= state.sideView "explorer") (not (empty? state.folder.root))) :use forgeExplorer)
     ~(side-bar-empty)))

(defcomponent forgeWelcome "The forgeWelcome component."
  {}
  `(row#welcome :width "100%" :grow 1 :color @color.editor
     (node#welcome-margin :grow 1)
     ~(welcome-body)
     (node#welcome-end :grow 1)))

(defcomponent forgeStatusItem "The forgeStatusItem component."
  {:text "" :icon "" :tip ""}
  `(label :role button
     :label ~tip
     :text ~text
     :icon ~icon
     :padding 7
     :height 20
     :fitText true
     :radius 5
     :cursor hand
     :textColor @color.statusText
     :tooltip ~tip
     :hover {:color @color.hover}))

(defcomponent forgeStatusBar "The forgeStatusBar component."
  {}
  `(row#status-bar :role status
     :label ~(t "status-bar.label")
     :point statusBar.right
     :width "100%"
     :height 28
     :color @color.statusBar
     :alignItems center
     :gap 2
     (node#status-start :width 6)
     ~(status-remote)
     (node#status-branch :use forgeStatusItem
       :when ~(not (empty? state.scm.status.branch.head))
       :with {:icon git-branch :text ~computed.scmBranchText :tip ~(t "status-branch.tip")}
       :onClick scmBranches)
     (node#status-sync :use forgeStatusItem
       :when ~(not (empty? state.scm.status.branch.head))
       :with {:icon refresh-cw :tip ~(t "status-sync.tip")}
       :onClick scmRefresh)
     ~(status-problems-view)
     (node#status-gap :grow 1)
     ~(status-position)
     ~(status-indent)
     ~(status-encoding)
     (node#status-eol :use forgeStatusItem
       :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
       :with {:text ~(or-else computed.activeDoc.eol "LF") :tip ~(t "status-eol.tip")}
       :onClick changeEndOfLine)
     (node#status-language-status :use forgeStatusItem
       :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
       :with {:icon braces :tip ~(t "status-language-status.tip")}
       :onClick showOutput)
     ~(status-language)
     (node#status-notifications :use forgeStatusItem
       :with {:icon ~(if (> (count state.notifications) 0) "bell-dot" "bell") :tip ~(t "status-notifications.tip")}
       :onClick showNotifications)
     (node#status-end :width 6)))

(defview status-language "#status-language, in forgeStatusBar." []
  `(node#status-language :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:text ~(or computed.activeDoc.language "Plain Text") :tip ~(t "status-language.tip")}
     :onClick changeLanguageMode))

(defview status-indent "#status-indent, in forgeStatusBar." []
  `(node#status-indent :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:text ~(str "Spaces: " (text (or-else settings.editor.tabSize 4))) :tip ~(t "status-indent.tip")}
     :onClick changeIndentation))

(defview status-encoding "#status-encoding, in forgeStatusBar." []
  `(node#status-encoding :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:text ~(t "status-encoding") :tip ~(t "status-encoding.tip")}
     :disabled true
     :cursor arrow
     :textColor @color.muted))

(defview status-remote "#status-remote, in forgeStatusBar." []
  `(label#status-remote :role button
     :label ~(t "status-remote.label")
     :icon cloud
     :width 28
     :height 20
     :radius 5
     :align center
     :cursor hand
     :textColor @color.accent
     :hover {:color @color.hover}
     :tooltip ~(t "status-remote.tip")
     :onClick accountsMenu))

(defview status-position "#status-position, in forgeStatusBar." []
  `(node#status-position :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:tip ~(t "status-position.tip")
            :text ~(str "Ln "
                        (text state.caret.line)
                        ", Col "
                        (text state.caret.column)
                        (if (> state.caret.selected 0) (str " (" (text state.caret.selected) " selected)") ""))}
     :onClick goToLinePick))

(defcomponent forgeNotificationCentre "The forgeNotificationCentre component."
  {}
  `(column#notification-centre :role dialog
     :label ~(t "notification-centre.label")
     :width 450
     :height ~(+ 40 (* (max 1 (min 6 (count state.notifications))) 28))
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :radius 8
     :overlay {:anchor "#status-notifications" :placement above-end :open state.notificationCentre :dismiss outside}
     (row#notification-centre-head :width "100%" :height 34 :alignItems center
       (label#notification-centre-title :text ~(if (empty? state.notifications) "NO NEW NOTIFICATIONS" "NOTIFICATIONS")
         :textColor @color.muted
         :grow 1
         :padding 10)
       (node#notification-centre-clear :use forgeTitleButton
         :with {:icon list-x :tip ~(t "notification-centre-clear.tip")}
         :onClick clearNotifications)
       (node#notification-centre-hide :use forgeTitleButton
         :with {:icon chevron-down :tip ~(t "notification-centre-hide.tip")}
         :onClick {:set state.notificationCentre :to false})
       (node#notification-centre-end :width 4))
     (list#notifications :label ~(t "notifications.label")
       :when ~(not (empty? state.notifications))
       :width "100%"
       :grow 1
       :items ~computed.notificationRows
       :rowHeight 28
       :color @color.widget
       :textColor @color.text
       :selectedBackground @color.selection)))

(defview command-centre-view "#command-centre." []
  `(row#command-centre :role button
     :label ~(t "command-centre.label")
     :width 380
     :height 26
     :color @color.input
     :radius 7
     :borderWidth 1
     :borderColor @color.border
     :alignItems center
     :gap 6
     :cursor hand
     :tooltip ~(t "command-centre.tip")
     :hover {:color @color.hover}
     :onClick quickOpen
     (node#command-centre-start :width 6)
     (label#command-centre-icon :icon search :textColor @color.muted :fitText true)
     (label#command-centre-text :text
                                ~(str "Search " computed.folderName)
                                :textColor
                                @color.muted
                                :fitText
                                true
                                :height
                                20)
     (node#command-centre-end :grow 1)
     (label#command-centre-hint :text ~(t "command-centre-hint")
       :textColor @color.muted
       :fitText true
       :height 18
       :padding 6
       :radius 4
       :borderWidth 1
       :borderColor @color.border)
     (node#command-centre-hint-end :width 4)))

(defview side-bar-empty "#side-bar-empty." []
  `(column#side-bar-empty :when ~(and (= state.sideView "explorer") (empty? state.folder.root))
     :width "100%"
     :alignItems center
     :gap 4
     (label#side-bar-empty-text :text
                                ~(t "side-bar-empty-text")
                                :textColor
                                @color.text
                                :width
                                "100%"
                                :padding
                                12
                                :wrap
                                true)
     (label#side-bar-open :text ~(t "side-bar-open")
       :width 200
       :height 28
       :align center
       :color @color.accent
       :textColor "#ffffff"
       :radius 6
       :cursor hand
       :focusable true
       :onClick openFolder)))

(defview welcome-open-view "#welcome-open." []
  `(label#welcome-open :role button
     :label ~(t "welcome-open.label")
     :icon folder-open
     :text ~(t "welcome-open")
     :width "100%"
     :height 38
     :padding 12
     :radius 9
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :textColor @color.text
     :focusable true
     :cursor hand
     :tooltip ~(t "welcome-open.tip")
     :hover {:color @color.hover :borderColor @color.accent}
     :focused {:borderColor @color.accent}
     :onClick openFolder))

(defview welcome-files-view "#welcome-files." []
  `(label#welcome-files :role button
     :label ~(t "welcome-files.label")
     :icon file-search
     :text ~(t "welcome-files")
     :width "100%"
     :height 38
     :padding 12
     :radius 9
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :textColor @color.text
     :focusable true
     :cursor hand
     :tooltip ~(t "welcome-files.tip")
     :hover {:color @color.hover :borderColor @color.accent}
     :focused {:borderColor @color.accent}
     :onClick quickOpen))

(defview welcome-commands-view "#welcome-commands." []
  `(label#welcome-commands :role button
     :label ~(t "welcome-commands.label")
     :icon command
     :text ~(t "welcome-commands")
     :width "100%"
     :height 38
     :padding 12
     :radius 9
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :textColor @color.text
     :focusable true
     :cursor hand
     :tooltip ~(t "welcome-commands.tip")
     :hover {:color @color.hover :borderColor @color.accent}
     :focused {:borderColor @color.accent}
     :onClick showCommands))

(defview welcome-theme-view "#welcome-theme." []
  `(label#welcome-theme :role button
     :label ~(t "welcome-theme.label")
     :icon palette
     :text ~(t "welcome-theme")
     :width "100%"
     :height 38
     :padding 12
     :radius 9
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :textColor @color.text
     :focusable true
     :cursor hand
     :tooltip ~(t "welcome-theme.tip")
     :hover {:color @color.hover :borderColor @color.accent}
     :focused {:borderColor @color.accent}
     :onClick pickTheme))

(defview welcome-settings-view "#welcome-settings." []
  `(label#welcome-settings :role button
     :label ~(t "welcome-settings.label")
     :icon settings
     :text ~(t "welcome-settings")
     :width "100%"
     :height 38
     :padding 12
     :radius 9
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :textColor @color.text
     :focusable true
     :cursor hand
     :tooltip ~(t "welcome-settings.tip")
     :hover {:color @color.hover :borderColor @color.accent}
     :focused {:borderColor @color.accent}
     :onClick openSettings))

(defview shell-view-view "#." [file fi]
  `(label :id ~(str "welcome-recent-" fi)
     :role link
     :label ~(str "Open " file.id)
     :icon file
     :text ~file.title
     :width "100%"
     :height 26
     :padding 6
     :radius 6
     :textColor @color.accent
     :focusable true
     :cursor hand
     :tooltip ~file.id
     :hover {:color @color.hover}
     :focused {:color @color.hover}
     :onClick ~(handler openPath file.id file.title)))

(defview welcome-body "#welcome-body." []
  `(column#welcome-body :width 400 :height "100%" :gap 8
     (node#welcome-top :height 96)
     (node#welcome-mark :width 36 :height 4 :radius 2 :color @color.accent)
     (label#welcome-title :text ~(t "welcome-title") :font title :textColor @color.text :fitText true)
     (label#welcome-subtitle :text ~(t "welcome-subtitle") :textColor @color.muted :fitText true)
     (node#welcome-gap :height 12)
     (label#welcome-start :text ~(t "welcome-start") :textColor @color.muted :fitText true)
     ~(welcome-open-view)
     ~(welcome-files-view)
     ~(welcome-commands-view)
     ~(welcome-theme-view)
     ~(welcome-settings-view)
     (node#welcome-recent-gap :when ~(not (empty? computed.welcomeRecent)) :height 12)
     (label#welcome-recent :when ~(not (empty? computed.welcomeRecent))
       :text ~(t "welcome-recent")
       :textColor @color.muted
       :fitText true)
     ~(repeat [file computed.welcomeRecent :index fi :key file.id]
       (shell-view-view file fi))))

(defview status-problems-view "#status-problems." []
  `(label#status-problems :role button
     :label ~(str (plural computed.errors "error") ", " (plural computed.warnings "warning"))
     :runs [~" "
            {:icon circle-x}
            ~(+ " " (text computed.errors) "  ")
            {:icon alert-triangle}
            ~(+ " " (text computed.warnings))]
     :padding 7
     :height 20
     :fitText true
     :radius 5
     :cursor hand
     :textColor @color.statusText
     :tooltip ~(t "status-problems.tip")
     :hover {:color @color.hover}
     :onClick showProblems))
