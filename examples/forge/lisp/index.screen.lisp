;;; index.screen.lisp --- Forge: a workbench for code, written as a program
;;;
;;; The window names its fonts and its parts; each feature of features/<name>/ brings its state, functions,
;;; commands, keys, settings and views (<name>.lisp and views.lisp), and menus.lisp gathers their menus. The open
;;; folder is the host's workspace root (Open Folder chooses it): the files service reads and writes in it,
;;; fileChanges watches it and search searches it; the features service offers the example catalogue (catalog/),
;;; and what it installs is kept in features.lisp. forge.lisp opens the folder and installs features.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "features.lisp"
         "forge.lisp"
         "menus.lisp"
         "features/editor/editor.lisp"
         "features/editor/views.lisp"
         "features/themes/colours.lisp"
         "features/themes/themes.lisp"
         "features/panels/panels.lisp"
         "features/panels/views.lisp"
         "features/palette/palette.lisp"
         "features/palette/views.lisp"
         "features/search/search.lisp"
         "features/search/views.lisp"
         "features/shell/shell.lisp"
         "features/shell/views.lisp"
         "features/accounts/accounts.lisp"
         "features/accounts/views.lisp"
         "features/explorer/explorer.lisp"
         "features/explorer/views.lisp"
         "features/terminal/terminal.lisp"
         "features/terminal/views.lisp"
         "features/scm/scm.lisp"
         "features/scm/views.lisp"
         "features/agent/agent.lisp"
         "features/agent/views.lisp"
         "features/lsp/lsp.lisp"
         "features/lsp/views.lisp"
         "features/debug/debug.lisp"
         "features/debug/views.lisp"
         "features/editing/editing.lisp"
         "features/editing/views.lisp"
         "features/workbench/workbench.lisp"
         "features/workbench/views.lisp"
         "features/merge/merge.lisp"
         "features/merge/views.lisp"
         "../../shared/languages/languages.lisp"
         "../../shared/addons/assistant/assistant.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Forge's one window: the workbench, and what opens over it."
  :window {:title "Forge"
           :width 1200
           :height 760
           :minWidth 800
           :minHeight 500
           :menuBar [{:menu file :title ~(t "menu.file")}
                     {:menu view :title ~(t "menu.view")}
                     {:menu go :title ~(t "menu.go")}
                     {:menu preferences :title ~(t "menu.preferences")}
                     {:menu help :title ~(t "menu.help")}]}
  :strings {:default en
            :locale ~settings.workbench.language
            :keep ["Forge" "AffrescoUI" "Forge Dark" "Forge Light" "Forge High Contrast"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 28}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 22}
          :iconsRail {:file "../../shared/fonts/Lucide.ttf" :size 19}
          :code {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 13 :fallback [icons]}}
  :persist [state.layout state.theme state.themes state.settings state.iconTheme state.quickPick.recent]
  :permissions {:allow [studio] :ask ["agent(/actions)"] :reviewers [studio]}
  :theme ~state.theme
  (node#root :color @color.editor
    ~(workbench)
    (node :use forgeNotificationCentre)
    (node :use forgePalette)
    (node :use forgeDebugOverlays)
    (node :use forgeLsp)
    ~(progress-toast)
    ~(about-dialog)))

;;; Its parts

(defview workbench
  "The workbench: the title bar, the view rail, the side bars around the editors and the panel, the status bar."
  []
  `(column#workbench :fill true
     (node :when ~(not state.layout.zen) :use forgeTitleBar)
     (row#middle :width "100%" :grow 1
       (node :when ~(and (not state.layout.zen) settings.workbench.activityBar.visible) :use forgeActivityBar)
       ~(main-split)
       (node :when ~(and state.layout.secondary (not state.layout.zen)) :use forgeSecondarySideBar))
     (node :when ~(and (not state.layout.zen) settings.workbench.statusBar.visible) :use forgeStatusBar)))

(defview main-split
  "The side bar beside the editors, and the panel under them, each beyond a sash that keeps its place."
  []
  `(split#main-split :grow 1
     :height "100%"
     :bind state.layout.sizes
     :minSize 170
     :sashSize 1
     :sashColor @color.border
     :sashActiveColor @color.accent
     (node :visible ~(and state.layout.sidebar (not state.layout.zen)) :use forgeSideBar)
     (split#editor-split :direction column
       :bind state.layout.panelSizes
       :minSize 100
       :sashSize 1
       :sashColor @color.border
       :sashActiveColor @color.accent
       (node :visible ~(not (and state.layout.panelMax state.layout.panel (not state.layout.zen))) :use forgeEditors)
       (node :visible ~(and state.layout.panel (not state.layout.zen)) :use forgePanel))))

(defview progress-toast "The toast in the corner: a notice, with a bar while a long task runs." []
  `(node :use toast :with {:open state.toast.open :message ~state.toast.message :progress ~state.toast.progress}))

(defview about-dialog "Help > About: what Forge is." []
  `(node :use dialog
     :with {:open state.about :title "Forge" :message ~(t "about.message") :buttons [{:title ~(t "about.ok")}]}))
