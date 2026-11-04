;;; settings.lisp --- Calamaio's variables: what C-h v describes and M-x customize changes
;;;
;;; A changed value is kept in state.settings, from one session to the next; the defaults stay here. Several
;;; variables share their name with the command that toggles them (menu-bar-mode is both, as in the editor Calamaio
;;; follows), and a program has one name for one thing: so they are the settings of one part, not defcustoms.

;;; The editor's variables

(defpart variables "The editor's variables, by the names the editor Calamaio follows gives them."
  :settings {:inhibit-startup-screen
             (boolean :default false
               :title ~(t "setting.inhibit-startup-screen") :description ~(t "setting.inhibit-startup-screen.about"))
             :custom-enabled-themes
             (string :default default :enum [default inchiostro pergamena]
               :title ~(t "setting.custom-enabled-themes") :description ~(t "setting.custom-enabled-themes.about"))
             :menu-bar-mode
             (boolean :default true
               :title ~(t "setting.menu-bar-mode") :description ~(t "setting.menu-bar-mode.about"))
             :tool-bar-mode
             (boolean :default true
               :title ~(t "setting.tool-bar-mode") :description ~(t "setting.tool-bar-mode.about"))
             :cursor-type
             (string :default box :enum [box bar hbar]
               :title ~(t "setting.cursor-type") :description ~(t "setting.cursor-type.about"))
             :global-display-line-numbers-mode
             (boolean :default false
               :title ~(t "setting.global-display-line-numbers-mode")
               :description ~(t "setting.global-display-line-numbers-mode.about"))
             :visual-line-mode
             (boolean :default false
               :title ~(t "setting.visual-line-mode") :description ~(t "setting.visual-line-mode.about"))
             :show-paren-mode
             (boolean :default true
               :title ~(t "setting.show-paren-mode") :description ~(t "setting.show-paren-mode.about"))
             :display-fill-column-indicator-mode
             (boolean :default false
               :title ~(t "setting.display-fill-column-indicator-mode")
               :description ~(t "setting.display-fill-column-indicator-mode.about"))
             :fill-column
             (integer :default 70 :minimum 8 :maximum 400
               :title ~(t "setting.fill-column") :description ~(t "setting.fill-column.about"))
             :search-highlight
             (boolean :default true
               :title ~(t "setting.search-highlight") :description ~(t "setting.search-highlight.about"))
             :kill-ring-max
             (integer :default 120 :minimum 1 :maximum 1000
               :title ~(t "setting.kill-ring-max") :description ~(t "setting.kill-ring-max.about"))})

;;; The language

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "setting.language") :description ~(t "setting.language.about"))
