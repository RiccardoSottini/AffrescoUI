{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge, a demo IDE. This file only names the window, the fonts and the features; each feature (features/<name>/<name>.json) contributes its state, commands, menus, keys, settings, tokens and components, merged by include. The open folder is the host's workspace root (Open Folder chooses it): files reads and writes in it, fileChanges watches it and search searches it; the features service offers the example catalogue (catalog/)."
  :window {:title Forge
    :width 1200
    :height 760
    :minWidth 800
    :minHeight 500
    :menuBar [{:menu file :title File}
      {:menu view :title View}
      {:menu go :title Go}
      {:menu preferences :title Preferences}
      {:menu help :title Help}]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons]}
    :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 28}
    :icons {:file "../../shared/fonts/Lucide.ttf" :size 13}
    :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 22}
    :iconsRail {:file "../../shared/fonts/Lucide.ttf" :size 19}
    :code {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 13 :fallback [icons]}}
  :include ["features/editor/editor.lisp"
    "features/themes/themes.lisp"
    "features/panels/panels.lisp"
    "features/palette/palette.lisp"
    "features/search/search.lisp"
    "features/shell/shell.lisp"
    "features/accounts/accounts.lisp"
    "features/explorer/explorer.lisp"
    "features/terminal/terminal.lisp"
    "features/scm/scm.lisp"
    "features/agent/agent.lisp"
    "features/lsp/lsp.lisp"
    "features/debug/debug.lisp"
    "features/debug/debugViews.lisp"
    "features/editing/editing.lisp"
    "features/workbench/workbench.lisp"
    "features/merge/merge.lisp"
    "../../shared/languages/languages.lisp"
    "../../shared/addons/assistant/assistant.lisp"]
  :tokens {:assistant.accent @color.accent
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
    :assistant.warningBackground @color.hover}
  :theme "{{state.theme}}"
  :permissions {:allow [studio] :ask ["agent(/actions)"] :reviewers [studio]}
  :state {:featureRows [] :featureStatus ""}
  :features {}
  :services {:features {:provider features}
    :files {:provider fs :root workspace}
    :fileChanges {:provider watch
      :root workspace
      :debounce 100
      :ignore [".git/**" "**/node_modules/**" "**/build/**" "**/out/**" "**/dist/**" "**/target/**"]}
    :search {:provider search :root workspace}}
  :actions {:refreshFeatures [{:invoke features.catalog :into state.featureRows}]
    :installFeature [{:note "event: name. Installs a feature from the example catalogue, by a revision the permissions review"}
      {:invoke features.install
        :with {:name "{{event.name}}"}
        :onDone [{:set state.featureStatus :to "{{result.status}}"}]
        :onError [{:set state.featureStatus :to "{{error.message}}"}]}]
    :upgradeFeature [{:note "event: name"}
      {:invoke features.upgrade
        :with {:name "{{event.name}}"}
        :onDone [{:set state.featureStatus :to "{{result.status}}"}]
        :onError [{:set state.featureStatus :to "{{error.message}}"}]}]}
  :persist [state.layout state.theme state.themes state.settings state.iconTheme state.quickPick.recent]
  :onLoad [{:set state.folder.root :to "{{services.files.folder ?? ''}}"}
    {:run explorerRefresh}
    {:if "{{!empty(state.folder.root)}}" :then [{:run indexFolder}]}]
  :exports {:editor.text {:type string
      :value "{{computed.activeDoc.text ?? ''}}"
      :description "The text of the editor in focus."}
    :workspace.files {:type array
      :value "{{map(filter(state.docs, it.kind == 'file'), {id: it.id, name: it.label, text: it.text})}}"
      :description "The open files: id, name and text."}}
  :root (node #root :color @color.editor
    (column #workbench :fill true
      {:when "{{!state.layout.zen}}" :use forgeTitleBar}
      (row #middle :width "100%" :grow 1
        {:when "{{!state.layout.zen && settings.workbench.activityBar.visible}}" :use forgeActivityBar}
        (split #main-split
          :grow 1
          :height "100%"
          :bind state.layout.sizes
          :minSize 170
          :sashSize 1
          :sashColor @color.border
          :sashActiveColor @color.accent
          {:visible "{{state.layout.sidebar && !state.layout.zen}}" :use forgeSideBar}
          (split #editor-split
            :direction column
            :bind state.layout.panelSizes
            :minSize 100
            :sashSize 1
            :sashColor @color.border
            :sashActiveColor @color.accent
            {:visible "{{!(state.layout.panelMax && state.layout.panel && !state.layout.zen)}}" :use forgeEditors}
            {:visible "{{state.layout.panel && !state.layout.zen}}" :use forgePanel}))
        {:when "{{state.layout.secondary && !state.layout.zen}}" :use forgeSecondarySideBar})
      {:when "{{!state.layout.zen && settings.workbench.statusBar.visible}}" :use forgeStatusBar})
    {:use forgeNotificationCentre}
    {:use forgePalette}
    {:use forgeDebugOverlays}
    {:use forgeLsp}
    {:use toast :with {:open state.toast.open :message "{{state.toast.message}}" :progress "{{state.toast.progress}}"}}
    {:use dialog
      :with {:open state.about
        :title Forge
        :message "A demo IDE built with AffrescoUI: its whole workbench is a screen document that people and agents change while it runs."
        :buttons [{:title OK}]}})}
