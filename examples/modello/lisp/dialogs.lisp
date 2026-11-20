;;; dialogs.lisp --- what opens over the window: the menu that pops up, the pie menu, the About dialog, the render
;;; window and F3's search, and the timer that plays the animation

(ns modello.dialogs)

;;; Over the window

(defview playback-timer "A timer, while the animation plays: a frame each tick, as many a second as the output's frame rate." []
  `(node#playback :when ~(not= state.playing false) :width 0 :height 0 :every {:ms ~(round (/ 1000 computed.fps)) :do tick}))

(defview popup-menu "The menu that popped up: its own items, or a menu of menus.lisp by name." []
  `(node#popup :use contextMenu
     :with {:open state.ctx.open
            :x ~state.ctx.x
            :y ~state.ctx.y
            :items ~(or-else state.ctx.items (get menus state.ctx.menu) [])
            :icons ~(some #(some? (:icon %)) (or-else state.ctx.items (get menus state.ctx.menu) []))}))

(defview pie "The pie menu: the views, or the shading modes." []
  `(node#pie :use pieMenu
     :with {:open state.pie.open
            :x ~state.pie.x
            :y ~state.pie.y
            :items ~computed.pieItems
            :title ~(if (= state.pie.menu "view") (t "header.view") (t "viewport.shading"))}))

(defview about-dialog "The About dialog." []
  `(node#about-dialog :use dialog
     :with {:open state.about
            :title "Modello 1.0"
            :width 360
            :height 170
            :message ~(t "about.message")
            :buttons [{:title ~(t "dialog.close")}]}))

(defn render-height "The render window's height: its image, 720 wide, takes the resolution's shape." []
  (let [[w h] (or-else state.render.resolution [1920 1080])
        tall (round (* 720 (/ h w)))]
    (clamp (+ 100 tall) 260 720)))

(defview render-window "The render window: the render, and a button that saves it." []
  `(node#render-window :use dialog
     :with {:open state.rendering
            :title ~(t "dialog.render")
            :width 760
            :height ~(render-height)
            :buttons [{:title ~(t "dialog.close")} {:title ~(t "dialog.saveImage") :run saveRender :keepOpen true}]}
     ~(render-image)))

(defview command-search "F3's search: every command that can run now." []
  `(node#search :when ~state.quickPick.open
     :use quickPick
     :with {:width 460 :rows 12 :rowHeight 22 :providers [{:items ~computed.searchItems :placeholder ~(t "search.placeholder")}]}))
