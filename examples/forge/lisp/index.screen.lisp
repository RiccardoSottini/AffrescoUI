;;; index.screen.lisp --- Forge, a demo IDE
;;;
;;; This file only names the window, the fonts and the features; each feature (features/<name>/<name>.json)
;;; contributes its state, commands, menus, keys, settings, tokens and components, merged by include. The open folder
;;; is the host's workspace root (Open Folder chooses it): files reads and writes in it, fileChanges watches it and
;;; search searches it; the features service offers the example catalogue (catalog/).

(include "features.lisp"
         "locales/en.lisp"
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
         "features/debug/views.lisp"
         "features/editing/editing.lisp"
         "features/editing/views.lisp"
         "features/workbench/workbench.lisp"
         "features/workbench/views.lisp"
         "features/merge/merge.lisp"
         "features/merge/views.lisp"
         "../../shared/languages/languages.lisp"
         "../../shared/addons/assistant/assistant.lisp")

(defstate featureRows [])

(defstate featureStatus "")

(deftokens assistant-colours "Tokens."
  {:assistant.accent @color.accent
   :assistant.accentHover @color.selection
   :assistant.onAccent "#ffffff"
   :assistant.surface @color.widget
   :assistant.raised @color.input
   :assistant.border @color.border
   :assistant.text @color.text
   :assistant.dim @color.muted
   :assistant.dot @color.muted
   :assistant.success @color.info
   :assistant.danger @color.error
   :assistant.warningBackground @color.hover})

(defpart index
         "What index adds."
         :services {:features {:provider features}
                    :files {:provider fs :root workspace}
                    :fileChanges {:provider watch
                                  :root workspace
                                  :debounce 100
                                  :ignore [".git/**"
                                           "**/node_modules/**"
                                           "**/build/**"
                                           "**/out/**"
                                           "**/dist/**"
                                           "**/target/**"]}
                    :search {:provider search :root workspace}}
         :onLoad openWorkspace
         :exports {:editor.text (string :value ~(or-else computed.activeDoc.text "")
                                        :description "The text of the editor in focus.")
                   :workspace.files (array :value ~(map (fn [it] {:id it.id :name it.label :text it.text})
                                                        (filter (fn [it] (= it.kind "file")) state.docs))
                                           :description "The open files: id, name and text.")})

(defview main-split "#main-split." []
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

(defscreen main
  :window {:title ~(t "window.title")
           :width 1200
           :height 760
           :minWidth 800
           :minHeight 500
           :menuBar [{:menu file :title ~(t "menu.file")}
                     {:menu view :title ~(t "menu.view")}
                     {:menu go :title ~(t "menu.go")}
                     {:menu preferences :title ~(t "menu.preferences")}
                     {:menu help :title ~(t "menu.help")}]}
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
    (column#workbench :fill true
      (node :when ~(not state.layout.zen) :use forgeTitleBar)
      (row#middle :width "100%" :grow 1
        (node :when ~(and (not state.layout.zen) settings.workbench.activityBar.visible) :use forgeActivityBar)
        ~(main-split)
        (node :when ~(and state.layout.secondary (not state.layout.zen)) :use forgeSecondarySideBar))
      (node :when ~(and (not state.layout.zen) settings.workbench.statusBar.visible) :use forgeStatusBar))
    (node :use forgeNotificationCentre)
    (node :use forgePalette)
    (node :use forgeDebugOverlays)
    (node :use forgeLsp)
    (node :use toast :with {:open state.toast.open :message ~state.toast.message :progress ~state.toast.progress})
    (node :use dialog
      :with {:open state.about
             :title "Forge"
             :message "A demo IDE built with AffrescoUI: its whole workbench is a screen document that people and agents change while it runs."
             :buttons [{:title ~(t "about.ok")}]})))

;;; Functions

(defmacro doseq "Runs its body once for each item of a list, for what it does: [item list]."
  [binding & body]
  `(do (map (fn [~(first binding)] ~@body) ~(second binding)) nil))

(defn refreshFeatures "TODO" []
  (invoke "features.catalog" :into "state.featureRows"))

(defn installFeature "event: name. Installs a feature from the example catalogue, by a revision the permissions review"
  []
  (invoke "features.install" :with {:name event.name} :onDone installFeatureDone :onError installFeatureFailed))

(defn upgradeFeature "event: name" []
  (invoke "features.upgrade" :with {:name event.name} :onDone upgradeFeatureDone :onError upgradeFeatureFailed))

(defn openWorkspace "TODO" []
  (set "state.folder.root" (or-else services.files.folder ""))
  (explorerRefresh)
  (when (not (empty? state.folder.root)) (indexFolder)))

(defn installFeatureDone "What installFeature does when it is done." [result]
  (set "state.featureStatus" result.status))

(defn installFeatureFailed "What installFeature does when it fails." [error]
  (set "state.featureStatus" error.message))

(defn upgradeFeatureDone "What upgradeFeature does when it is done." [result]
  (set "state.featureStatus" result.status))

(defn upgradeFeatureFailed "What upgradeFeature does when it fails." [error]
  (set "state.featureStatus" error.message))
