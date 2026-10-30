{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's shell: the title bar with a command centre, the activity bar, the explorer side bar, editor groups with tabs, the panel and the status bar. The layout (what is shown, the sash positions) is in state.layout, kept by persist; zen mode hides everything but the editors. Other features contribute commands, menus, keys, settings, tokens and components (examples/forge/README.md)."
  :state {:layout {:sidebar true
      :panel false
      :panelMax false
      :zen false
      :secondary false
      :sizes [240]
      :panelSizes [480]
      :groupSizes []}
    :folder {:root ""
      :files []
      :truncated false
      :skip "**/node_modules/**,**/build/**,**/out/**,**/dist/**,**/target/**,**/__pycache__/**,**/.*/**,**/.*"}
    :explorer {:expanded [] :selected nil}
    :docs []
    :groups [{:tabs [] :active nil}]
    :activeGroup 0
    :caret {:line 1 :column 1 :selected 0}
    :menuBar {}
    :about false
    :sideView explorer
    :nav {:back [] :forward [] :moving false}
    :notificationCentre false
    :languages {:c "C++"
      :cc "C++"
      :cpp "C++"
      :h "C++"
      :hpp "C++"
      :json JSON
      :md Markdown
      :cmake CMake
      :py Python
      :js JavaScript
      :mjs JavaScript
      :ts TypeScript
      :tsx TypeScript
      :sh Shell
      :bash Shell
      :zsh Shell}}
  :computed {:docIds "{{map(state.docs, it.id)}}"
    :dirtyIds "{{map(filter(state.docs, it.dirty), it.id)}}"
    :activeDoc "{{find(state.docs, it.id == state.groups[state.activeGroup].active)}}"
    :folderName "{{empty(state.folder.root) ? 'No Folder Opened' : reverse(split(state.folder.root, '/'))[0]}}"
    :welcomeRecent "{{empty(state.folder.root) ? [] : slice(filter(state.quickPick.recent ?? [], contains(map(state.folder.files ?? [], it.id), it.id)), 0, 5)}}"}
  :actions {:toggleSidebar {:title "Toggle Left Side Bar"
      :category View
      :icon panel-left
      :do [{:toggle state.layout.sidebar}]}
    :forgeShowView [{:if "{{state.sideView == view && state.layout.sidebar}}"
        :then [{:set state.layout.sidebar :to false}]
        :else [{:set state.sideView :to "{{view}}"} {:set state.layout.sidebar :to true}]}]
    :showExplorer {:title "Show Explorer" :category View :icon files :do [{:run forgeShowView :with {:view explorer}}]}
    :showSearch {:title "Show Search" :category View :icon search :do [{:run forgeShowView :with {:view search}}]}
    :navigateBack {:title "Go Back"
      :category Go
      :icon arrow-left
      :enablement "{{length(state.nav.back) > 0}}"
      :do [{:set state.nav.forward
          :to "{{concat(filter(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active), state.nav.forward)}}"}
        {:run forgeNavigateTo :with {:tab "{{state.nav.back[length(state.nav.back) - 1]}}"}}
        {:set state.nav.back :to "{{slice(state.nav.back, 0, length(state.nav.back) - 1)}}"}]}
    :navigateForward {:title "Go Forward"
      :category Go
      :icon arrow-right
      :enablement "{{length(state.nav.forward) > 0}}"
      :do [{:set state.nav.back
          :to "{{concat(state.nav.back, filter(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active))}}"}
        {:run forgeNavigateTo :with {:tab "{{state.nav.forward[0]}}"}}
        {:set state.nav.forward :to "{{slice(state.nav.forward, 1)}}"}]}
    :forgeNavigateTo [{:note "tab: { id, label }, shown without being recorded in the history"}
      {:set state.nav.moving :to true}
      {:run showTab :with (node #"{{tab.id}}" :label "{{tab.label}}")}
      {:set state.nav.moving :to false}]
    :togglePanel {:title "Toggle Panel"
      :category View
      :icon panel-bottom
      :do [{:toggle state.layout.panel} {:set state.layout.panelMax :to false}]}
    :toggleMaximizedPanel {:title "Maximize Panel Size"
      :category View
      :icon chevron-up
      :do [{:set state.layout.panel :to true} {:toggle state.layout.panelMax}]}
    :toggleZen {:title "Toggle Zen Mode"
      :category View
      :icon maximize
      :description "Hide everything but the editors; run it again to bring the workbench back."
      :do [{:toggle state.layout.zen}]}
    :openFolder {:title "Open Folder..."
      :category File
      :icon folder-open
      :do [{:chooseFolder workspace
          :title "Open Folder"
          :onError [{:run notify :with {:severity error :message "{{'No folder opened: ' + error.message}}"}}]
          :onDone [{:set state.explorer :to "{{ {expanded: [], selected: null} }}"}
            {:set state.explorerTree :to "{{ {entries: [], listed: []} }}"}
            {:set state.folder.root :to "{{result.name}}"}
            {:run explorerRefresh}
            {:run indexFolder}
            {:run log :with {:text "{{'Opened ' + result.name}}"}}]}]}
    :indexFolder [{:note "Lists the folder's files for Go to File"}
      {:invoke search.files
        :with {:exclude "{{state.folder.skip}}"}
        :onDone [{:run indexed :with {:files "{{result.files}}"}}
          {:set state.folder.truncated :to "{{result.truncated}}"}]
        :onError [{:set state.folder.files :to []}]}]
    :indexed [{:set state.folder.files
        :to "{{map(files, {id: it, title: reverse(split(it, '/'))[0], detail: join(slice(split(it, '/'), 0, -1), '/')})}}"}]
    :openExplorerItem [{:if "{{!empty(event.id) && (find(state.explorerTree.entries, it.path == event.id) ?? {}).kind != 'folder'}}"
        :then [{:run openPath :with {:path "{{event.id}}" :label "{{reverse(split(event.id, '/'))[0]}}"}}]}]
    :forgeRecordNav [{:note "id: the tab about to be shown. The tab shown now goes into the history Go Back walks (the last 50), unless Go Back or Go Forward is moving"}
      {:if "{{!state.nav.moving && !empty(state.groups[state.activeGroup].active) && state.groups[state.activeGroup].active != id}}"
        :then [{:set state.nav
            :to "{{ {back: slice(concat(state.nav.back, filter(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active)), max(0, length(state.nav.back) - 49)), forward: [], moving: false} }}"}]}]
    :showTab [{:run forgeRecordNav :with (node #"{{id}}")}
      {:if "{{!some(state.groups[state.activeGroup].tabs, it.id == id)}}"
        :then [{:push "state.groups[state.activeGroup].tabs" :value "{{ {id: id, label: label} }}"}]}
      {:set "state.groups[state.activeGroup].active" :to "{{id}}"}]
    :openPage [{:if "{{!contains(computed.docIds, id)}}"
        :then [{:push state.docs
            :value "{{ {id: id, label: label, kind: kind, text: '', dirty: false, language: ''} }}"}]}
      {:run showTab :with (node #"{{id}}" :label "{{label}}")}]
    :closeEditor {:title "Close Editor"
      :category View
      :icon x
      :enablement "{{!empty(state.groups[state.activeGroup].active)}}"
      :do [{:run closeTab :with (node :gi "{{state.activeGroup}}" #"{{state.groups[state.activeGroup].active}}")}]}
    :splitEditor {:title "Split Editor"
      :category View
      :icon columns-2
      :description "Open the active editor beside the current one (two groups at most)."
      :enablement "{{length(state.groups) < 2 && !empty(state.groups[state.activeGroup].active)}}"
      :do [{:push state.groups
          :value "{{ {tabs: filter(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active), active: state.groups[state.activeGroup].active} }}"}
        {:set state.activeGroup :to "{{length(state.groups) - 1}}"}]}
    :about {:title "About Forge" :category Help :icon info :do [{:set state.about :to true}]}}
  :keys {:"mod+b" toggleSidebar
    :"mod+j" togglePanel
    :"mod+k z" toggleZen
    :"mod+o" openFolder
    :"mod+w" closeEditor
    :"mod+\\" splitEditor
    :"ctrl+minus" navigateBack
    :"ctrl+shift+minus" navigateForward}
  :menus {:file {:openFolder {:run openFolder :group "1_open" :order 1}
      :reindex {:run reindex :group "1_open" :order 2}
      :closeEditor {:run closeEditor :group "2_close" :order 1}}
    :view {:sidebar {:run toggleSidebar :group "2_layout" :order 1}
      :panel {:run togglePanel :group "2_layout" :order 2}
      :split {:run splitEditor :group "2_layout" :order 3}
      :zen {:run toggleZen :group "2_layout" :order 4}}
    :go {:back {:run navigateBack :group "0_history" :order 1}
      :forward {:run navigateForward :group "0_history" :order 2}}
    :help {:keybindings {:run openKeybindings :group "1_help" :order 1} :about {:run about :group "2_about" :order 1}}}
  :components {:forgeTitleBar {:node (row #title-bar
        :role banner
        :label "Title Bar"
        :width "100%"
        :height 38
        :color @color.titleBar
        :alignItems center
        (node :when "{{!window.nativeMenuBar}}"
          :use menuBar
          #menu-bar
          :width 330
          :with {:open state.menuBar
            :menus "{{[{name: 'file', title: 'File', items: menus.file}, {name: 'view', title: 'View', items: menus.view}, {name: 'go', title: 'Go', items: menus.go}, {name: 'preferences', title: 'Preferences', items: menus.preferences}, {name: 'help', title: 'Help', items: menus.help}]}}"})
        (node #title-left :grow 1)
        (node :use forgeTitleButton
          #title-back
          :with {:icon arrow-left :tip "Go Back (Ctrl+-)" :on "{{length(state.nav.back) > 0}}"}
          :onClick {:run navigateBack})
        (node :use forgeTitleButton
          #title-forward
          :with {:icon arrow-right :tip "Go Forward (Ctrl+Shift+-)" :on "{{length(state.nav.forward) > 0}}"}
          :onClick {:run navigateForward})
        (node #title-centre-gap :width 4)
        (row #command-centre
          :role button
          :label "Search Files"
          :width 380
          :height 26
          :color @color.input
          :radius 7
          :borderWidth 1
          :borderColor @color.border
          :alignItems center
          :gap 6
          :cursor hand
          :tooltip "Search files (> for commands, : for a line)"
          :hover {:color @color.hover}
          :onClick {:run quickOpen}
          (node #command-centre-start :width 6)
          (label #command-centre-icon :icon search :textColor @color.muted :fitText true)
          (label #command-centre-text
            :text "{{'Search ' + computed.folderName}}"
            :textColor @color.muted
            :fitText true
            :height 20)
          (node #command-centre-end :grow 1)
          (label #command-centre-hint
            :text "Cmd+P"
            :textColor @color.muted
            :fitText true
            :height 18
            :padding 6
            :radius 4
            :borderWidth 1
            :borderColor @color.border)
          (node #command-centre-hint-end :width 4))
        (node #title-right :grow 1)
        (node :use forgeTitleButton
          #title-sidebar
          :with {:icon panel-left :tip "Toggle Left Side Bar (Cmd+B)" :on "{{state.layout.sidebar}}"}
          :onClick {:run toggleSidebar})
        (node :use forgeTitleButton
          #title-panel
          :with {:icon panel-bottom :tip "Toggle Panel (Cmd+J)" :on "{{state.layout.panel}}"}
          :onClick {:run togglePanel})
        (node :use forgeTitleButton
          #title-secondary
          :with {:icon panel-right :tip "Toggle Outline Bar (Cmd+Alt+B)" :on "{{state.layout.secondary}}"}
          :onClick {:run toggleSecondarySidebar})
        (node #title-end :width 8))}
    :forgeTitleButton {:params {:icon "" :tip "" :on false}
      :node {:type label
        :role button
        :label "{{tip}}"
        :icon "{{icon}}"
        :font icons
        :width 28
        :height 26
        :align center
        :radius 7
        :cursor hand
        :textColor "{{on ? '@color.text' : '@color.muted'}}"
        :tooltip "{{tip}}"
        :hover {:color @color.hover}}}
    :forgeActivityItem {:params {:view "" :icon "" :label "" :tip "" :badge 0}
      :node (node #"activity-{{view}}"
        :role tab
        :label "{{label}}"
        :selected "{{state.layout.sidebar && state.sideView == view}}"
        :width 44
        :height 42
        :color @color.activityBar
        :cursor hand
        :tooltip "{{tip}}"
        (node #"activity-{{view}}-indicator"
          :anchors {:left 5 :top 4 :right 5 :bottom 4}
          :radius 8
          :color "{{state.layout.sidebar && state.sideView == view ? '@color.selection' : '@color.activityBar'}}")
        {:type label
          :icon "{{icon}}"
          :font iconsRail
          :anchors {:left 5 :top 4 :right 5 :bottom 4}
          :align center
          :radius 8
          :textColor "{{state.layout.sidebar && state.sideView == view ? '@color.accent' : '@color.muted'}}"
          :hover {:textColor "{{state.layout.sidebar && state.sideView == view ? '@color.accent' : '@color.text'}}"
            :borderWidth 1
            :borderColor @color.border}}
        (label #"activity-{{view}}-badge"
          :when "{{badge > 0}}"
          :text "{{badge > 99 ? '99+' : text(badge)}}"
          :anchors {:right 2 :top 1}
          :height 15
          :padding 4
          :fitText true
          :radius 8
          :align center
          :color @color.accent
          :textColor "#ffffff"))}
    :forgeActivityBar {:node (column #activity-bar
        :role tablist
        :label "View Rail"
        :width 44
        :height "100%"
        :color @color.activityBar
        (node #activity-top :height 4)
        {:use forgeActivityItem
          :with {:view explorer :icon folder-tree :label Explorer :tip "Explorer (Cmd+Shift+E)"}
          :onClick {:run showExplorer}}
        {:use forgeActivityItem
          :with {:view search :icon text-search :label Search :tip "Search (Cmd+Shift+F)"}
          :onClick {:run showSearch}}
        {:use forgeActivityItem
          :with {:view scm
            :icon git-fork
            :label "Source Control"
            :tip "Source Control (Cmd+Shift+G)"
            :badge "{{state.scm.status.count}}"}
          :onClick {:run showSourceControl}}
        {:use forgeActivityItem
          :with {:view debug :icon bug-play :label Debugger :tip "Debugger (Cmd+Shift+D)"}
          :onClick {:run showDebug}}
        {:use forgeActivityItem
          :with {:view features :icon puzzle :label Features :tip "Features (Cmd+Shift+X)"}
          :onClick {:run showFeatures}}
        {:use forgeActivityItem
          :with {:view agent :icon sparkles :label Agent :tip "Agent (Cmd+Alt+I)"}
          :onClick {:run showAgent}}
        (node #activity-gap :grow 1)
        {:use forgeAccountsStatus}
        (label #activity-settings
          :role button
          :label Settings
          :icon settings
          :font iconsRail
          :width 44
          :height 42
          :align center
          :cursor hand
          :tooltip "Settings (Cmd+,)"
          :textColor @color.muted
          :hover {:textColor @color.text}
          :onClick {:run openSettings})
        (node #activity-bottom :height 4))}
    :forgeViewTitle {:params {:text ""}
      :node {:type label :text "{{text}}" :textColor @color.muted :width "100%" :height 35 :padding 12}}
    :forgeSideBar {:node (column #side-bar
        :role complementary
        :label "Left Side Bar"
        :point sidebar.views
        :color @color.sideBar
        {:when "{{state.sideView == 'scm'}}" :use forgeScmView}
        {:when "{{state.sideView == 'debug'}}" :use forgeDebugView}
        {:when "{{state.sideView == 'agent'}}" :use forgeAgentView}
        {:when "{{state.sideView == 'features'}}" :use forgeFeaturesView}
        {:when "{{state.sideView == 'search'}}" :use forgeSearchPage}
        {:when "{{state.sideView == 'explorer'}}" :use forgeExplorerTitle}
        (label #side-bar-folder
          :when "{{state.sideView == 'explorer' && !empty(state.folder.root)}}"
          :icon chevron-down
          :text "{{upper(computed.folderName)}}"
          :textColor @color.text
          :width "100%"
          :height 22
          :padding 6)
        {:when "{{state.sideView == 'explorer' && !empty(state.folder.root)}}" :use forgeExplorer}
        (column #side-bar-empty
          :when "{{state.sideView == 'explorer' && empty(state.folder.root)}}"
          :width "100%"
          :alignItems center
          :gap 4
          (label #side-bar-empty-text
            :text "You have not yet opened a folder."
            :textColor @color.text
            :width "100%"
            :padding 12
            :wrap true)
          (label #side-bar-open
            :text "Open Folder"
            :width 200
            :height 28
            :align center
            :color @color.accent
            :textColor "#ffffff"
            :radius 6
            :cursor hand
            :focusable true
            :onClick {:run openFolder})))}
    :forgeWelcome {:node (row #welcome :width "100%" :grow 1 :color @color.editor
        (node #welcome-margin :grow 1)
        (column #welcome-body :width 400 :height "100%" :gap 8
          (node #welcome-top :height 96)
          (node #welcome-mark :width 36 :height 4 :radius 2 :color @color.accent)
          (label #welcome-title :text Forge :font title :textColor @color.text :fitText true)
          (label #welcome-subtitle
            :text "A workbench written in JSON, changed live."
            :textColor @color.muted
            :fitText true)
          (node #welcome-gap :height 12)
          (label #welcome-start :text "Quick actions" :textColor @color.muted :fitText true)
          (label #welcome-open
            :role button
            :label "Open Folder..."
            :icon folder-open
            :text "Open Folder..."
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
            :tooltip "Open a folder to work in"
            :hover {:color @color.hover :borderColor @color.accent}
            :focused {:borderColor @color.accent}
            :onClick {:run openFolder})
          (label #welcome-files
            :role button
            :label "Find a File..."
            :icon file-search
            :text "Find a File..."
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
            :tooltip "Cmd+P"
            :hover {:color @color.hover :borderColor @color.accent}
            :focused {:borderColor @color.accent}
            :onClick {:run quickOpen})
          (label #welcome-commands
            :role button
            :label "Run a Command..."
            :icon command
            :text "Run a Command..."
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
            :tooltip "Cmd+Shift+P"
            :hover {:color @color.hover :borderColor @color.accent}
            :focused {:borderColor @color.accent}
            :onClick {:run showCommands})
          (label #welcome-theme
            :role button
            :label "Color Theme"
            :icon palette
            :text "Color Theme"
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
            :tooltip "Choose a colour theme"
            :hover {:color @color.hover :borderColor @color.accent}
            :focused {:borderColor @color.accent}
            :onClick {:run pickTheme})
          (label #welcome-settings
            :role button
            :label Settings
            :icon settings
            :text Settings
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
            :tooltip "Cmd+,"
            :hover {:color @color.hover :borderColor @color.accent}
            :focused {:borderColor @color.accent}
            :onClick {:run openSettings})
          (node #welcome-recent-gap :when "{{!empty(computed.welcomeRecent)}}" :height 12)
          (label #welcome-recent
            :when "{{!empty(computed.welcomeRecent)}}"
            :text "Recent files"
            :textColor @color.muted
            :fitText true)
          (label :repeat {:for file :in "{{computed.welcomeRecent}}" :index fi :key "{{file.id}}"}
            #"{{'welcome-recent-' + fi}}"
            :role link
            :label "{{'Open ' + file.id}}"
            :icon file
            :text "{{file.title}}"
            :width "100%"
            :height 26
            :padding 6
            :radius 6
            :textColor @color.accent
            :focusable true
            :cursor hand
            :tooltip "{{file.id}}"
            :hover {:color @color.hover}
            :focused {:color @color.hover}
            :onClick {:run openPath :with {:path "{{file.id}}" :label "{{file.title}}"}}))
        (node #welcome-end :grow 1))}
    :forgeStatusItem {:params {:text "" :icon "" :tip ""}
      :node {:type label
        :role button
        :label "{{tip}}"
        :text "{{text}}"
        :icon "{{icon}}"
        :padding 7
        :height 20
        :fitText true
        :radius 5
        :cursor hand
        :textColor @color.statusText
        :tooltip "{{tip}}"
        :hover {:color @color.hover}}}
    :forgeStatusBar {:node (row #status-bar
        :role status
        :label "Status Bar"
        :point statusBar.right
        :width "100%"
        :height 28
        :color @color.statusBar
        :alignItems center
        :gap 2
        (node #status-start :width 6)
        (label #status-remote
          :role button
          :label "Document Server"
          :icon cloud
          :width 28
          :height 20
          :radius 5
          :align center
          :cursor hand
          :textColor @color.accent
          :hover {:color @color.hover}
          :tooltip "Document Server: sign in to share and sync"
          :onClick {:run accountsMenu})
        (node :use forgeStatusItem
          #status-branch
          :when "{{!empty(state.scm.status.branch.head)}}"
          :with {:icon git-branch :text "{{computed.scmBranchText}}" :tip "Show Branches"}
          :onClick {:run scmBranches})
        (node :use forgeStatusItem
          #status-sync
          :when "{{!empty(state.scm.status.branch.head)}}"
          :with {:icon refresh-cw :tip "Refresh Source Control"}
          :onClick {:run scmRefresh})
        (label #status-problems
          :role button
          :label "{{plural(computed.errors, 'error') + ', ' + plural(computed.warnings, 'warning')}}"
          :runs ["{{' '}}"
            {:icon circle-x}
            "{{' ' + text(computed.errors) + '  '}}"
            {:icon alert-triangle}
            "{{' ' + text(computed.warnings)}}"]
          :padding 7
          :height 20
          :fitText true
          :radius 5
          :cursor hand
          :textColor @color.statusText
          :tooltip "Problems (Cmd+Shift+M)"
          :hover {:color @color.hover}
          :onClick {:run showProblems})
        (node #status-gap :grow 1)
        (node :use forgeStatusItem
          #status-position
          :when "{{computed.activeDoc && computed.activeDoc.kind == 'file'}}"
          :with {:tip "Go to Line"
            :text "{{'Ln ' + text(state.caret.line) + ', Col ' + text(state.caret.column) + (state.caret.selected > 0 ? ' (' + text(state.caret.selected) + ' selected)' : '')}}"}
          :onClick {:run goToLinePick})
        (node :use forgeStatusItem
          #status-indent
          :when "{{computed.activeDoc && computed.activeDoc.kind == 'file'}}"
          :with {:text "{{'Spaces: ' + text(settings.editor.tabSize ?? 4)}}" :tip "Select Indentation"}
          :onClick {:run changeIndentation})
        (node :use forgeStatusItem
          #status-encoding
          :when "{{computed.activeDoc && computed.activeDoc.kind == 'file'}}"
          :with {:text UTF-8 :tip "Encoding: UTF-8. Other encodings aren't available yet"}
          :disabled true
          :cursor arrow
          :textColor @color.muted)
        (node :use forgeStatusItem
          #status-eol
          :when "{{computed.activeDoc && computed.activeDoc.kind == 'file'}}"
          :with {:text "{{computed.activeDoc.eol ?? 'LF'}}" :tip "Select End of Line Sequence"}
          :onClick {:run changeEndOfLine})
        (node :use forgeStatusItem
          #status-language-status
          :when "{{computed.activeDoc && computed.activeDoc.kind == 'file'}}"
          :with {:icon braces :tip "Language Status"}
          :onClick {:run showOutput})
        (node :use forgeStatusItem
          #status-language
          :when "{{computed.activeDoc && computed.activeDoc.kind == 'file'}}"
          :with {:text "{{computed.activeDoc.language || 'Plain Text'}}" :tip "Select Language Mode"}
          :onClick {:run changeLanguageMode})
        (node :use forgeStatusItem
          #status-notifications
          :with {:icon "{{length(state.notifications) > 0 ? 'bell-dot' : 'bell'}}" :tip Notifications}
          :onClick {:run showNotifications})
        (node #status-end :width 6))}
    :forgeNotificationCentre {:node (column #notification-centre
        :role dialog
        :label Notifications
        :width 450
        :height "{{40 + max(1, min(6, length(state.notifications))) * 28}}"
        :color @color.widget
        :borderWidth 1
        :borderColor @color.border
        :radius 8
        :overlay {:anchor "#status-notifications" :placement above-end :open state.notificationCentre :dismiss outside}
        (row #notification-centre-head :width "100%" :height 34 :alignItems center
          (label #notification-centre-title
            :text "{{empty(state.notifications) ? 'NO NEW NOTIFICATIONS' : 'NOTIFICATIONS'}}"
            :textColor @color.muted
            :grow 1
            :padding 10)
          (node :use forgeTitleButton
            #notification-centre-clear
            :with {:icon list-x :tip "Clear All Notifications"}
            :onClick {:run clearNotifications})
          (node :use forgeTitleButton
            #notification-centre-hide
            :with {:icon chevron-down :tip "Hide Notifications"}
            :onClick {:set state.notificationCentre :to false})
          (node #notification-centre-end :width 4))
        (list #notifications
          :label Notifications
          :when "{{!empty(state.notifications)}}"
          :width "100%"
          :grow 1
          :items "{{computed.notificationRows}}"
          :rowHeight 28
          :color @color.widget
          :textColor @color.text
          :selectedBackground @color.selection))}}}
