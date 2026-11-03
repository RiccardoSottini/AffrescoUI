;;; shell.lisp --- Forge's shell: the layout, the open folder and documents, the tabs' history, the views
;;;
;;; What is shown and where the sashes are is in state.layout, kept by persist; zen mode hides everything but the
;;; editors. The open documents are state.docs (files, and pages such as the settings), shown by the tabs of
;;; state.groups. Go Back and Go Forward walk the tabs shown before. The views (views.lisp) are the title bar with its
;;; command centre, the view rail, the side bar, the welcome page, the breadcrumbs and the status bar.

;;; The layout

(defstate layout
  {:sidebar true :panel false :panelMax false :zen false :secondary false :sizes [240] :panelSizes [480] :groupSizes []})

(defcommand toggleSidebar "Toggle Left Side Bar"
  :title ~(t "command.toggleSidebar") :category ~(t "category.view") :icon "panel-left" []
  (toggle "state.layout.sidebar"))

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

(defkey "mod+b" toggleSidebar)

(defkey "mod+j" togglePanel)

(defkey "mod+k z" toggleZen)

;;; The side bar's views

(defstate sideView explorer)

(defn forgeShowView "Shows a view of the side bar, or hides the side bar if it shows that view already." [view]
  (if (and (= state.sideView view) state.layout.sidebar)
    (set "state.layout.sidebar" false)
    (do (set "state.sideView" view) (set "state.layout.sidebar" true))))

(defcommand showExplorer "Show Explorer"
  :title ~(t "command.showExplorer") :category ~(t "category.view") :icon "files" []
  (forgeShowView "explorer"))

(defcommand showSearch "Show Search"
  :title ~(t "command.showSearch") :category ~(t "category.view") :icon "search" []
  (forgeShowView "search"))

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
  :title ~(t "command.openFolder") :category ~(t "category.file") :icon "folder-open" []
  (chooseFolder "workspace" :title (t "dialog.openFolder") :onError openFolderFailed :onDone openFolderDone))

(defn openFolderDone "The folder chosen: its tree listed and its files indexed, from nothing." [result]
  (set "state.explorer" {:expanded [] :selected nil})
  (set "state.explorerTree" {:entries [] :listed []})
  (set "state.folder.root" result.name)
  (explorerRefresh)
  (indexFolder)
  (log (t "log.opened" {:folder result.name})))

(defn openFolderFailed "Says why no folder was opened." [error]
  (notify "error" (t "notify.noFolder" {:error error.message})))

(defn indexFolder "Lists the folder's files for Go to File." []
  (invoke "search.files" :with {:exclude state.folder.skip} :onDone indexFolderDone :onError indexFolderFailed))

(defn indexFolderDone "The folder's files, listed." [result]
  (indexed result.files)
  (set "state.folder.truncated" result.truncated))

(defn indexFolderFailed "Nothing listed: Go to File has no files." []
  (set "state.folder.files" []))

(defn indexed "The folder's files as Go to File lists them: each its name, and the folder it is in." [files]
  (set "state.folder.files"
       (map (fn [it] {:id it :title (baseName it) :detail (join "/" (slice (split it "/") 0 (- 1)))}) files)))

(defn openExplorerItem "An entry of the explorer, opened: a file opens in a tab." []
  (when (and (not (empty? event.id))
             (not= (:kind (or-else (find (fn [it] (= it.path event.id)) state.explorerTree.entries) {})) "folder"))
    (openPath event.id (baseName event.id))))

(defkey "mod+o" openFolder)

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

(defcomputed docIds (map (fn [it] it.id) state.docs))

(defcomputed dirtyIds (map (fn [it] it.id) (filter (fn [it] it.dirty) state.docs)))

(defcomputed activeDoc (find (fn [it] (= it.id (get-in state.groups [state.activeGroup :active]))) state.docs))

(defcomputed welcomeRecent
  (if (empty? state.folder.root)
        []
        (slice (filter (fn [it] (contains (map (fn [it] it.id) (or-else state.folder.files [])) it.id))
                       (or-else state.quickPick.recent []))
               0
               5)))

(defn showTab "Shows a tab in the active group, adding it if it isn't there." [id label]
  (forgeRecordNav id)
  (when (not (some (fn [it] (= it.id id)) (activeTabs)))
    (push (str "state.groups[" state.activeGroup "].tabs") {:id id :label label}))
  (set (str "state.groups[" state.activeGroup "].active") id))

(defn openPage "Opens a page (the settings, the keyboard shortcuts...) as a document of its own, in a tab."
  [id label kind]
  (when (not (contains computed.docIds id))
    (push "state.docs" {:id id :label label :kind kind :text "" :dirty false :language ""}))
  (showTab id label))

(defcommand closeEditor "Close Editor"
  :title ~(t "command.closeEditor") :category ~(t "category.view") :icon "x"
  :enablement ~(not (empty? (get-in state.groups [state.activeGroup :active]))) []
  (closeTab state.activeGroup (activeTabId)))

(defcommand splitEditor "Open the active editor beside the current one (two groups at most)."
  :title ~(t "command.splitEditor") :category ~(t "category.view") :icon "columns-2"
  :description ~(t "command.splitEditor.about")
  :enablement ~(and (< (count state.groups) 2) (not (empty? (get-in state.groups [state.activeGroup :active])))) []
  (push "state.groups" {:tabs (filter (fn [it] (= it.id (activeTabId))) (activeTabs)) :active (activeTabId)})
  (set "state.activeGroup" (- (count state.groups) 1)))

(defkey "mod+w" closeEditor)

(defkey "mod+\\" splitEditor)

;;; Going back and forward

(defstate nav {:back [] :forward [] :moving false})

(defcommand navigateBack "Go Back"
  :title ~(t "command.navigateBack") :category ~(t "category.go") :icon "arrow-left"
  :enablement ~(> (count state.nav.back) 0) []
  (set "state.nav.forward" (concat (filter (fn [it] (= it.id (activeTabId))) (activeTabs)) state.nav.forward))
  (forgeNavigateTo (last state.nav.back))
  (set "state.nav.back" (slice state.nav.back 0 (- (count state.nav.back) 1))))

(defcommand navigateForward "Go Forward"
  :title ~(t "command.navigateForward") :category ~(t "category.go") :icon "arrow-right"
  :enablement ~(> (count state.nav.forward) 0) []
  (set "state.nav.back" (concat state.nav.back (filter (fn [it] (= it.id (activeTabId))) (activeTabs))))
  (forgeNavigateTo state.nav.forward.0)
  (set "state.nav.forward" (slice state.nav.forward 1)))

(defn forgeNavigateTo "Shows a tab (`tab`: id, label) without recording it in the history." [tab]
  (set "state.nav.moving" true)
  (showTab tab.id tab.label)
  (set "state.nav.moving" false))

(defn forgeRecordNav "Before a tab (`id`) is shown: the one shown now goes into the history Go Back walks (the last 50), unless Go Back or Go Forward is moving."
  [id]
  (when (and (not state.nav.moving) (not (empty? (activeTabId))) (not= (activeTabId) id))
    (set "state.nav"
         {:back (slice (concat state.nav.back (filter (fn [it] (= it.id (activeTabId))) (activeTabs)))
                       (max 0 (- (count state.nav.back) 49)))
          :forward []
          :moving false})))

(defkey "ctrl+minus" navigateBack)

(defkey "ctrl+shift+minus" navigateForward)

;;; The rest

(defn hideNotifications "Hides the notification centre." []
  (set "state.notificationCentre" false))

(defstate menuBar {})

(defstate about false)

(defstate notificationCentre false)

(defcommand about "About Forge"
  :title ~(t "command.about") :category ~(t "category.help") :icon "info" []
  (set "state.about" true))
