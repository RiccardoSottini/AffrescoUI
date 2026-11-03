;;; index.screen.lisp --- Tempera: an image editor, with layers, masks, adjustments, filters and colour profiles
;;;
;;; The menu bar and the options bar of the current tool across the top; the tools bar, the canvas with its rulers and
;;; status bar, and the dock of panels across the middle. The pixels live in the library's image doc (image.lisp):
;;; the canvas shows it, the image step changes it (tempera.lisp), and the library puts its snapshot in state.doc. V,
;;; M, L, W, C, B, E, S, G, I, H and Z choose a tool; Cmd+Z undoes, Cmd+J duplicates a layer, Cmd+L, M, U, B and I add
;;; an adjustment layer, Cmd+F searches the commands, F changes the screen mode (keys.lisp).

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "offers.lisp"
         "state.lisp"
         "image.lisp"
         "computed.lisp"
         "look.lisp"
         "commands.lisp"
         "keys.lisp"
         "menus.lisp"
         "controls.lisp"
         "menu.lisp"
         "menubar.lisp"
         "options.lisp"
         "tools.lisp"
         "canvas.lisp"
         "panels.lisp"
         "properties.lisp"
         "layers.lisp"
         "dialogs.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "../../shared/components/charts.lisp"
         "tempera.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Tempera's one window: the bars, the tools, the canvas and the dock, then what opens over them."
  :window {:title "Tempera" :width 1280 :height 800 :minWidth 900 :minHeight 600}
  :strings {:default en :locale ~settings.appearance.language :keep ["Tempera"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons symbols]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 10 :fallback [icons]}
          :symbols {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 11}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 17}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio user]}
  :persist [state.theme state.view]
  :theme ~state.theme
  (node#root :color @background
    (column#workspace :fill true :gap 6
      ~(menu-bar)
      ~(options-bar)
      (row#main :anchors {:left 6 :right 6} :grow 1 :gap 6
        ~(tool-bar)
        ~(canvas-area)
        ~(dock))
      (node#workspace-foot :style clear :width 1 :height 0))
    (node :use quickPick
      :with {:width 520 :providers ~[{:placeholder (t "search.placeholder") :items computed.searchItems}]})
    ~(filter-dialog)
    ~(new-dialog)
    ~(transform-dialog)
    ~(image-size-dialog)
    ~(canvas-size-dialog)
    ~(rename-dialog)
    ~(fill-dialog)
    ~(close-dialog)
    ~(about-dialog)))
