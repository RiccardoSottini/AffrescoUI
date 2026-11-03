;;; index.screen.lisp --- Quaderno: a code editor
;;;
;;; A menu bar, an explorer tree, editor tabs with unsaved dots (closing an unsaved one asks first), a generated
;;; 5,000-line file, the Quick Switcher (Cmd+P; file@symbol, file:line), symbols in the project (Cmd+Shift+R), Find in
;;; Files (Cmd+Shift+F, F4 for the next result), every command in a palette (Cmd+Shift+P), the TODO and FIXME lines as
;;; problems (updated on save), and a terminal. A find panel (Cmd+F, Cmd+Alt+F to replace), word wrap (Alt+Cmd+W) and
;;; a ruler at column 80 (View); multiple selections, Cmd+/ comments, line operations.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "parts/look.lisp"
         "state.lisp"
         "computed.lisp"
         "keys.lisp"
         "window.lisp"
         "explorer.lisp"
         "editor.lisp"
         "panel.lisp"
         "../../shared/languages/languages.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "quaderno.lisp")

(defpart events "Quaderno's start and its watch: the files are prepared, and the assistant sits above the find panel."
  :onLoad prepare
  :watch {:state.findBar [~(handler liftAssistant)]})

;;; The screen

(defscreen main
  :window {:title "Quaderno: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640
           :focusRing false}
  :strings {:default en :locale ~settings.appearance.language :keep ["Quaderno" "Quaderno: an AffrescoUI template"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons code]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons code]}
          :code {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 14}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}}
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#statusBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~computed.theme
  (node#panel-root :color @background
    ~(title-bar)
    (split#workbench :dir ~state.dir
      :anchors {:left 0 :right 0 :top ~(if state.zen 0 26) :bottom ~(+ (if state.zen 0 24) computed.findHeight)}
      :bind state.sizes
      :defaultSizes [232]
      :minSize [160 480]
      :sashSize 2
      :sashColor @border
      :sashActiveColor @accent
      ~(explorer)
      ~(editor-column))
    ~(find-panel)
    ~(status-bar)
    ~(close-dialog)
    ~(about-dialog)
    ~(command-palette)
    ~(symbol-pick)
    ~(quick-pick)
    ~(tab-menu)
    ~(editor-menu)
    ~(theme-pick)
    ~(syntax-pick)))
