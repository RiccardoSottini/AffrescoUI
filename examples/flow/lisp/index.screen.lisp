;;; index.screen.lisp --- Flow: an issue tracker with a kanban board
;;;
;;; Boards are rows of the sidebar, next to the inbox, the sprints and the projects; the board, a grouped list,
;;; swimlanes by member, a table of every card or a timeline of due dates shares a split with the chosen card's
;;; drawer, or the board's insights. Drag a card to another column, use the drawer's arrows, Alt and the arrows, or
;;; right-click it for its menu; double-click it or press Cmd+Enter to open it as a page, Enter to edit it. Cmd+P goes
;;; to a card, Cmd+Shift+P runs a command, Cmd+N types a new card, Cmd+F searches, Cmd+Z undoes, Cmd+J switches light
;;; and dark, Cmd+L the language, Cmd+, opens the settings, Escape closes the issue page, then clears the filters.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "state.lisp"
         "computed.lisp"
         "look.lisp"
         "keys.lisp"
         "toolbar.lisp"
         "columns.lisp"
         "dialogs.lisp"
         "overlays.lisp"
         "../../shared/components/charts.lisp"
         "board.lisp"
         "drawer.lisp"
         "views.lisp"
         "sidebar.lisp"
         "list.lisp"
         "issue.lisp"
         "pages.lisp"
         "select.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "flow.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Flow's one window: the bars, the work area, the sidebar and what opens over them."
  :window {:title "Flow: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
  :strings {:default en :locale ~settings.appearance.language}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons symbols]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [iconsSmall]}
          :tiny {:file "../../shared/fonts/Roboto-Regular.ttf" :size 9}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons symbols]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 20 :fallback [iconsLarge]}
          :display {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 15}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 20}
          :symbols {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 13}}
  :persist [state.theme state.sizes state.view state.swimlanes state.settings]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#statusBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~state.theme
  (node#panel-root :color @color.sidebar
    ~(top-bar)
    ~(toolbar)
    ~(work-area)
    ~(status-bar)
    ~(issue-page)
    ~(context-menu "cardMenu" "state.cardMenu" 'menus.card)
    ~(context-menu "columnMenuList" "state.columnMenu" 'menus.column)
    ~(edit-dialog)
    (node#settings :use settingsSheet)
    ~(composer)
    ~(toast)
    ~(command-palette)
    ~(issue-picker)
    ~(property-picker)
    ~(page "inboxPage" "inbox")
    ~(page "cyclesPage" "cycles")
    ~(page "projectsPage" "projects")
    (node#sidebar :use sidebar)
    ~(selection-bar)))
