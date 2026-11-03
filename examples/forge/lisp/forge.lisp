;;; forge.lisp --- what the workbench does on its own: the folder it opens, the catalogue's features, its language
;;;
;;; The folder is the host's workspace root, read through the files service and watched through fileChanges; the
;;; features service lists the example catalogue and installs a feature from it by a revision the permissions
;;; review. The other files' functions use `doseq`, defined here.

;;; The open folder

(defpart workspace "The services on the open folder, and what Forge gives an installed feature to read."
  :services {:features {:provider features}
             :files {:provider fs :root workspace}
             :fileChanges {:provider watch
                           :root workspace
                           :debounce 100
                           :ignore [".git/**" "**/node_modules/**" "**/build/**" "**/out/**" "**/dist/**" "**/target/**"]}
             :search {:provider search :root workspace}}
  :onLoad openWorkspace
  :exports {:editor.text (string :value ~(or-else computed.activeDoc.text "") :description "The text of the editor in focus.")
            :workspace.files (array :value ~(map (fn [it] {:id it.id :name it.label :text it.text})
                                                 (filter (fn [it] (= it.kind "file")) state.docs))
                               :description "The open files: id, name and text.")})

(defn openWorkspace "On load: the folder the host opened, its tree listed and its files indexed for Go to File." []
  (set "state.folder.root" (or-else services.files.folder ""))
  (explorerRefresh)
  (when (not (empty? state.folder.root)) (indexFolder)))

;;; The catalogue's features

(defstate featureRows [])

(defstate featureStatus "")

(defn refreshFeatures "Lists the features of the example catalogue." []
  (invoke "features.catalog" :into "state.featureRows"))

(defn installFeature "Installs the feature `event.name` from the catalogue, by a revision the permissions review." []
  (invoke "features.install" :with {:name event.name} :onDone featureDone :onError featureFailed))

(defn upgradeFeature "Upgrades the feature `event.name` to the catalogue's release." []
  (invoke "features.upgrade" :with {:name event.name} :onDone featureDone :onError featureFailed))

(defn featureDone "What the service says of an install or an upgrade." [result]
  (set "state.featureStatus" result.status))

(defn featureFailed "Why an install or an upgrade failed." [error]
  (set "state.featureStatus" error.message))

;;; The language, and the assistant's colours

(defcustom workbench.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "settings.workbench.language") :description ~(t "settings.workbench.language.about"))

(deftokens assistant-colours "The assistant add-on's colours, taken from the workbench's theme."
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

;;; What the others use

(defn activeTabs "The active editor group's tabs." []
  (get-in state.groups [state.activeGroup :tabs]))

(defn activeTabId "The id of the active group's tab shown: a file's path, or a page's id." []
  (get-in state.groups [state.activeGroup :active]))

(defn baseName "A path's last part: a file's or a folder's name." [path]
  (last (split path "/")))

(defmacro doseq "Runs its body once for each item of a list, for what it does: (doseq [item list] body...)."
  [binding & body]
  `(do (map (fn [~(first binding)] ~@body) ~(second binding)) nil))
