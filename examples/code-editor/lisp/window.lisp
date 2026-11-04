;;; window.lisp --- Quaderno's window: the title bar and its menus, the status bar, and the pickers over them

;;; Views

(defview title-bar "The title bar: the menus, the project's name and the file's path." []
  `(node#titleBar :dir ~state.dir :when ~(not state.zen) :anchors {:left 0 :right 0 :top 0} :height 26 :color @chrome
     ~(menu-bar)
     (label#windowTitle :font small
       :text ~(str (if computed.file (str computed.file.name "  ·  ") "") state.project)
       :textColor @muted
       :anchors {:left 520 :right 44 :top 0 :bottom 0}
       :align {:horizontal end :vertical center})
     (label#themeSwitch :text ""
       :icon ~(if (= computed.theme "dark") "moon" "sun")
       :font small
       :anchors {:right 10 :top 3}
       :width 24
       :height 20
       :align center
       :radius 4
       :textColor @muted
       :hover {:textColor @accent :color @hover}
       :cursor hand
       :tooltip ~(t "bar.lightOrDarkSelectTheme")
       :onClick toggleTheme)))

(defview menu-bar "The menu bar: File, Edit, Selection, View, Find, Go, Tools, Project, Window and Help." []
  `(node#menuBar :use menuBar
     :anchors {:left 4 :top 0}
     :width 500
     :with {:menus [{:name file :title ~(t "bar.file") :width 40 :items ~menus.file}
                    {:name edit :title ~(t "bar.edit") :width 40 :items ~menus.edit}
                    {:name selection :title ~(t "bar.selection") :width 70 :items ~menus.selection}
                    {:name find :title ~(t "bar.find") :width 40 :items ~menus.find}
                    {:name view :title ~(t "bar.view") :width 44 :items ~menus.view}
                    {:name go :title ~(t "bar.go") :width 32 :items ~menus.go}
                    {:name terminal :title ~(t "bar.tools") :width 46 :items ~menus.terminal}
                    {:name project :title ~(t "bar.project") :width 58 :items ~menus.project}
                    {:name window :title ~(t "bar.window") :width 62 :items ~menus.window}
                    {:name help :title ~(t "bar.help") :width 42 :items ~menus.help}]
            :open state.menuBar
            :width 260
            :color @chrome
            :textColor @text
            :dimColor @muted
            :highlight @rowSelected}))

(defview status-bar "The status bar: the problems, the caret, the file's size, its syntax and the theme." []
  `(node#statusBar :dir ~state.dir :when ~(not state.zen) :anchors {:left 0 :right 0 :bottom 0} :height 24
     :color @sidebar
     (node#statusEdge :anchors {:left 0 :right 0 :top 0} :height 1 :color @border)
     (label#panelSwitcher :text ""
       :icon square-terminal
       :font small
       :anchors {:left 6 :top 2}
       :width 22
       :height 20
       :align center
       :radius 4
       :color transparent
       :textColor ~(if state.panelOpen "@accent" "@muted")
       :hover {:textColor @text :color @hover}
       :cursor hand
       :tooltip ~(t "bar.showOrHideThePanel")
       :onClick togglePanel)
     (node#statusText :style statusItem
       :text main
       :icon git-branch
       :textColor @faint
       :anchors {:left 36}
       :tooltip ~(t "bar.branchSwitchingBranchesIsnT"))
     ~(status-problems)
     ~(status-size)
     (node#statusIndent :style statusItem
       :text ~(if computed.file (t "bar.spaces4") "")
       :textColor @faint
       :anchors {:right 140}
       :tooltip ~(t "bar.indentation4SpacesChangingIt"))
     (node#statusSyntax :style statusItem
       :text ~(if computed.file (or computed.file.language "Plain Text") "")
       :hover {:textColor @text}
       :anchors {:right 16}
       :cursor hand
       :tooltip ~(t "bar.selectTheSyntax")
       :onClick selectSyntax)))

(defview status-size "Where the caret is, and how much is selected." []
  `(node#statusSize :style statusItem
     :text ~(if computed.file
                (if state.caret.selected
                  (t "status.caretSelected" {:line (text state.caret.line) :column (text state.caret.column)
                                             :n state.caret.selected})
                  (t "status.caret" {:line (text state.caret.line) :column (text state.caret.column)}))
                "")
     :hover {:textColor @text}
     :anchors {:right 236}
     :cursor hand
     :tooltip ~(t "bar.goToLineCtrlG")
     :onClick goToLine))

(defview status-problems "How many errors and warnings: a click shows them." []
  `(node#statusProblems :style statusItem
     :text ~(str computed.errors
                 " "
                 (if (= computed.errors 1) "error" "errors")
                 ", "
                 computed.warnings
                 " "
                 (if (= computed.warnings 1) "warning" "warnings"))
     :textColor ~(if (> computed.errors 0) "@error" "@muted")
     :hover {:textColor @text}
     :anchors {:left 104}
     :cursor hand
     :tooltip ~(t "bar.showTheProblemsCmdShift")
     :onClick showProblems))

(defview quick-pick "The Quick Switcher: files, file@symbol and file:line, as you type." []
  `(node#quickPick :when ~state.quickPick.open
     :use quickPick
     :with {:stacked true
            :rowHeight 44
            :detailFont small
            :color @menu
            :textColor @text
            :dimColor @muted
            :accent @accent
            :highlight @rowSelected
            :field @input
            :providers [{:items ~(if (= computed.gotoKind "") computed.picks computed.gotoItems)
                         :filter ~(= computed.gotoKind "")
                         :run goToPlace
                         :placeholder ~(t "bar.goToAFileThen")}
                        {:prefix @
                         :items ~computed.gotoItems
                         :filter false
                         :run goToPlace
                         :placeholder ~(t "bar.goToASymbolIn")}
                        {:prefix ":" :items ~computed.gotoItems :filter false :run goToPlace
                         :placeholder ~(t "bar.goToALine")}
                        {:prefix "#"
                         :items ~computed.gotoItems
                         :filter false
                         :run goToPlace
                         :placeholder ~(t "bar.searchTheFile")}]}))

(defview theme-pick "The theme picker: dark, light or the system's." []
  `(node#themePick :when ~state.themePick.open
     :use quickPick
     :with {:name themePick
            :stacked true
            :rowHeight 44
            :detailFont small
            :color @menu
            :textColor @text
            :dimColor @muted
            :accent @accent
            :highlight @rowSelected
            :field @input
            :providers [{:items ~computed.themes
                         :filter false
                         :run setTheme
                         :placeholder ~(t "bar.selectAThemeTheArrows")}]}))

;;; Dialogs, menus and pickers over the window

(defview close-dialog "The dialog that asks whether to save a file before its tab closes." []
  `(node#closeDialog :use dialog
     :with {:open state.closing.open
            :title ~(t "dialog.saveChanges"
                       {:name (if (>= state.closing.file 0) (get-in state.files [state.closing.file :name]) "")})
            :message ~(t "ui.yourChangesWillBeLost")
            :primaryLast true
            :width 470
            :height 124
            :color @menu
            :textColor @text
            :dimColor @muted
            :highlight @accent
            :backdrop @backdrop
            :buttons [{:title ~(t "ui.save") :run saveAndClose}
                      {:title ~(t "ui.cancel")}
                      {:title ~(t "ui.donTSave") :run discardAndClose}]}))

(defview about-dialog "About Quaderno." []
  `(node#aboutDialog :use dialog
     :with {:open state.aboutOpen
            :title "Quaderno"
            :message ~(t "ui.aCodeEditorWrittenAs")
            :width 460
            :height 170
            :color @menu
            :textColor @text
            :dimColor @muted
            :highlight @selection
            :backdrop @backdrop
            :buttons [{:title ~(t "ui.ok")}]}))

(defview command-palette "The palette of every command." []
  `(node#palette :when ~state.palette.open
     :use quickPick
     :with {:name palette
            :color @menu
            :textColor @text
            :dimColor @muted
            :accent @accent
            :highlight @rowSelected
            :field @input
            :providers [{:items ~computed.palette :placeholder ""}]}))

(defview symbol-pick "Find Symbol in Project: every symbol of the files, as you type." []
  `(node#symbolPick :when ~state.symbolPick.open
     :use quickPick
     :with {:name symbolPick
            :stacked true
            :rowHeight 44
            :detailFont small
            :color @menu
            :textColor @text
            :dimColor @muted
            :accent @accent
            :highlight @rowSelected
            :field @input
            :providers [{:items ~computed.projectSymbols :run goToPlace :placeholder ~(t "ui.goToASymbolIn")}]}))

(defview tab-menu "A tab's context menu." []
  `(node#tabMenu :use contextMenu
     :with {:open state.tabMenu.open
            :x ~state.tabMenu.x
            :y ~state.tabMenu.y
            :items [{:title ~(t "ui.close") :run close :with {:file ~state.tabMenu.file}}
                    {:title ~(t "ui.closeOthers")
                     :run closeOthersOf
                     :with {:file ~state.tabMenu.file}
                     :enabled ~(> (count state.tabs) 1)}
                    {:title ~(t "ui.closeTabsToTheRight")
                     :run closeToTheRight
                     :with {:file ~state.tabMenu.file}
                     :enabled ~(< (indexOf state.tabs state.tabMenu.file) (- (count state.tabs) 1))}
                    {:separator true}
                    {:title ~(t "ui.reopenClosedFile") :run reopenClosed :enabled ~(> (count state.closed) 0)}]}))

(defview editor-menu "The editor's context menu." []
  `(node#editorMenu :use contextMenu
     :with {:open state.editorMenu.open
            :x ~state.editorMenu.x
            :y ~state.editorMenu.y
            :items [{:title ~(t "ui.cut") :run cut :enabled ~(not (or-else computed.file.readOnly false))}
                    {:title ~(t "ui.copy") :run copy}
                    {:title ~(t "ui.paste") :run paste :enabled ~(not (or-else computed.file.readOnly false))}
                    {:separator true}
                    {:title ~(t "ui.selectAll") :run selectAll}
                    {:title ~(t "ui.toggleComment") :run toggleComment}
                    {:separator true}
                    {:title ~(t "ui.find") :run openFind}
                    {:title ~(t "ui.syntax") :run selectSyntax}]}))

(defview syntax-pick "The syntax picker: the language of the file shown." []
  `(node#syntaxPick :when ~state.syntaxPick.open
     :use quickPick
     :with {:name syntaxPick
            :color @menu
            :textColor @text
            :dimColor @muted
            :accent @accent
            :highlight @rowSelected
            :field @input
            :providers [{:items ~computed.syntaxes :run setSyntax :placeholder ~(t "ui.selectTheSyntaxOfThe")}]}))
