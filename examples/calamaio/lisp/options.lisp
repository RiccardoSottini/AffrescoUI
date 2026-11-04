;;; options.lisp --- the Options menu: the minor modes, the theme, and Customize, the variables in a buffer of their own
;;;
;;; Each minor mode is a variable of settings.lisp that its command of the same name toggles; Customize lists the
;;; variables to change and reset, and the theme is the variable custom-enabled-themes.

;;; Minor modes

(defn mode-said "Says whether a minor mode is now on or off." [name on]
  (message (if on (t "echo.modeOn" {:name name}) (t "echo.modeOff" {:name name}))))

(defcommand display-line-numbers-mode "Shows or hides the line numbers in the margin."
  :title ~(t "command.display-line-numbers-mode") :category ~(t "category.options")
  :description ~(t "command.display-line-numbers-mode.about")
  []
  (toggle "settings.global-display-line-numbers-mode")
  (mode-said (t "mode.lineNumbers") settings.global-display-line-numbers-mode))

(defcommand visual-line-mode "Wraps the lines that don't fit the window, or lets them run past its edge."
  :title ~(t "command.visual-line-mode") :category ~(t "category.options")
  :description ~(t "command.visual-line-mode.about")
  []
  (toggle "settings.visual-line-mode")
  (mode-said (t "mode.lineWrapping") settings.visual-line-mode))

(defcommand show-paren-mode "Highlights the bracket that pairs with the one at point, or stops."
  :title ~(t "command.show-paren-mode") :category ~(t "category.options")
  :description ~(t "command.show-paren-mode.about")
  []
  (toggle "settings.show-paren-mode")
  (mode-said (t "mode.bracketMatching") settings.show-paren-mode))

(defcommand menu-bar-mode "Shows or hides the menu bar; F10 still opens its menus."
  :title ~(t "command.menu-bar-mode") :category ~(t "category.options")
  :description ~(t "command.menu-bar-mode.about")
  []
  (toggle "settings.menu-bar-mode")
  (mode-said (t "mode.menuBar") settings.menu-bar-mode))

(defcommand tool-bar-mode "Shows or hides the tool bar."
  :title ~(t "command.tool-bar-mode") :category ~(t "category.options")
  :description ~(t "command.tool-bar-mode.about")
  []
  (toggle "settings.tool-bar-mode")
  (mode-said (t "mode.toolBar") settings.tool-bar-mode))

;;; The theme

(def themes "The themes look.lisp colours." ["default" "inchiostro" "pergamena"])

(defcommand load-theme "Switches the colours to another theme: default, inchiostro or pergamena."
  :title ~(t "command.load-theme") :category ~(t "category.options")
  :description ~(t "command.load-theme.about")
  []
  (minibuffer-read "theme" (t "prompt.loadTheme") "" ""))

(defn mb-theme "Switches to the theme named." [value]
  (if (contains themes value)
    (set "settings.custom-enabled-themes" value)
    (message (t "echo.noTheme" {:name value}))))

;;; Customize

(defn customize-show "Shows the variables whose name holds a text, in the *Customize* buffer." [q]
  (set "state.customQuery" q)
  (listing-show "bc" {:name "*Customize*" :mode "customize" :kind "customize" :readOnly true :text ""}))

(defcommand customize "Shows Calamaio's variables in a buffer, to change and reset them."
  :title ~(t "command.customize") :category ~(t "category.options")
  :description ~(t "command.customize.about")
  []
  (customize-show ""))

(defcommand customize-startup "Shows the variables of what Calamaio does at start."
  :title ~(t "command.customize-startup") :category ~(t "category.options")
  :description ~(t "command.customize-startup.about")
  []
  (customize-show "startup"))
