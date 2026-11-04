;;; index.screen.lisp --- Orrery: a small planetarium, planets going round a sun, in 3D and on a sky chart
;;;
;;; The 3D view on the left, with a browser of the bodies, places and eclipses over it; the sky chart (from above, or
;;; the sky from the ground) and the orbit graph on the right; the timeline of events and the bodies strip under them.
;;; The strip is Sinopia's slide sorter, installed through the bodies.dock point (offers.lisp, features.lisp). What it does is in
;;; orrery.lisp. Space plays, J reverses, K and L slow down and speed up, G goes to the selected body, C centres it,
;;; F follows it, B shows the browser, Cmd+D bookmarks the view.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "offers.lisp"
         "features.lisp"
         "state.lisp"
         "bodies.lisp"
         "time.lisp"
         "look.lisp"
         "keys.lisp"
         "bars.lisp"
         "view.lisp"
         "browser.lisp"
         "chart.lisp"
         "sky.lisp"
         "orrery.lisp"
         "../../shared/addons/assistant/assistant.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main
  "Orrery's one window: the bars, the view beside the chart, the timeline, the bodies strip, the status bar."
  :window {:title ~(t "app.title")
           :width 1280
           :height 860
           :minWidth 1000
           :minHeight 700
           :menuBar [{:menu view :title ~(t "menu.view")} {:menu time :title ~(t "menu.time")}]}
  :strings {:default en :locale ~settings.appearance.language :keep ["Orrery"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 11 :fallback [icons]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 18 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 14}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio user]}
  :theme ~state.theme
  (node#root :color @background :every {:ms 50 :do tick}
    (column#layout :fill true
      ~(top-bar)
      (row#main :width "100%" :grow 1 :gap 1 :color @border ~(view-pane) ~(side))
      ~(timeline)
      (column#dock :point bodies.dock :width "100%" :height 136 :color @panel :children [])
      ~(status-bar))
    ~(render-dialog)))
