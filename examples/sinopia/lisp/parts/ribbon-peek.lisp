;;; ribbon-peek.lisp --- The ribbon while it is collapsed: a tab shows it over the slide, a click elsewhere or Escape
;;; hides it again, and its pin keeps it open in its place
;;;
;;; The ribbon's tabs are the same views as the pinned ribbon's (ribbon-tabs, in window.lisp); only one of the two is
;;; ever built.

(ns sinopia.parts.ribbon-peek)

;;; Definitions

(defstate ribbonPeek false "Whether the collapsed ribbon is shown over the slide, after a click on a tab.")

(defcomponent ribbonOverlay "The collapsed ribbon, shown over the slide under the command bar, with its pin." {}
  `(node#ribbon-peek :overlay {:placement window :open state.ribbonPeek :dismiss outside}
     :anchors {:left 0 :right 0 :top 45}
     :height 100
     :color @rb
     :shadow {:y 6 :blur 16 :color "#00000038"}
     :role toolbar
     :label ~(t "bar.ribbon")
     ~(ribbon-tabs)
     (label#ribbon-pin :style barIcon
       :anchors {:right 10 :bottom 2}
       :width 22
       :height 22
       :runs [{:icon pin :size 13}]
       :tooltip ~(t "bar.pinTheRibbon")
       :onClick pin-ribbon)))

(defpart ribbon-peek "The collapsed ribbon over the slide."
         :overlays {:zRibbon (node :use ribbonOverlay :when ~(and state.ribbonPeek (not state.ribbon)))})

;;; What its commands do

(defn pin-ribbon "Keeps the ribbon open in its place, above the slide." []
  (set "state.ribbon" true)
  (set "state.ribbonPeek" false))

(defn toggle-ribbon "Collapses the ribbon, or pins it open again." []
  (set "state.ribbonPeek" false)
  (set "state.ribbon" (not state.ribbon)))
