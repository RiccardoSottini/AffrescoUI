;;; themes.lisp --- Forge's themes: every colour of the workbench is a token with one variant per theme, so a theme is a set of values and switching is one state change
;;;
;;; Importing a colour theme adds a variant to each token through a patch. Importing reads the file with openFile and
;;; maps the theme format's colour keys to the tokens with themeSources. The file icon theme maps names and extensions
;;; to icons, which the explorer shows.

;;; State and definitions

(defstate theme forge-dark)

(defstate themes
  [(dark#forge-dark :name "Forge Dark")
   (light#forge-light :name "Forge Light")
   (hc#forge-high-contrast :name "Forge High Contrast")])

(defstate imported nil)

(defstate importedFile "")

(defstate themeSources
  [{:token titleBar
    :keys [titleBar.activeBackground editorGroupHeader.tabsBackground]
    :dark "#141211"
    :light "#e9e4dd"
    :hc "#000000"}
   {:token activityBar :keys [activityBar.background sideBar.background] :dark "#141211" :light "#e9e4dd" :hc "#000000"}
   {:token sideBar :keys [sideBar.background editor.background] :dark "#1a1816" :light "#f3efea" :hc "#000000"}
   {:token editor :keys [editor.background] :dark "#1f1d1b" :light "#fbf9f6" :hc "#000000"}
   {:token editorText :keys [editor.foreground foreground] :dark "#ddd6cd" :light "#2d2925" :hc "#ffffff"}
   {:token tabs
    :keys [editorGroupHeader.tabsBackground tab.inactiveBackground]
    :dark "#1a1816"
    :light "#efeae4"
    :hc "#000000"}
   {:token tabActive :keys [tab.activeBackground editor.background] :dark "#1f1d1b" :light "#fbf9f6" :hc "#000000"}
   {:token panel :keys [panel.background editor.background] :dark "#1a1816" :light "#f3efea" :hc "#000000"}
   {:token statusBar :keys [statusBar.background] :dark "#141211" :light "#e9e4dd" :hc "#000000"}
   {:token statusText :keys [statusBar.foreground] :dark "#a69d93" :light "#5a524a" :hc "#ffffff"}
   {:token accent
    :keys [focusBorder activityBarBadge.background button.background]
    :dark "#cf6d26"
    :light "#b0531a"
    :hc "#c25a12"}
   {:token selection
    :keys [list.activeSelectionBackground editor.selectionBackground]
    :dark "#46311f"
    :light "#f2d6bf"
    :hc "#5a2e0c"}
   {:token hover :keys [list.hoverBackground] :dark "#2a2623" :light "#e2dcd4" :hc "#1f1a16"}
   {:token input :keys [input.background dropdown.background] :dark "#262320" :light "#ffffff" :hc "#000000"}
   {:token border :keys [panel.border editorGroup.border contrastBorder] :dark "#2d2926" :light "#dbd3c9" :hc "#ffa552"}
   {:token muted :keys [descriptionForeground tab.inactiveForeground] :dark "#968d83" :light "#6e655c" :hc "#cfc6bc"}
   {:token error :keys [editorError.foreground errorForeground] :dark "#ec6a5e" :light "#c8382a" :hc "#ff8a80"}
   {:token warning :keys [editorWarning.foreground] :dark "#d9a441" :light "#946800" :hc "#f0c75a"}
   {:token info :keys [editorInfo.foreground] :dark "#6fa8dc" :light "#2268a8" :hc "#8cc4f2"}
   {:token widget :keys [editorWidget.background quickInput.background] :dark "#23201d" :light "#f7f4f0" :hc "#1a0f05"}
   {:token text :keys [foreground sideBar.foreground editor.foreground] :dark "#ddd6cd" :light "#2d2925" :hc "#ffffff"}])

(defstate iconTheme
  {:file {:icon file :color "#98a0a9"}
   :folder {:icon folder :color "#d1a35e"}
   :extensions {:c {:icon file-code :color "#5ea7cf"}
                :cpp {:icon file-code :color "#5ea7cf"}
                :h {:icon file-code :color "#b48ad8"}
                :hpp {:icon file-code :color "#b48ad8"}
                :json {:icon braces :color "#d4b84a"}
                :md {:icon file-text :color "#5ea7cf"}
                :txt {:icon file-text :color "#98a0a9"}
                :cmake {:icon hammer :color "#7f9198"}
                :py {:icon file-code :color "#4c88bd"}
                :js {:icon file-code :color "#d4b84a"}
                :ts {:icon file-code :color "#5ea7cf"}
                :sh {:icon file-terminal :color "#6a8a74"}}
   :names {:CMakeLists.txt {:icon hammer :color "#7f9198"}
           :README.md {:icon info :color "#5ea7cf"}
           :LICENSE {:icon file-text :color "#d4b84a"}}})

(defcustom workbench.fileIcons
  true "Show the file icon theme's icons in the explorer." :description ~(t "settings.workbench.fileIcons.about") :type boolean :title ~(t "settings.workbench.fileIcons"))

;;; Functions and commands

(defcommand pickTheme "Choose the workbench's colour theme."
  :title ~(t "command.pickTheme") :category ~(t "category.preferences") :icon "palette"
  :description ~(t "command.pickTheme.about") []
  (set "state.quickPick.open" true)
  (set "state.quickPick.query" "theme "))

(defn setTheme "TODO" [item]
  (set "state.theme" item.id))

(defcommand importTheme "Read a colour theme (.json) and add it as a theme."
  :title ~(t "command.importTheme") :category ~(t "category.preferences") :icon "palette"
  :description ~(t "command.importTheme.about") []
  (openFile "state.imported"
            :comments true
            :title "Import Color Theme"
            :extensions ["json"]
            :onDone importThemeDone
            :onError importThemeFailed))

(defn readTheme "The colour theme read into state.imported: its name (else its file's), its type and its colours" []
  (readThemeOf state.imported state.importedFile))

(defn readThemeOf "TODO" [theme file]
  (if (not= (type theme) "object")
    (notify "info" "Theme not imported: not a colour theme: the file isn't a JSON object")
    (addTheme (cond
                (= (type theme.name) "string") theme.name
                (= (slice file (- 5)) ".json") (slice file 0 (- 5))
                :else file)
              (cond
                (not= (type theme.type) "string") "dark"
                (or (= theme.type "light") (= theme.type "vs")) "light"
                (or (= (slice theme.type 0 2) "hc") (= theme.type "high-contrast")) "hc"
                :else "dark")
              (if (= (type theme.colors) "object") theme.colors nil))))

(defn addTheme "Each token takes the first of its keys the theme gives a colour, else its default for the theme's kind; the theme's id is made of its name"
  [name kind colors]
  (addThemeAs (or (slug name 40) "imported")
              name
              (if (= kind "light") "light" "dark")
              (map (fn [it]
                     {:token it.token
                      :value (or-else (find (fn [it] (some? it)) (map (fn [it] (hexColor (get colors it))) it.keys))
                                      (get it kind))})
                   state.themeSources)))

(defn addThemeAs "TODO" [id name type values]
  (patch (map (fn [it] {:op "add" :path (str "/tokens/color." it.token "/" id) :value it.value}) values)
         :message (str "Import the " name " theme"))
  (set "state.themes" (concat (filter (fn [it] (not= it.id id)) state.themes) [{:id id :name name :type type}]))
  (set "state.theme" id)
  (notify "info" (str "Imported the " name " theme")))

(defcommand toggleIcons "Toggle File Icons"
  :title ~(t "command.toggleIcons") :category ~(t "category.preferences") :icon "file" []
  (set "settings.workbench.fileIcons" (not settings.workbench.fileIcons))
  (reindex))

(defn importThemeDone "What importTheme does when it is done." [result]
  (set "state.importedFile" result.name)
  (readTheme))

(defn importThemeFailed "What importTheme does when it fails." [error]
  (notify "info" (str "Theme not imported: " error.message)))
