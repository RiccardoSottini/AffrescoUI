;;; index.screen.lisp --- Sinopia: slides and vector documents, written as a program
;;;
;;; The deck lives in state.deck (parts/sample-deck.lisp), its themes and layouts in parts/themes.lisp, its shapes in
;;; parts/geometry.lisp, and every slide is drawn by parts/slide.lisp, in the thumbnails and on the canvas alike. The
;;; window around it is parts/window.lisp; the ribbon is parts/ribbon.lisp and parts/tabs.lisp; editing is
;;; parts/edit.lisp and sinopia.lisp; the drop-downs, the panes and the show have a file each. Each file holds its
;;; views, then what its buttons do.

(ns sinopia.index
  (:require "locales/en.lisp"
            "locales/es.lisp"
            "locales/fr.lisp"
            "locales/design.en.lisp"
            "locales/design.es.lisp"
            "locales/design.fr.lisp"
            "locales/insert.en.lisp"
            "locales/insert.es.lisp"
            "locales/insert.fr.lisp"
            sinopia.parts.window
            sinopia.parts.keys
            sinopia.parts.themes
            sinopia.parts.geometry
            sinopia.parts.slide
            sinopia.parts.sample-deck
            sinopia.parts.clipboard
            sinopia.parts.edit
            sinopia.parts.arrange
            sinopia.parts.content
            sinopia.parts.chart
            sinopia.parts.picture
            sinopia.parts.ribbon
            sinopia.parts.ribbon-extras
            sinopia.parts.key-hints
            sinopia.parts.own-themes
            sinopia.parts.motion-draw
            sinopia.parts.tabs
            sinopia.parts.show
            sinopia.parts.motion
            sinopia.parts.ink
            sinopia.parts.notes-page
            sinopia.parts.views
            sinopia.parts.format
            sinopia.parts.shape-format
            sinopia.parts.canvas
            sinopia.parts.file
            sinopia.parts.design
            sinopia.parts.search
            sinopia.parts.menus
            sinopia.parts.review
            "../../shared/addons/assistant/assistant.lisp"
            sinopia.sinopia))

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Sinopia's window: the bars, the ribbon, the slide and its views; the show over them."
  :window {:title "Sinopia" :width 1280 :height 800 :minWidth 960 :minHeight 600}
  :strings {:default en :locale ~settings.appearance.language :keep ["Sinopia" "Aa"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 12}
          :mono {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 12 :fallback [icons]}
          :slideText {:file "../../shared/fonts/Roboto-Regular.ttf" :size 48 :fallback [icons]}}
  :persist [state.deck state.theme state.presenterView state.recentDecks state.ownThemes]
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
    ~(repeat [s (if (= state.view "notesPage") state.deck.slides []) :index n :key s.id]
      `(node :use notesPageImage :with {:s ~s :n ~n}))
    (node :use showStage :when ~state.show.on)))
