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

;;; The image and text editors, which an area can show

(defview editor-header "An editor's header: its type's button (its menu open at the state path open), and its title." [editor icon title open]
  `(row :anchors {:left 0 :right 0 :top 0}
     :height 26
     :color @header
     :radius {:topLeft 8 :topRight 8}
     :alignItems center
     :gap 2
     (node :use gap)
     (node :id ~(str "editor-type-" editor)
       :use hButton
       :with {:icon ~icon
              :chevron true
              :width 38
              :tooltip ~(t "header.editorType")
              :open ~open
              :items ~(editor-choices editor)})
     (node :use gap :with {:width 6})
     (label :text ~title :textColor @dim :fitText true :height 20)))

(defcomponent imageArea "The image editor: the scene through its camera, as F12 renders it, kept up to date." {}
  `(node#image-area :color @region :radius 8
     ~(editor-header "image" "image" (t "image.title") "state.open.editorImage")
     (scene#image-view :anchors {:left 0 :right 0 :top 26 :bottom 0}
       :when ~(some? computed.cameraNode)
       :scene ~computed.renderScene
       :view ~{:camera computed.cameraView :selection []}
       :up z
       :extras false
       :shading material
       :lighting scene
       :exposure ~state.render.exposure
       :grid false
       :axes false
       :background ~state.world.color
       :controls {:orbit none :pan none :select none :zoom false :selects false}
       :label ~(t "render.label" {:camera (or-else computed.cameraNode.name (t "render.theCamera"))})
       :role img)
     (label#image-empty :when ~(nil? computed.cameraNode)
       :anchors {:left 0 :right 0 :top 26 :bottom 0}
       :text ~(t "image.noCamera")
       :textColor @dim
       :align center)))

(defcomponent textArea "The text editor: the scene's notes, saved with it." {}
  `(node#text-area :color @region :radius 8
     ~(editor-header "text" "file-text" (t "text.title") "state.open.editorText")
     (editor#notes :anchors {:left 0 :right 0 :top 26 :bottom 0}
       :bind state.scene.notes
       :lineNumbers true
       :wrap true
       :color @region
       :textColor @text
       :label ~(t "text.title"))))
