;;; shell.lisp --- Forge's shell: the title bar with a command centre, the activity bar, the explorer side bar, editor groups with tabs, the panel and the status bar
;;;
;;; The layout (what is shown, the sash positions) is in state.layout, kept by persist; zen mode hides everything but
;;; the editors. Other features contribute commands, menus, keys, settings, tokens and components
;;; (examples/forge/README.md).

;;; State and definitions

(defstate layout
  {:sidebar true :panel false :panelMax false :zen false :secondary false :sizes [240] :panelSizes [480] :groupSizes []})

(defstate folder
  {:root ""
   :files []
   :truncated false
   :skip "**/node_modules/**,**/build/**,**/out/**,**/dist/**,**/target/**,**/__pycache__/**,**/.*/**,**/.*"})

(defstate explorer {:expanded [] :selected nil})

(defstate docs [])

(defstate groups [{:tabs [] :active nil}])

(defstate activeGroup 0)

(defstate caret {:line 1 :column 1 :selected 0})

(defstate menuBar {})

(defstate about false)

(defstate sideView explorer)

(defstate nav {:back [] :forward [] :moving false})

(defstate notificationCentre false)

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

(defcomputed docIds (map (fn [it] it.id) state.docs))

(defcomputed dirtyIds (map (fn [it] it.id) (filter (fn [it] it.dirty) state.docs)))

(defcomputed activeDoc (find (fn [it] (= it.id (get-in state.groups [state.activeGroup :active]))) state.docs))

(defcomputed folderName
  (if (empty? state.folder.root) "No Folder Opened" (get (reverse (split state.folder.root "/")) 0)))

(defcomputed welcomeRecent
  (if (empty? state.folder.root)
        []
        (slice (filter (fn [it] (contains (map (fn [it] it.id) (or-else state.folder.files [])) it.id))
                       (or-else state.quickPick.recent []))
               0
               5)))

(defkey "mod+b" toggleSidebar)

(defkey "mod+j" togglePanel)

(defkey "mod+k z" toggleZen)

(defkey "mod+o" openFolder)

(defkey "mod+w" closeEditor)

(defkey "mod+\\" splitEditor)

(defkey "ctrl+minus" navigateBack)

(defkey "ctrl+shift+minus" navigateForward)

;;; Functions and commands

(defcommand toggleSidebar "Toggle Left Side Bar"
  :title ~(t "command.toggleSidebar") :category ~(t "category.view") :icon "panel-left" []
  (toggle "state.layout.sidebar"))

(defn forgeShowView "TODO" [view]
  (if (and (= state.sideView view) state.layout.sidebar)
    (set "state.layout.sidebar" false)
    (do (set "state.sideView" view) (set "state.layout.sidebar" true))))

(defcommand showExplorer "Show Explorer"
  :title ~(t "command.showExplorer") :category ~(t "category.view") :icon "files" []
  (forgeShowView "explorer"))

(defcommand showSearch "Show Search"
  :title ~(t "command.showSearch") :category ~(t "category.view") :icon "search" []
  (forgeShowView "search"))

(defcommand navigateBack "Go Back"
  :title ~(t "command.navigateBack") :category ~(t "category.go") :icon "arrow-left"
  :enablement ~(> (count state.nav.back) 0) []
  (set "state.nav.forward"
       (concat (filter (fn [it] (= it.id (get-in state.groups [state.activeGroup :active])))
                       (get-in state.groups [state.activeGroup :tabs]))
               state.nav.forward))
  (forgeNavigateTo (get state.nav.back (- (count state.nav.back) 1)))
  (set "state.nav.back" (slice state.nav.back 0 (- (count state.nav.back) 1))))

(defcommand navigateForward "Go Forward"
  :title ~(t "command.navigateForward") :category ~(t "category.go") :icon "arrow-right"
  :enablement ~(> (count state.nav.forward) 0) []
  (set "state.nav.back"
       (concat state.nav.back
               (filter (fn [it] (= it.id (get-in state.groups [state.activeGroup :active])))
                       (get-in state.groups [state.activeGroup :tabs]))))
  (forgeNavigateTo state.nav.forward.0)
  (set "state.nav.forward" (slice state.nav.forward 1)))

(defn forgeNavigateTo "tab: { id, label }, shown without being recorded in the history" [tab]
  (set "state.nav.moving" true)
  (showTab tab.id tab.label)
  (set "state.nav.moving" false))

(defcommand togglePanel "Toggle Panel"
  :title ~(t "command.togglePanel") :category ~(t "category.view") :icon "panel-bottom" []
  (toggle "state.layout.panel")
  (set "state.layout.panelMax" false))

(defcommand toggleMaximizedPanel "Maximize Panel Size"
  :title ~(t "command.toggleMaximizedPanel") :category ~(t "category.view") :icon "chevron-up" []
  (set "state.layout.panel" true)
  (toggle "state.layout.panelMax"))

(defcommand toggleZen "Hide everything but the editors; run it again to bring the workbench back."
  :title ~(t "command.toggleZen") :category ~(t "category.view") :icon "maximize"
  :description ~(t "command.toggleZen.about") []
  (toggle "state.layout.zen"))

(defcommand openFolder "Open Folder..."
  :title ~(t "command.openFolder") :category ~(t "category.file") :icon "folder-open" []
  (chooseFolder "workspace" :title "Open Folder" :onError openFolderFailed :onDone openFolderDone))

(defn indexFolder "Lists the folder's files for Go to File" []
  (invoke "search.files" :with {:exclude state.folder.skip} :onDone indexFolderDone :onError indexFolderFailed))

(defn indexed "TODO" [files]
  (set "state.folder.files"
       (map (fn [it]
              {:id it :title (get (reverse (split it "/")) 0) :detail (join "/" (slice (split it "/") 0 (- 1)))})
            files)))

(defn openExplorerItem "TODO" []
  (when (and (not (empty? event.id))
             (not= (:kind (or-else (find (fn [it] (= it.path event.id)) state.explorerTree.entries) {})) "folder"))
    (openPath event.id (get (reverse (split event.id "/")) 0))))

(defn forgeRecordNav "id: the tab about to be shown. The tab shown now goes into the history Go Back walks (the last 50), unless Go Back or Go Forward is moving"
  [id]
  (when (and (not state.nav.moving)
             (not (empty? (get-in state.groups [state.activeGroup :active])))
             (not= (get-in state.groups [state.activeGroup :active]) id))
    (set "state.nav"
         {:back (slice (concat state.nav.back
                               (filter (fn [it] (= it.id (get-in state.groups [state.activeGroup :active])))
                                       (get-in state.groups [state.activeGroup :tabs])))
                       (max 0 (- (count state.nav.back) 49)))
          :forward []
          :moving false})))

(defn showTab "TODO" [id label]
  (forgeRecordNav id)
  (when (not (some (fn [it] (= it.id id)) (get-in state.groups [state.activeGroup :tabs])))
    (push (str "state.groups[" state.activeGroup "].tabs") {:id id :label label}))
  (set (str "state.groups[" state.activeGroup "].active") id))

(defn openPage "TODO" [id label kind]
  (when (not (contains computed.docIds id))
    (push "state.docs" {:id id :label label :kind kind :text "" :dirty false :language ""}))
  (showTab id label))

(defcommand closeEditor "Close Editor"
  :title ~(t "command.closeEditor") :category ~(t "category.view") :icon "x"
  :enablement ~(not (empty? (get-in state.groups [state.activeGroup :active]))) []
  (closeTab state.activeGroup (get-in state.groups [state.activeGroup :active])))

(defcommand splitEditor "Open the active editor beside the current one (two groups at most)."
  :title ~(t "command.splitEditor") :category ~(t "category.view") :icon "columns-2"
  :description ~(t "command.splitEditor.about")
  :enablement ~(and (< (count state.groups) 2) (not (empty? (get-in state.groups [state.activeGroup :active])))) []
  (push "state.groups"
        {:tabs (filter (fn [it] (= it.id (get-in state.groups [state.activeGroup :active])))
                       (get-in state.groups [state.activeGroup :tabs]))
         :active (get-in state.groups [state.activeGroup :active])})
  (set "state.activeGroup" (- (count state.groups) 1)))

(defcommand about "About Forge"
  :title ~(t "command.about") :category ~(t "category.help") :icon "info" []
  (set "state.about" true))

(defn openFolderFailed "What openFolder does when it fails." [error]
  (notify "error" (str "No folder opened: " error.message)))

(defn openFolderDone "What openFolder does when it is done." [result]
  (set "state.explorer" {:expanded [] :selected nil})
  (set "state.explorerTree" {:entries [] :listed []})
  (set "state.folder.root" result.name)
  (explorerRefresh)
  (indexFolder)
  (log (str "Opened " result.name)))

(defn indexFolderDone "What indexFolder does when it is done." [result]
  (indexed result.files)
  (set "state.folder.truncated" result.truncated))

(defn indexFolderFailed "What indexFolder does when it fails." []
  (set "state.folder.files" []))
