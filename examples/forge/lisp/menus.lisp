;;; menus.lisp --- Forge's menus: the menu bar's, and those a right-click opens
;;;
;;; Each item runs a command by name, in a group and an order; every feature adds its own here.

;;; Menus

(defmenu file
         "TODO"
         {:save {:run save :group "3_save" :order 1}
          :saveAll {:run saveAll :group "3_save" :order 2}
          :revertFile {:run revertFile :group "3_save" :order 3}
          :openFolder {:run openFolder :group "1_open" :order 1}
          :reindex {:run reindex :group "1_open" :order 2}
          :closeEditor {:run closeEditor :group "2_close" :order 1}
          :newFile {:run explorerNewFile :group "0_new" :order 1}
          :newFolder {:run explorerNewFolder :group "0_new" :order 2}
          :openRecent {:run openRecent :group "1_open" :order 3}
          :clearRecent {:run clearRecent :group "1_open" :order 4}})

(defmenu preferences
         "TODO"
         {:theme {:run pickTheme :group "2_themes" :order 1}
          :importTheme {:run importTheme :group "2_themes" :order 2}
          :icons {:run toggleIcons :group "2_themes" :order 3}
          :settings {:run openSettings :group "1_settings" :order 1}
          :keybindings {:run openKeybindings :group "1_settings" :order 2}
          :accountsSignIn {:run accountsSignIn :group "3_accounts" :order 1}
          :accountsSignOut {:run accountsSignOut :group "3_accounts" :order 2}
          :settingsJson {:run openSettingsJson :group "1_settings" :order 3}
          :keybindingsJson {:run openKeybindingsJson :group "1_settings" :order 4}})

(defmenu view
         "TODO"
         {:problems {:run showProblems :group "3_panels" :order 1}
          :output {:run showOutput :group "3_panels" :order 2}
          :notifications {:run showNotifications :group "3_panels" :order 3}
          :maximizePanel {:run toggleMaximizedPanel :group "3_panels" :order 4}
          :commands {:run showCommands :group "1_open" :order 1}
          :sidebar {:run toggleSidebar :group "2_layout" :order 1}
          :panel {:run togglePanel :group "2_layout" :order 2}
          :split {:run splitEditor :group "2_layout" :order 3}
          :zen {:run toggleZen :group "2_layout" :order 4}
          :terminal {:run showTerminal :group "3_panels" :order 4}
          :scm {:run showSourceControl :group "1_views" :order 2}
          :agent {:run showAgent :group "1_views" :order 3}
          :features {:run showFeatures :group "1_views" :order 4}
          :debug {:run showDebug :group "1_views" :order 3}
          :debugConsole {:run showDebugConsole :group "3_panels" :order 5}
          :toggleWordWrap {:run toggleWordWrap :group "4_editor" :order 1}
          :secondary {:run toggleSecondarySidebar :group "2_layout" :order 2}
          :moveNext {:run moveEditorToNextGroup :group "3_groups" :order 1}
          :movePrevious {:run moveEditorToPreviousGroup :group "3_groups" :order 2}
          :pin {:run pinEditor :group "3_groups" :order 3}
          :unpin {:run unpinEditor :group "3_groups" :order 4}
          :closeOthers {:run closeOtherEditors :group "3_groups" :order 5}
          :minimap {:run toggleMinimap :group "4_editor" :order 2}
          :breadcrumbs {:run toggleBreadcrumbs :group "4_editor" :order 3}})

(defmenu go
         "TODO"
         {:quickOpen {:run quickOpen :group "1_go" :order 1}
          :goToLine {:run goToLinePick :group "1_go" :order 2}
          :findInFiles {:run findInFiles :group "2_search" :order 1}
          :back {:run navigateBack :group "0_history" :order 1}
          :forward {:run navigateForward :group "0_history" :order 2}
          :definition {:run goToDefinition :group "2_symbols" :order 1}
          :references {:run findReferences :group "2_symbols" :order 2}
          :symbol {:run goToSymbol :group "2_symbols" :order 3}
          :workspaceSymbol {:run goToWorkspaceSymbol :group "2_symbols" :order 4}})

(defmenu searchResult "TODO" {:dismiss {:run searchDismiss :group "1_dismiss" :order 1}})

(defmenu help
         "TODO"
         {:keybindings {:run openKeybindings :group "1_help" :order 1} :about {:run about :group "2_about" :order 1}})

(defmenu explorer
         "TODO"
         {:newFile {:run explorerNewFile :group "1_new" :order 1}
          :newFolder {:run explorerNewFolder :group "1_new" :order 2}
          :reveal {:run explorerReveal :group "2_reveal" :order 1}
          :copy {:run explorerCopy :group "3_clipboard" :order 1}
          :paste {:run explorerPaste :group "3_clipboard" :order 2}
          :rename {:run explorerRename :group "4_edit" :order 1}
          :delete {:run explorerDelete :group "4_edit" :order 2}
          :refresh {:run explorerRefresh :group "5_refresh" :order 1}})
