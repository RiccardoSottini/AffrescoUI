;;; shell.lisp --- Forge's shell: the layout, the open folder and documents, the tabs' history, the views
;;;
;;; What is shown and where the sashes are is in state.layout, kept by persist; zen mode hides everything but the
;;; editors. The open documents are state.docs (files, and pages such as the settings), shown by the tabs of
;;; state.groups. Go Back and Go Forward walk the tabs shown before. The views (views.lisp) are the title bar with its
;;; command centre, the view rail, the side bar, the welcome page, the breadcrumbs and the status bar.

(ns forge.features.shell.shell)

;;; The layout

(defstate layout
  {:sidebar true :panel false :panelMax false :zen false :secondary false :sizes [240] :panelSizes [480] :groupSizes []})

(defcommand toggleSidebar "Toggle Left Side Bar"
  :title ~(t "command.toggleSidebar")
  :description ~(t "command.toggleSidebar") :category ~(t "category.view") :icon "panel-left" []
  (toggle "state.layout.sidebar"))

(defcommand togglePanel "Toggle Panel"
  :title ~(t "command.togglePanel")
  :description ~(t "command.togglePanel") :category ~(t "category.view") :icon "panel-bottom" []
  (toggle "state.layout.panel")
  (set "state.layout.panelMax" false))

(defcommand toggleMaximizedPanel "Maximize Panel Size"
  :title ~(t "command.toggleMaximizedPanel")
  :description ~(t "command.toggleMaximizedPanel") :category ~(t "category.view") :icon "chevron-up" []
  (set "state.layout.panel" true)
  (toggle "state.layout.panelMax"))

(defcommand toggleZen "Hide everything but the editors; run it again to bring the workbench back."
  :title ~(t "command.toggleZen") :category ~(t "category.view") :icon "maximize"
  :description ~(t "command.toggleZen.about") []
  (toggle "state.layout.zen"))

(defkey "mod+b" toggleSidebar)

(defkey "mod+j" togglePanel)

(defkey "mod+k z" toggleZen)

;;; The side bar's views

(defstate sideView explorer)

(defn forge-show-view "Shows a view of the side bar, or hides the side bar if it shows that view already." [view]
  (if (and (= state.sideView view) (true? state.layout.sidebar))
    (set "state.layout.sidebar" false)
    (do (set "state.sideView" view) (set "state.layout.sidebar" true))))

(defcommand showExplorer "Show Explorer"
  :title ~(t "command.showExplorer")
  :description ~(t "command.showExplorer") :category ~(t "category.view") :icon "files" []
  (forge-show-view "explorer"))

(defcommand showSearch "Show Search"
  :title ~(t "command.showSearch")
  :description ~(t "command.showSearch") :category ~(t "category.view") :icon "search" []
  (forge-show-view "search"))

;;; The open folder

(defstate folder
  {:root ""
   :files []
   :truncated false
   :skip "**/node_modules/**,**/build/**,**/out/**,**/dist/**,**/target/**,**/__pycache__/**,**/.*/**,**/.*"})

(defstate explorer {:expanded [] :selected nil})

(defcomputed folderName
  (if (empty? state.folder.root) (t "folder.none") (get (reverse (split state.folder.root "/")) 0)))

(defcommand openFolder "Open Folder..."
  :title ~(t "command.openFolder")
  :description ~(t "command.openFolder") :category ~(t "category.file") :icon "folder-open" []
  (chooseFolder "workspace" :title (t "dialog.openFolder") :onError open-folder-failed :onDone open-folder-done))

(defn open-folder-done "The folder chosen: its tree listed and its files indexed, from nothing." [result]
  (set "state.explorer" {:expanded [] :selected nil})
  (set "state.explorerTree" {:entries [] :listed []})
  (set "state.folder.root" result.name)
  (explorerRefresh)
  (index-folder)
  (log-line (t "log.opened" {:folder result.name})))

(defn open-folder-failed "Says why no folder was opened." [error]
  (notify "error" (t "notify.noFolder" {:error error.message})))

(defn index-folder "Lists the folder's files for Go to File." []
  (invoke "search.files" :with {:exclude state.folder.skip} :onDone index-folder-done :onError index-folder-failed))

(defn index-folder-done "The folder's files, listed." [result]
  (indexed result.files)
  (set "state.folder.truncated" result.truncated))

(defn index-folder-failed "Nothing listed: Go to File has no files." []
  (set "state.folder.files" []))

(defn indexed "The folder's files as Go to File lists them: each its name, and the folder it is in." [files]
  (set "state.folder.files"
       ; style: keep, #(...) can't give a map, and the host's split keeps the empty name after a last slash
       (map (fn [it] {:id it :title (base-name it) :detail (str/join "/" (slice (split it "/") 0 (- 1)))}) files)))

; style: keep, named in examples/forge/lisp/layout.scenarios.json, examples/forge/lisp/workbench.scenarios.json, examples/forge/lisp/controls.scenarios.json and more
(defn openExplorerItem "An entry of the explorer, opened: a file opens in a tab." []
  (when (and (seq event.id) (not= (:kind (find #(= (:path %) event.id) state.explorerTree.entries)) "folder"))
    (open-path event.id (base-name event.id))))

(defkey "mod+o" openFolder)

;;; Cloning a repository

(defstate clone {:url ""})

(defcomputed cloneItems
  (if (str/blank? (clone-address))
        []
        [{:title (t "pick.cloneFrom" {:url (clone-address)}) :url (clone-address)}]))

(defn clone-address "The repository's address typed in the palette after 'clone '." []
  (str/trim (slice (or-else state.quickPick.query "") 6)))

(defcommand cloneRepository "Clone a git repository into a folder you choose, then open it."
  :title ~(t "command.cloneRepository") :category ~(t "category.file") :icon "git-branch"
  :description ~(t "command.cloneRepository.about") []
  (set "state.quickPick.query" "clone ")
  (set "state.quickPick.open" true))

(defn clone-picked "A repository's address typed in the palette: an empty folder is asked for, to clone it into." [item]
  (if (or (str/blank? item.url) (str/starts-with? item.url "-") (str/includes? item.url " "))
    (notify "error" (t "notify.cloneAddress"))
    (do (set "state.clone.url" item.url)
        (chooseFolder "workspace" :title (t "dialog.cloneInto") :onError open-folder-failed :onDone clone-into))))

(defn clone-into
  "The folder chosen, opened: git clones the repository into it in a terminal of its own, where the person sees it run
and answers it if it asks for a password; the explorer shows the files as they arrive."
  [result]
  (open-folder-done result)
  (task-start {:title (t "terminal.clone") :line (shellLine ["git" "clone" "--" state.clone.url "."])}))

;;; Documents and tabs

(defstate docs [])

(defstate groups [{:tabs [] :active nil}])

(defstate activeGroup 0)

(defstate caret {:line 1 :column 1 :selected 0})

(defstate languages
  {:c "C++"
   :cc "C++"
   :cpp "C++"
   :h "C++"
   :hpp "C++"
   :json "JSON"
   :md "Markdown"
   :cmake "CMake"
   :py "Python"
   :js "JavaScript"
   :mjs "JavaScript"
   :ts "TypeScript"
   :tsx "TypeScript"
   :sh "Shell"
   :bash "Shell"
   :zsh "Shell"})

(defcomputed docIds (map :id state.docs))

(defcomputed dirtyIds (map :id (filter :dirty state.docs)))

(defcomputed activeDoc (find #(= (:id %) (get-in state.groups [state.activeGroup :active])) state.docs))

(defcomputed welcomeRecent
  (if (empty? state.folder.root)
        []
        (slice (filter #(contains (map :id (or-else state.folder.files [])) (:id %))
                       (or-else state.quickPick.recent []))
               0
               5)))

;;; The welcome page's walkthrough and keys

(defstate walkthrough {:theme false :live false :keys false})

(defcomputed welcomeKeys (welcome-keys))

(defn welcome-keys "The keys the welcome page lists, each its command's name, what it does and its key." []
  (mapv welcome-key-row
        ["quickOpen" "showCommands" "toggleSidebar" "togglePanel" "splitEditor" "toggleZen" "openSettings"]))

(defn welcome-key-row "A key the welcome page lists: the command's name, what it does, and its key." [name]
  {:name name :title (t (str "welcome-key." name)) :key (key-label name)})

(defn key-label "The key of a command, by its name, as the keyboard shortcuts page shows it ('' if it has none)."
  [command-name]
  (let [command (first (filter #(= (:name %) command-name) commands))]
    (or-else (:keyLabel command) "")))

(defn welcome-step-theme "The walkthrough's first step: a colour theme to choose." []
  (set "state.walkthrough.theme" true)
  (pickTheme))

(defn welcome-step-live "The walkthrough's third step: the Agent view, which changes Forge while it runs." []
  (set "state.walkthrough.live" true)
  (showAgent))

(defn welcome-step-keys "The walkthrough's last step: the keyboard shortcuts page." []
  (set "state.walkthrough.keys" true)
  (openKeybindings))

; style: keep, named in examples/forge/lisp/features.scenarios.json, examples/forge/lisp/catalog/todoTree/1.2.0.json
(defn showTab "Shows a tab in the active group, adding it if it isn't there." [id label]
  (forge-record-nav id)
  (when-not (some #(= (:id %) id) (active-tabs))
    (push (str "state.groups[" state.activeGroup "].tabs") {:id id :label label}))
  (set (str "state.groups[" state.activeGroup "].active") id))

(defn open-page "Opens a page (the settings, the keyboard shortcuts...) as a document of its own, in a tab."
  [id label kind]
  (when-not (contains computed.docIds id)
    (push "state.docs" {:id id :label label :kind kind :text "" :dirty false :language ""}))
  (showTab id label))

(defcommand closeEditor "Close Editor"
  :title ~(t "command.closeEditor") :description ~(t "command.closeEditor") :category ~(t "category.view") :icon "x"
  :enablement ~(not (empty? (get-in state.groups [state.activeGroup :active]))) []
  (close-tab state.activeGroup (active-tab-id)))

(defcommand splitEditor "Open the active editor beside the current one (two groups at most)."
  :title ~(t "command.splitEditor") :category ~(t "category.view") :icon "columns-2"
  :description ~(t "command.splitEditor.about")
  :enablement ~(and (< (count state.groups) 2) (not (empty? (get-in state.groups [state.activeGroup :active])))) []
  (push "state.groups" {:tabs (filter #(= (:id %) (active-tab-id)) (active-tabs)) :active (active-tab-id)})
  (set "state.activeGroup" (dec (count state.groups))))

(defkey "mod+w" closeEditor)

(defkey "mod+\\" splitEditor)

;;; Going back and forward

(defstate nav {:back [] :forward [] :moving false})

(defcommand navigateBack "Go Back"
  :title ~(t "command.navigateBack")
  :description ~(t "command.navigateBack") :category ~(t "category.go") :icon "arrow-left"
  :enablement ~(> (count state.nav.back) 0) []
  (set "state.nav.forward" (concat (filter #(= (:id %) (active-tab-id)) (active-tabs)) state.nav.forward))
  (forge-navigate-to (last state.nav.back))
  (set "state.nav.back" (slice state.nav.back 0 (- 1))))

(defcommand navigateForward "Go Forward"
  :title ~(t "command.navigateForward")
  :description ~(t "command.navigateForward") :category ~(t "category.go") :icon "arrow-right"
  :enablement ~(> (count state.nav.forward) 0) []
  (set "state.nav.back" (concat state.nav.back (filter #(= (:id %) (active-tab-id)) (active-tabs))))
  (forge-navigate-to state.nav.forward.0)
  (set "state.nav.forward" (slice state.nav.forward 1)))

(defn forge-navigate-to "Shows a tab (`tab`: id, label) without recording it in the history." [tab]
  (set "state.nav.moving" true)
  (showTab tab.id tab.label)
  (set "state.nav.moving" false))

(defn forge-record-nav
  "Before a tab (`id`) is shown: the one shown goes into the history (the last 50), unless Back or Forward moves."
  [id]
  (when (and (not (true? state.nav.moving)) (seq (active-tab-id)) (not= (active-tab-id) id))
    (set "state.nav"
         {:back (slice (concat state.nav.back (filter #(= (:id %) (active-tab-id)) (active-tabs)))
                       (max 0 (- (count state.nav.back) 49)))
          :forward []
          :moving false})))

(defkey "ctrl+minus" navigateBack)

(defkey "ctrl+shift+minus" navigateForward)

;;; The rest

(defn hide-notifications "Hides the notification centre." []
  (set "state.notificationCentre" false))

(defstate menuBar {})

(defstate about false)

(defstate notificationCentre false)

(defcommand about "About Forge"
  :title ~(t "command.about") :description ~(t "command.about") :category ~(t "category.help") :icon "info" []
  (set "state.about" true))
