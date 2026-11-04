;;; index.screen.lisp --- Sinopia: slides and vector documents, written as a program
;;;
;;; The deck lives in state.deck (parts/sample-deck.lisp), its themes and layouts in parts/themes.lisp, its shapes in
;;; parts/geometry.lisp, and every slide is drawn by parts/slide.lisp, in the thumbnails and on the canvas alike. The
;;; window around it is parts/window.lisp; the ribbon is parts/ribbon.lisp and parts/tabs.lisp; editing is
;;; parts/edit.lisp and sinopia.lisp; the drop-downs, the panes and the show have a file each. Each file holds its
;;; views, then what its buttons do.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "parts/window.lisp"
         "parts/keys.lisp"
         "parts/themes.lisp"
         "parts/geometry.lisp"
         "parts/slide.lisp"
         "parts/sample-deck.lisp"
         "parts/edit.lisp"
         "parts/ribbon.lisp"
         "parts/tabs.lisp"
         "parts/show.lisp"
         "parts/views.lisp"
         "parts/menus.lisp"
         "parts/review.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "sinopia.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Sinopia's window: the bars, the ribbon, the slide and its views; the show over them."
  :window {:title "Sinopia" :width 1280 :height 800 :minWidth 960 :minHeight 600}
  :strings {:default en :locale ~settings.appearance.language :keep ["Sinopia" "Aa"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 12}
          :mono {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 12 :fallback [icons]}
          :slideText {:file "../../shared/fonts/Roboto-Regular.ttf" :size 48 :fallback [icons]}}
  :persist [state.deck state.theme state.presenterView]
  :permissions {:allow [studio] :ask [agent] :reviewers [studio]}
  :theme ~state.theme
  (node#root :color @work
    (column#window :fill 0 :color @work :alignItems stretch
      ~(command-bar)
      ~(ribbon)
      (node#ribbon-line :height 1 :color @barLine)
      ~(work-area)
      (node#status-line :height 1 :color @barLine)
      ~(status-bar))
    ~(repeat [s state.deck.slides :key s.id]
      `(node :use slideImage :with {:s ~s}))
    (node :use showStage :when ~state.show.on)))
