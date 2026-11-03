;;; viewport.lisp --- the 3D viewport: the scene, its header (editor type, mode, menus, orientation, pivot, snapping,
;;; visibility, gizmos, overlays, X-ray and shading), the toolbar on the left, the navigation gizmo on the right, the
;;; view's name and counts, the Measure tool's line, and the Adjust Last Operation panel
;;;
;;; The sidebar (N) is in sidebar.lisp; what a click and a transform do, in transform.lisp.

;;; The area

(defcomponent viewportArea "The 3D viewport's area: the scene, and everything drawn over it." {}
  `(node#viewport-area :color @view :radius 8
     ~(viewport-scene)
     (node :use viewportHeader)
     (node :when ~(not= state.toolbar false) :use viewportToolbar)
     (node :when ~computed.measureShown :use measureLine)
     ~@(view-texts)
     ~(measure-hint)
     (node :when ~(and state.gizmos (not= state.navGizmo false)) :use navigator)
     (node :when ~state.sidebar :use viewportSidebar)
     (node#sidebar-open :when ~(not state.sidebar)
       :use hButton
       :anchors {:right 0 :top 44}
       :with {:icon chevron-left :width 14 :back transparent :tooltip ~(t "viewport.sidebarTip") :run toggleSidebar :textColor @dim})
     (label#modal-hint :when ~(some? state.modal) :live polite :text ~state.modalText :textColor @text :anchors {:left 52 :bottom 12} :fitText true)
     (node :when ~(and (nil? state.modal) (some? state.lastOp)) :use redoPanel)))

(defview viewport-scene "The scene, as the viewport's shading, overlays and gizmo show it; a click picks, a drag transforms." []
  `(scene#viewport :fill true
     :bind state.view
     :scene ~computed.displayScene
     :up z
     :glyphs true
     :cursor3d ~(if (and state.overlays state.overlay.cursor) state.cursor nil)
     :shading ~(if (= state.shading "rendered") "material" state.shading)
     :lighting ~(if (and (= state.shading "material") (not computed.lit)) "studio" "scene")
     :xray ~(if state.xray 0.5 0)
     :exposure ~state.render.exposure
     :grid ~(and state.overlays state.overlay.grid)
     :axes ~(and state.overlays state.overlay.axes)
     :background ~(if computed.worldShown state.world.color "@view")
     :gridColor @grid
     :outlineColor @selected
     :gizmo ~computed.gizmo
     :controls {:selects false}
     :label ~(t "editor.viewport")
     :role img
     :onSelect pick
     :onTransform transformed))

;;; What is written over the scene

(defcomputed viewName "The view's name in its corner: which side it looks from, and its projection."
  (if state.inCamera
    (t "view.camera")
    (t (if state.view.camera.orthographic "view.orthographic" "view.perspective")
       {:side (t (cond (< (abs (- (abs state.view.camera.pitch) (/ (pi) 2))) 0.001)
                         (if (> state.view.camera.pitch 0) "side.top" "side.bottom")
                       (> (abs state.view.camera.pitch) 0.001) "side.user"
                       (< (abs state.view.camera.yaw) 0.001) "side.front"
                       (< (abs (- state.view.camera.yaw (/ (pi) 2))) 0.001) "side.right"
                       (< (abs (+ state.view.camera.yaw (/ (pi) 2))) 0.001) "side.left"
                       (< (abs (- (abs state.view.camera.yaw) (pi))) 0.001) "side.back"
                       :else "side.user"))})))

(defview view-texts "The view's name, the frame and the active object's collection and name, and the counts: three labels." []
  [`(label#view-name :when ~(and state.overlays state.overlay.text) :text ~computed.viewName :textColor @text :anchors {:left 54 :top 42} :fitText true)
   `(label#view-collection :when ~(and state.overlays state.overlay.text)
      :text ~(str "(" state.frame ") "
                  (if computed.activeNode
                    (str (or-else (:name (find (fn [it] (= it.id (or-else computed.activeNode.collection "collection"))) computed.collections))
                                  (t "collection.default"))
                         " | "
                         computed.activeNode.name)
                    computed.activeCollectionData.name))
      :textColor @text
      :anchors {:left 54 :top 58}
      :fitText true)
   `(label#view-stats :when ~(and state.overlays state.overlay.stats)
      :text ~(t "viewport.stats" {:selected (count state.view.selection)
                                  :objects (count state.scene.nodes)
                                  :lights (count (filter (fn [it] (= it.kind "light")) state.scene.nodes))
                                  :cameras (count (filter (fn [it] (= it.kind "camera")) state.scene.nodes))})
      :textColor @text
      :anchors {:left 54 :top ~(if state.overlay.text 78 42)}
      :fitText true)])

(defview measure-hint "What the Measure tool asks for, or the distance it measured." []
  `(label#measure-hint :when ~(= state.tool "measure")
     :live polite
     :text ~(cond (some? computed.measured) (t "measure.distance" {:length (fixed computed.measured 3)})
                  (= (count state.measure.points) 1) (t "measure.second")
                  :else (t "measure.two"))
     :textColor @text
     :anchors {:left 52 :bottom 40}
     :fitText true))

;;; The Measure tool

(defcomputed measureShown "Whether the Measure tool's line is shown: it has points, picked from this view."
  (and (= state.tool "measure") (> (count state.measure.screen) 0) (= state.measure.camera state.view.camera)))

(defcomputed measured "The distance between the two points measured, or nil."
  (if (= (count state.measure.points) 2)
    (sqrt (+ (pow (- state.measure.points.1.0 state.measure.points.0.0) 2)
             (pow (- state.measure.points.1.1 state.measure.points.0.1) 2)
             (pow (- state.measure.points.1.2 state.measure.points.0.2) 2)))
    nil))

(defcomponent measureLine "The Measure tool's points, the line between them, and its length." {}
  `(node#measure :x 0
     :y 0
     :width 0
     :height 0
     :overflow visible
     :color transparent
     :role img
     :label ~(if (some? computed.measured) (t "measure.measured" {:length (fixed computed.measured 3)}) (t "measure.measuring"))
     ~(measure-ruler)
     ~(repeat [p state.measure.screen]
       `{:x ~(- p.0 4) :y ~(- p.1 4) :width 8 :height 8 :radius 4 :color @active})
     (label#measure-length :when ~(some? computed.measured)
       :text ~(t "measure.length" {:length (fixed computed.measured 3)})
       :textColor @text
       :color @toolGroup
       :radius 4
       :x ~(+ (/ (+ state.measure.screen.0.0 state.measure.screen.1.0) 2) 6)
       :y ~(- (/ (+ state.measure.screen.0.1 state.measure.screen.1.1) 2) 22)
       :fitText true
       :padding 4)))

(defview measure-ruler "The line between the two points, turned to join them." []
  `{:when ~(= (count state.measure.screen) 2)
    :x ~state.measure.screen.0.0
    :y ~(- state.measure.screen.0.1 1)
    :height 2
    :color @active
    :width ~(hypot (- state.measure.screen.1.0 state.measure.screen.0.0) (- state.measure.screen.1.1 state.measure.screen.0.1))
    :rotate ~(degrees (atan2 (- state.measure.screen.1.1 state.measure.screen.0.1) (- state.measure.screen.1.0 state.measure.screen.0.0)))
    :transformOrigin [0 50]})

;;; Adjust Last Operation

(defcomponent redoPanel "The Adjust Last Operation panel: the last operation's name, and its amounts to change." {}
  `(node#redo-panel :anchors {:left 48 :bottom 8}
     :width 230
     :height ~(if (and state.redoOpen (some? state.lastOp.values)) 94 24)
     :color @panel
     :radius 8
     :role region
     :label ~(t "command.adjustLastPanel")
     ~(redo-header)
     (node :when ~(and state.redoOpen (some? state.lastOp.values)) :anchors {:left 4 :right 0 :top 26} :height 62 :color transparent
       (node :use vectorField
         :with {:title ~(or-else state.lastOp.label
                                 (cond (= state.lastOp.kind "scale") (t "op.scale")
                                       (= state.lastOp.kind "rotate") (t "op.rotation")
                                       :else (t "op.move")))
                :values ~state.lastOp.values
                :key redo
                :unit ~(cond (= state.lastOp.kind "scale") "" (= state.lastOp.kind "rotate") "°" :else " m")
                :step ~(cond (= state.lastOp.kind "scale") 0.005 (= state.lastOp.kind "rotate") 0.5 :else 0.01)
                :digits ~(if (= state.lastOp.kind "rotate") 1 3)
                :exact ~(= state.lastOp.kind "scale")}))))

(defview redo-header "The panel's header: a chevron that opens or closes it, and the operation's name." []
  `(node#redo-header :anchors {:left 0 :right 0 :top 0}
     :height 24
     :color transparent
     :role button
     :focusable true
     :cursor hand
     :label ~state.lastOp.title
     :onClick {:toggle state.redoOpen}
     (label :font iconsSmall
       :icon ~(if state.redoOpen "chevron-down" "chevron-right")
       :text ""
       :textColor @dim
       :align center
       :anchors {:left 4 :top 0 :bottom 0}
       :width 14)
     (label :text ~state.lastOp.title :textColor @text :align {:horizontal start :vertical center} :anchors {:left 22 :right 4 :top 0 :bottom 0})))

;;; The header

(defcomponent viewportHeader "The viewport's header: modes and menus, the transform's settings, and what is displayed." {}
  `(row#viewport-header :anchors {:left 6 :right 6 :top 6} :height 28 :color transparent :alignItems center :gap 0
     ~(header-modes)
     {:grow 1 :height 1 :color transparent}
     ~(header-transform)
     {:grow 1 :height 1 :color transparent}
     ~(header-display)))

(defview header-modes "The editor type, the mode, and the View, Select, Add and Object menus." []
  `(row#header-modes :height 28 :fitContent width :color @toolGroup :radius 8 :alignItems center :gap 2
     :role toolbar
     :label ~(t "viewport.modes")
     (node :use gap)
     (node#editor-type :use hButton
       :with {:icon axis-3-d
              :chevron true
              :width 38
              :tooltip ~(t "header.editorType")
              :open state.open.editor0
              :items ~(editor-choices "viewport")})
     (node :use gap :with {:width 6})
     (node#mode :use hButton
       :with {:icon ~(if (= state.mode "edit") "pen-tool" "box")
              :text ~(if (= state.mode "edit") (t "mode.edit") (t "mode.object"))
              :chevron true
              :width 116
              :tooltip ~(t "viewport.modeTip")
              :open state.open.mode
              :items ~computed.modeItems})
     (node :use gap :with {:width 6})
     ~(repeat [m ["view" "select" "add" "object"]] (header-menu m))
     (node :use gap)))

(defview header-menu "One of the header's menus, a text button as wide as its title." [m]
  `(node :id ~(str "menu-" m)
     :use hButton
     :with {:text ~(t (str "header." m))
            :width ~(+ (* (count (t (str "header." m))) 7) 14)
            :back transparent
            :open ~(str "state.open." m)
            :items ~(get menus m)
            :menuWidth 230}))

(defview header-transform "The transform's orientation and pivot, snapping and its target, and proportional editing." []
  `(row#header-transform :height 28 :fitContent width :color @toolGroup :radius 8 :alignItems center :gap 2
     :role toolbar
     :label ~(t "viewport.transformSettings")
     (node :use gap)
     (node#orientation :use hButton
       :with {:icon axis-3-d
              :text ~(t (str "orientation." state.orientation))
              :chevron true
              :width 86
              :tooltip ~(t "viewport.orientationTip")
              :open state.open.orientation
              :items ~computed.orientationItems
              :menuWidth 160})
     (node#pivot :use hButton
       :with {:icon ~(cond (= state.pivot "cursor") "crosshair"
                           (= state.pivot "individual") "dot"
                           (= state.pivot "active") "circle-dot"
                           (= state.pivot "bounds") "square-dashed-bottom"
                           :else "git-commit-horizontal")
              :chevron true
              :width 38
              :tooltip ~(t "viewport.pivotTip")
              :open state.open.pivot
              :items ~computed.pivotItems
              :menuWidth 190})
     (node :use gap)
     ~@(snap-buttons)
     (node :use gap)
     (node#proportional :use hButton :with {:icon circle-dot-dashed :width 24 :tooltip ~(t "viewport.proportionalTip") :disabled true})
     (node :use gap)))

(defview snap-buttons "Snapping on or off, and what it snaps to: two halves of one button." []
  [`(node#snap :use hButton
      :with {:icon magnet :on ~state.snap :width 24 :tooltip ~(t "viewport.snapTip") :run toggleSnap :radius {:topLeft 4 :bottomLeft 4}})
   `(node#snap-target :use hButton
      :with {:icon grid-3-x-3
             :chevron true
             :width 34
             :tooltip ~(t "viewport.snapTargetTip")
             :open state.open.snapTarget
             :items ~computed.snapItems
             :menuWidth 190
             :radius {:topRight 4 :bottomRight 4}})])

(defview header-display "What is shown: visibility, gizmos, overlays, X-ray and the shading." []
  `(row#header-display :height 28 :fitContent width :color @toolGroup :radius 8 :alignItems center :gap 2
     :role toolbar
     :label ~(t "viewport.display")
     (node :use gap)
     (node#visibility :use hButton
       :with {:icon eye
              :chevron true
              :width 38
              :tooltip ~(t "viewport.visibilityTip")
              :open state.open.visibility
              :items ~computed.visibilityItems
              :menuWidth 180})
     (node :use gap)
     ~@(gizmo-buttons)
     (node :use gap)
     ~@(overlay-buttons)
     (node :use gap)
     (node#xray :use hButton :with {:icon square-dashed-bottom :on ~state.xray :width 24 :tooltip ~(t "viewport.xrayTip") :run toggleXray})
     (node :use gap)
     ~(repeat [s computed.shadingModes :index i] (shading-button s i))
     (node#shading-menu :use hButton
       :with {:icon ""
              :chevron true
              :width 16
              :tooltip ~(t "viewport.shading")
              :open state.open.shading
              :items ~computed.shadingItems
              :menuWidth 230
              :radius {:topRight 4 :bottomRight 4}})
     (node :use gap)))

(defview gizmo-buttons "The gizmos shown or not, and the dropdown of which: two halves of one button." []
  [`(node#gizmos :use hButton
      :with {:icon move-3-d
             :on ~state.gizmos
             :width 24
             :tooltip ~(t "viewport.gizmoTip")
             :run toggleOption
             :with {:key gizmos}
             :radius {:topLeft 4 :bottomLeft 4}})
   `(node#gizmos-menu :use hButton
      :with {:icon ""
             :chevron true
             :width 16
             :tooltip ~(t "viewport.gizmos")
             :open state.open.gizmos
             :items ~computed.gizmoItems
             :menuWidth 180
             :radius {:topRight 4 :bottomRight 4}})])

(defview overlay-buttons "The overlays shown or not, and the dropdown of which: two halves of one button." []
  [`(node#overlays :use hButton
      :with {:icon blend :on ~state.overlays :width 24 :tooltip ~(t "viewport.overlaysTip") :run toggleOverlays :radius {:topLeft 4 :bottomLeft 4}})
   `(node#overlays-menu :use hButton
      :with {:icon ""
             :chevron true
             :width 16
             :tooltip ~(t "viewport.overlays")
             :open state.open.overlays
             :items ~computed.overlayItems
             :menuWidth 190
             :radius {:topRight 4 :bottomRight 4}})])

(defview shading-button "One of the four shading modes, as a button; the first rounds the group's start." [s i]
  `(node :id ~(str "shading-" s.id)
     :use hButton
     :with {:icon ~s.icon
            :on ~(= state.shading s.id)
            :width 24
            :tooltip ~(t "viewport.shadingTip" {:mode s.title})
            :run setOption
            :with {:key shading :value ~s.id}
            :radius ~(if (= i 0) {:topLeft 4 :bottomLeft 4} 0)}))

;;; The toolbar

(defcomputed tools "The toolbar's tools, in groups; Transform and Annotate are to come."
  [[{:id "select" :icon "mouse-pointer-2" :title (t "tool.select") :key "W"}]
   [{:id "cursor" :icon "crosshair" :title (t "tool.cursor") :key ""}]
   [{:id "move" :icon "move" :title (t "tool.move") :key "G"}
    {:id "rotate" :icon "rotate-cw" :title (t "tool.rotate") :key "R"}
    {:id "scale" :icon "scale-3-d" :title (t "tool.scale") :key "S"}
    {:id "transform" :icon "move-3-d" :title (t "tool.transform") :key "" :disabled true}]
   [{:id "annotate" :icon "pencil" :title (t "tool.annotate") :key "" :disabled true}
    {:id "measure" :icon "ruler" :title (t "tool.measure") :key ""}]
   [{:id "add" :icon "box" :title (t "tool.add") :key ""}]])

(defcomputed lens "The view's lens, in millimetres, from its field of view." (/ 18 (tan (/ state.view.camera.fieldOfView 2))))

(defcomputed toolName "The active tool's name." (:title (find (fn [it] (= it.id state.tool)) (flat computed.tools))))

(defcomponent viewportToolbar "The toolbar on the left: the tools in groups, a line between groups." {}
  `(column#toolbar :anchors {:left 8 :top 42}
     :width 34
     :height ~(- (sum computed.tools (fn [it] (+ (* (count it) 30) 9))) 3)
     :gap 0
     :color @toolGroup
     :radius 8
     :role toolbar
     :label ~(t "viewport.tools")
     :arrows vertical
     ~(repeat [group computed.tools :index g] (tool-group group g))))

(defview tool-group "A group of tools, and a line under it but the last." [group g]
  `(column :width 34
     :height ~(+ (* (count group) 30) (if (< g (- (count computed.tools) 1)) 9 6))
     :color transparent
     :gap 0
     {:height ~(if (= g 0) 3 0) :width 1 :color transparent}
     ~(repeat [tool group] (tool-button tool))
     (node :when ~(< g (- (count computed.tools) 1)) :height 9 :width 34 :color transparent
       {:x 9 :y 4 :width 16 :height 1 :color @faint})))

(defview tool-button "A tool: chosen with a click; its tooltip says its key, or that it is to come." [tool]
  `(node :id ~(str "tool-" tool.id)
     :use hButton
     :x 2
     :with {:icon ~tool.icon
            :on ~(= state.tool tool.id)
            :width 30
            :radius 6
            :tooltip ~(cond (= tool.disabled true) (t "tool.notYet" {:title tool.title})
                            (not= tool.key "") (t "tool.withKey" {:title tool.title :key tool.key})
                            :else tool.title)
            :back transparent
            :disabled ~(= tool.disabled true)
            :run setTool
            :with {:tool ~tool.id}}
     :height 30))

;;; The navigation gizmo

(defcomputed nav "The view's turn, as the sines and cosines the gizmo's axes are drawn with."
  {:cy (cos state.view.camera.yaw)
   :sy (sin state.view.camera.yaw)
   :cp (cos state.view.camera.pitch)
   :sp (sin state.view.camera.pitch)})

(defcomputed navAxes "The gizmo's six axis ends, where they are drawn as the view turns, the farthest first."
  ;; sort-by with a function giving "d" is the expressions' sort by the key d.
  (sort-by (fn [it] "d")
           [{:name "x" :label "X" :x computed.nav.cy :y (* computed.nav.sp computed.nav.sy) :d (* computed.nav.cp computed.nav.sy)
             :color "#e8604c" :view "right"}
            {:name "-x" :label "" :x (- computed.nav.cy) :y (* (- computed.nav.sp) computed.nav.sy)
             :d (* (- computed.nav.cp) computed.nav.sy) :color "#e8604c40" :view "left"}
            {:name "y" :label "Y" :x computed.nav.sy :y (* (- computed.nav.sp) computed.nav.cy) :d (* (- computed.nav.cp) computed.nav.cy)
             :color "#4cbf7a" :view "back"}
            {:name "-y" :label "" :x (- computed.nav.sy) :y (* computed.nav.sp computed.nav.cy) :d (* computed.nav.cp computed.nav.cy)
             :color "#4cbf7a40" :view "front"}
            {:name "z" :label "Z" :x 0 :y (- computed.nav.cp) :d computed.nav.sp :color "#5b74f0" :view "top"}
            {:name "-z" :label "" :x 0 :y computed.nav.cp :d (- computed.nav.sp) :color "#5b74f040" :view "bottom"}]))

(defcomputed navButtons "The buttons under the gizmo: zoom, pan, the camera's view, and the projection."
  [{:id "zoom" :icon "zoom-in" :title (t "nav.zoom")}
   {:id "pan" :icon "hand" :title (t "nav.pan")}
   {:id "camera" :icon "video" :title (t "nav.camera")}
   {:id "ortho" :icon "grid-3-x-3" :title (t "nav.ortho")}])

(defcomponent navigator "The navigation gizmo, and its buttons under it." {}
  `(node#navigator :dir ltr :anchors {:right ~(if state.sidebar 254 14) :top 44} :width 84 :height 212 :color transparent
     ~(nav-ball)
     (column#nav-buttons :anchors {:top 94} :x 28 :width 28 :height 112 :gap 0 :color @navButton :radius 8
       ~(repeat [b computed.navButtons] (nav-button b)))))

(defview nav-ball "The ball: dragged, it turns the view; its axes, drawn to their ends, look along them when clicked." []
  `(node#nav-ball :width 84
     :height 84
     :radius 14
     :color transparent
     :hover {:color @navBall}
     :role slider
     :label ~(t "nav.ball")
     :onPress {:set state.dragLast :to [0 0]}
     :onDrag orbitByDrag
     ~(repeat [a (filter (fn [it] (not= it.label "")) computed.navAxes) :key a.name] (nav-axis-line a))
     ~(repeat [a computed.navAxes :key a.name] (nav-axis-end a))))

(defview nav-axis-line "The line from the ball's middle to a positive axis's end." [a]
  `{:x 42 :y 41 :width ~(* (hypot a.x a.y) 30) :height 2 :color ~a.color :rotate ~(degrees (atan2 a.y a.x)) :transformOrigin [0 50]})

(defview nav-axis-end "An axis's end: its letter on a disc, a click looks along it." [a]
  `(node :x ~(- (+ 42 (* a.x 30)) 9)
     :y ~(- (+ 42 (* a.y 30)) 9)
     :width 18
     :height 18
     :radius 5
     :color ~a.color
     :borderWidth ~(if (= a.label "") 1 0)
     :borderColor ~(slice a.color 0 7)
     :role button
     :label ~(t (str "nav." a.view))
     :hover {:color ~(slice a.color 0 7)}
     :onClick ~(handler viewAxis a.view)
     (label :text ~a.label :textColor "#151515" :font small :align center :fill true)))

(defview nav-button "A button under the gizmo: dragged, the magnifier zooms and the hand pans; clicked, the others switch."
  [b]
  `(node :id ~(str "nav-" b.id)
     :width 28
     :height 28
     :radius 8
     :color transparent
     :hover {:color @navHover}
     :role button
     :label ~b.title
     :tooltip ~b.title
     :focusable true
     :onPress {:set state.dragLast :to [0 0]}
     :onDrag ~(handler dragNav b.id)
     :onClick ~(handler navClick b.id)
     (label :font icons :icon ~b.icon :text "" :textColor @text :align center :fill true)))
