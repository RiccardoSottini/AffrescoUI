;;; views.lisp --- the workbench's frame: the title bar, the view rail, the side bar, the welcome page, the status bar

(ns forge.features.shell.views)

;;; The title bar

(defcomponent forgeTitleBar "The title bar: the menus, Back and Forward, the command centre, and the layout's buttons."
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
              :menus ~[{:name "file" :title (t "menu.file") :items menus.file}
                       {:name "view" :title (t "menu.view") :items menus.view}
                       {:name "go" :title (t "menu.go") :items menus.go}
                       {:name "preferences" :title (t "menu.preferences") :items menus.preferences}
                       {:name "help" :title (t "menu.help") :items menus.help}]})
     (node#title-left :grow 1)
     (node#title-back :use forgeTitleButton
       :with {:icon arrow-left :tip ~(t "title-back.tip") :on ~(> (count state.nav.back) 0)}
       :onClick navigateBack)
     (node#title-forward :use forgeTitleButton
       :with {:icon arrow-right :tip ~(t "title-forward.tip") :on ~(> (count state.nav.forward) 0)}
       :onClick navigateForward)
     (node#title-centre-gap :width 4)
     ~(command-centre)
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

(defcomponent forgeTitleButton "A button of an icon, its tooltip `tip`; dimmed while it is off."
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

(defview command-centre "The command centre: a click opens Go to File, the folder's name on it." []
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
     (label#command-centre-text :text ~(t "command-centre.text" {:folder computed.folderName})
       :textColor @color.muted
       :fitText true
       :height 20)
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

;;; The view rail

(defcomponent forgeActivityBar "The view rail: a button for each view of the side bar, the account and the settings."
  {}
  `(column#activity-bar :role tablist :label ~(t "activity-bar.label") :width 44 :height "100%" :color @color.activityBar
     (node#activity-top :height 4)
     ~(activity-item "explorer" "folder-tree" "showExplorer")
     ~(activity-item "search" "text-search" "showSearch")
     (node :use forgeActivityItem
       :with {:view scm :icon git-fork :label ~(t "activity.scm") :tip ~(t "activity.scm.tip") :badge ~state.scm.status.count}
       :onClick showSourceControl)
     ~(activity-item "debug" "bug-play" "showDebug")
     ~(activity-item "features" "puzzle" "showFeatures")
     ~(activity-item "agent" "sparkles" "showAgent")
     (node#activity-gap :grow 1)
     (node :use forgeAccountsStatus)
     ~(activity-settings)
     (node#activity-bottom :height 4)))

(defview activity-item "A view's button on the rail, which shows it or hides the side bar." [view icon run]
  `(node :use forgeActivityItem
     :with {:view ~view :icon ~icon :label ~(t (str "activity." view)) :tip ~(t (str "activity." view ".tip"))}
     :onClick ~run))

(defcomponent forgeActivityItem
  "A view's button on the rail: outlined while its view is shown, with a badge if it counts something."
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

(defview activity-settings "Settings, at the rail's foot." []
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

;;; The side bar

(defcomponent forgeViewTitle "A side bar view's title."
  {:text ""}
  `(label :text ~text :textColor @color.muted :width "100%" :height 35 :padding 12))

(defcomponent forgeSideBar "The side bar: the view chosen on the rail; the explorer's folder, or a button to open one."
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

(defview side-bar-empty "The explorer with no folder open: a button to open one." []
  `(column#side-bar-empty :when ~(and (= state.sideView "explorer") (empty? state.folder.root))
     :width "100%"
     :fitContent height
     :alignItems center
     :gap 4
     (label#side-bar-empty-text :text ~(t "side-bar-empty-text")
       :textColor @color.text
       :width "100%"
       :height 40
       :padding 12
       :wrap true)
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

;;; The welcome page

(defcomponent forgeWelcome
  "The welcome page, when no file is open: its title, then two columns in the middle, Start and Recent on the left,
the walkthrough and the keys on the right."
  {}
  `(row#welcome :width "100%" :grow 1 :color @color.editor :scroll vertical
     (node#welcome-margin :grow 1)
     ~(welcome-body)
     (node#welcome-end :grow 1)))

(defview welcome-body "The welcome page's column: its title and what Forge is, above the two columns." []
  `(column#welcome-body :width 760 :fitContent height :gap 6
     (node#welcome-top :height 40)
     (node#welcome-mark :width 36 :height 4 :radius 2 :color @color.accent)
     (label#welcome-title :text ~(t "welcome-title") :font title :textColor @color.text :fitText true)
     (label#welcome-subtitle :text ~(t "welcome-subtitle") :textColor @color.muted :fitText true)
     (node#welcome-gap :height 22)
     (row#welcome-columns :width "100%" :fitContent height :gap 48
       ~(welcome-left)
       ~(welcome-right))
     (node#welcome-bottom :height 32)))

(defview welcome-left "Start, a button for each way in; then Recent, the files opened in this folder." []
  `(column#welcome-left :width 330 :fitContent height :gap 2
     ~(welcome-heading "welcome-start")
     (node#welcome-new-file :when ~(not (empty? state.folder.root)) :use forgeWelcomeAction
       :with {:key welcome-new-file :icon file-plus :command explorerNewFile}
       :onClick explorerNewFile)
     (node#welcome-open :use forgeWelcomeAction :with {:key welcome-open :icon folder-open :command openFolder} :onClick openFolder)
     (node#welcome-clone :use forgeWelcomeAction
       :with {:key welcome-clone :icon git-branch :command cloneRepository}
       :onClick cloneRepository)
     (node#welcome-files :use forgeWelcomeAction :with {:key welcome-files :icon file-search :command quickOpen} :onClick quickOpen)
     (node#welcome-commands :use forgeWelcomeAction
       :with {:key welcome-commands :icon command :command showCommands}
       :onClick showCommands)
     (node#welcome-recent-gap :height 18)
     (label#welcome-recent :text ~(t "welcome-recent") :textColor @color.text :fitText true :height 24)
     (label#welcome-recent-none :when ~(empty? computed.welcomeRecent)
       :text ~(t "welcome-recent-none")
       :textColor @color.muted
       :width "100%"
       :height 36
       :wrap true)
     ~(repeat [file computed.welcomeRecent :index fi :key file.id]
       (welcome-recent-file file fi))))

(defview welcome-right "The walkthrough, four steps checked as they are done; then the keys a daily user needs." []
  `(column#welcome-right :grow 1 :fitContent height :gap 4
     ~(welcome-heading "welcome-walkthrough")
     (node#welcome-step-theme :use forgeWelcomeStep
       :with {:key welcome-step-theme :icon palette :done ~state.walkthrough.theme}
       :onClick welcome-step-theme)
     (node#welcome-step-folder :use forgeWelcomeStep
       :with {:key welcome-step-folder :icon folder-open :done ~(not (empty? state.folder.root))}
       :onClick openFolder)
     (node#welcome-step-live :use forgeWelcomeStep
       :with {:key welcome-step-live :icon sparkles :done ~state.walkthrough.live}
       :onClick welcome-step-live)
     (node#welcome-step-keys :use forgeWelcomeStep
       :with {:key welcome-step-keys :icon keyboard :done ~state.walkthrough.keys}
       :onClick welcome-step-keys)
     (node#welcome-keys-gap :height 12)
     ~(welcome-heading "welcome-keys")
     ~(repeat [k computed.welcomeKeys :index ki :key k.name]
       (welcome-key k ki))))

(defview welcome-heading "A heading of the welcome page's columns." [id]
  `(label :id ~id :text ~(t id) :textColor @color.text :fitText true :height 24))

(defcomponent forgeWelcomeAction "A way in: a link with its icon, which runs `command`, its key on the right."
  {:key "" :icon "" :command ""}
  `(row :role button
     :label ~(t (str key ".label"))
     :width "100%"
     :height 30
     :radius 6
     :alignItems center
     :gap 8
     :focusable true
     :cursor hand
     :tooltip ~(t (str key ".tip"))
     :hover {:color @color.hover}
     :focused {:color @color.hover}
     (node :width 2)
     (label :icon ~icon :textColor @color.accent :fitText true)
     (label :text ~(t (str key ".label")) :textColor @color.accent :grow 1 :height 20)
     (label :text ~(key-label command) :font code :textColor @color.muted :fitText true :height 20)
     (node :width 6)))

(defcomponent forgeWelcomeStep "A walkthrough step: a check once `done`, its title and what it does; a click does it."
  {:key "" :icon "" :done false}
  `(row :role button
     :label ~(t (str key ".title"))
     :selected ~done
     :width "100%"
     :height 48
     :radius 8
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :gap 10
     :focusable true
     :cursor hand
     :hover {:color @color.hover :borderColor @color.accent}
     :focused {:borderColor @color.accent}
     (node :width 2)
     (label :icon ~(if done "check-circle" icon)
       :textColor ~(if done "@color.accent" "@color.muted")
       :width 18
       :height 48
       :align center)
     (column :grow 1 :height 48 :gap 0
       (node :height 6)
       (label :text ~(t (str key ".title")) :textColor @color.text :width "100%" :height 18)
       (label :text ~(t (str key ".text")) :textColor @color.muted :width "100%" :height 18))
     (node :width 8)))

(defview welcome-key "A key of the welcome page: what it does, and the key, from the command it runs." [k ki]
  `(row :id ~(str "welcome-key-" ki)
     :width "100%"
     :height 24
     :alignItems center
     (label :text ~k.title :textColor @color.muted :grow 1 :height 24 :align "start center")
     (label :text ~k.key
       :font code
       :textColor @color.text
       :fitText true
       :height 20
       :padding 6
       :radius 4
       :borderWidth 1
       :borderColor @color.border)))

(defview welcome-recent-file "A recent file on the welcome page: a link that opens it, its folder beside it." [file fi]
  `(row :id ~(str "welcome-recent-" fi)
     :role link
     :label ~(t "welcome-recent.open" {:file file.id})
     :width "100%"
     :height 26
     :radius 6
     :alignItems center
     :gap 8
     :focusable true
     :cursor hand
     :tooltip ~file.id
     :hover {:color @color.hover}
     :focused {:color @color.hover}
     :onClick ~(handler open-path file.id file.title)
     (node :width 2)
     (label :icon file :textColor @color.muted :fitText true)
     (label :id ~(str "welcome-recent-" fi "-name") :text ~file.title :textColor @color.accent :fitText true :height 20)
     (label :text ~(or-else file.detail "") :textColor @color.muted :fitText true)
     (node :grow 1)))

;;; The status bar

(defcomponent forgeStatusItem "An item of the status bar: its text or its icon, a button with a tooltip."
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

(defcomponent forgeStatusBar
  "The status bar: the server, the branch, the problems; the active file's place, indentation, line endings, language."
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
     ~(status-problems)
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

(defview status-problems "The errors and warnings in Problems, counted; a click shows them." []
  `(label#status-problems :role button
     :label ~(t "status-problems.label" {:errors computed.errors :warnings computed.warnings})
     :runs [~" "
            {:icon circle-x}
            ~(str " " computed.errors "  ")
            {:icon alert-triangle}
            ~(str " " computed.warnings)]
     :padding 7
     :height 20
     :fitText true
     :radius 5
     :cursor hand
     :textColor @color.statusText
     :tooltip ~(t "status-problems.tip")
     :hover {:color @color.hover}
     :onClick showProblems))

(defview status-language "The active file's language; a click changes it." []
  `(node#status-language :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:text ~(or computed.activeDoc.language (t "pick.plainText")) :tip ~(t "status-language.tip")}
     :onClick changeLanguageMode))

(defview status-indent "The spaces per indentation level; a click changes them." []
  `(node#status-indent :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:text ~(t "pick.spaces" {:n (or-else settings.editor.tabSize 4)}) :tip ~(t "status-indent.tip")}
     :onClick changeIndentation))

(defview status-encoding "The encoding, UTF-8: the only one there is yet." []
  `(node#status-encoding :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:text ~(t "status-encoding") :tip ~(t "status-encoding.tip")}
     :disabled true
     :cursor arrow
     :textColor @color.muted))

(defview status-remote "The document server's button: signing in, sharing and syncing." []
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

(defview status-position "The caret's line and column, and how much is selected; a click goes to a line." []
  `(node#status-position :use forgeStatusItem
     :when ~(and computed.activeDoc (= computed.activeDoc.kind "file"))
     :with {:tip ~(t "status-position.tip")
            :text ~(str (t "status-position" {:line state.caret.line :column state.caret.column})
                        (if (> state.caret.selected 0) (t "status-position.selected" {:n state.caret.selected}) ""))}
     :onClick goToLinePick))

;;; The notification centre

(defcomponent forgeNotificationCentre "The notification centre over the bell: the notices, newest first."
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
       (label#notification-centre-title :text ~(if (empty? state.notifications) (t "notifications.none") (t "notifications.title"))
         :textColor @color.muted
         :grow 1
         :padding 10)
       (node#notification-centre-clear :use forgeTitleButton
         :with {:icon list-x :tip ~(t "notification-centre-clear.tip")}
         :onClick clearNotifications)
       (node#notification-centre-hide :use forgeTitleButton
         :with {:icon chevron-down :tip ~(t "notification-centre-hide.tip")}
         :onClick hide-notifications)
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
