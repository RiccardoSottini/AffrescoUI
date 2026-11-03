;;; viewport.lisp --- TODO

(defcomputed nav
  {:cy (cos state.view.camera.yaw)
     :sy (sin state.view.camera.yaw)
     :cp (cos state.view.camera.pitch)
     :sp (sin state.view.camera.pitch)})

(defcomputed navAxes
  (sort-by (fn [it] "d")
             [{:name "x"
               :label "X"
               :x computed.nav.cy
               :y (* computed.nav.sp computed.nav.sy)
               :d (* computed.nav.cp computed.nav.sy)
               :color "#e8604c"
               :view "right"}
              {:name "-x"
               :label ""
               :x (- computed.nav.cy)
               :y (* (- computed.nav.sp) computed.nav.sy)
               :d (* (- computed.nav.cp) computed.nav.sy)
               :color "#e8604c40"
               :view "left"}
              {:name "y"
               :label "Y"
               :x computed.nav.sy
               :y (* (- computed.nav.sp) computed.nav.cy)
               :d (* (- computed.nav.cp) computed.nav.cy)
               :color "#4cbf7a"
               :view "back"}
              {:name "-y"
               :label ""
               :x (- computed.nav.sy)
               :y (* computed.nav.sp computed.nav.cy)
               :d (* computed.nav.cp computed.nav.cy)
               :color "#4cbf7a40"
               :view "front"}
              {:name "z" :label "Z" :x 0 :y (- computed.nav.cp) :d computed.nav.sp :color "#5b74f0" :view "top"}
              {:name "-z" :label "" :x 0 :y computed.nav.cp :d (- computed.nav.sp) :color "#5b74f040" :view "bottom"}]))

(defcomputed viewName
  (if state.inCamera
        "Camera Perspective"
        (+ (cond (< (abs (- (abs state.view.camera.pitch) (/ (pi) 2))) 0.001) (if (> state.view.camera.pitch 0) "Top" "Bottom")
                 (> (abs state.view.camera.pitch) 0.001) "User"
                 (< (abs state.view.camera.yaw) 0.001) "Front"
                 (< (abs (- state.view.camera.yaw (/ (pi) 2))) 0.001) "Right"
                 (< (abs (+ state.view.camera.yaw (/ (pi) 2))) 0.001) "Left"
                 (< (abs (- (abs state.view.camera.yaw) (pi))) 0.001) "Back"
                 :else "User")
           (if state.view.camera.orthographic " Orthographic" " Perspective"))))

(defcomputed tools
  [[{:id "select" :icon "mouse-pointer-2" :title "Select Box" :key "W"}]
     [{:id "cursor" :icon "crosshair" :title "Cursor" :key ""}]
     [{:id "move" :icon "move" :title "Move" :key "G"}
      {:id "rotate" :icon "rotate-cw" :title "Rotate" :key "R"}
      {:id "scale" :icon "scale-3-d" :title "Scale" :key "S"}
      {:id "transform" :icon "move-3-d" :title "Transform" :key "" :disabled true}]
     [{:id "annotate" :icon "pencil" :title "Annotate" :key "" :disabled true}
      {:id "measure" :icon "ruler" :title "Measure" :key ""}]
     [{:id "add" :icon "box" :title "Add Cube" :key ""}]])

(defcomputed toolName (:title (find (fn [it] (= it.id state.tool)) (flat computed.tools))))

(defcomputed lens (/ 18 (tan (/ state.view.camera.fieldOfView 2))))

(defcomputed measureShown
  (and (= state.tool "measure") (> (count state.measure.screen) 0) (= state.measure.camera state.view.camera)))

(defcomputed measured
  (if (= (count state.measure.points) 2)
        (sqrt (+ (pow (- state.measure.points.1.0 state.measure.points.0.0) 2)
                 (pow (- state.measure.points.1.1 state.measure.points.0.1) 2)
                 (pow (- state.measure.points.1.2 state.measure.points.0.2) 2)))
        nil))

(defcomponent viewportArea "The viewportArea component."
  {}
  `(node#viewport-area :color @view :radius 8
     ~(viewport-view)
     (node :use viewportHeader)
     (node :when ~(not= state.toolbar false) :use viewportToolbar)
     (node :when ~computed.measureShown :use measureLine)
     (label#view-name :when ~(and state.overlays state.overlay.text)
       :text ~computed.viewName
       :textColor @text
       :anchors {:left 54 :top 42}
       :fitText true)
     (label#view-collection :when ~(and state.overlays state.overlay.text)
       :text ~(str "("
                   state.frame
                   ") "
                   (if computed.activeNode
                       (str (or-else (:name (find (fn [it]
                                                    (= it.id (or-else computed.activeNode.collection "collection")))
                                                  computed.collections))
                                     "Collection")
                            " | "
                            computed.activeNode.name)
                       computed.activeCollectionData.name))
       :textColor @text
       :anchors {:left 54 :top 58}
       :fitText true)
     (label#view-stats :when ~(and state.overlays state.overlay.stats)
       :text ~(str "Objects "
                   (count state.view.selection)
                   " / "
                   (count state.scene.nodes)
                   "   Lights "
                   (count (filter (fn [it] (= it.kind "light")) state.scene.nodes))
                   "   Cameras "
                   (count (filter (fn [it] (= it.kind "camera")) state.scene.nodes)))
       :textColor @text
       :anchors {:left 54 :top ~(if state.overlay.text 78 42)}
       :fitText true)
     (label#measure-hint :when ~(= state.tool "measure")
       :live polite
       :text ~(cond (some? computed.measured) (str "Distance " (fixed computed.measured 3) " m")
                    (= (count state.measure.points) 1) "Click the second point"
                    :else "Click two points to measure")
       :textColor @text
       :anchors {:left 52 :bottom 40}
       :fitText true)
     (node :when ~(and state.gizmos (not= state.navGizmo false)) :use navigator)
     (node :when ~state.sidebar :use viewportSidebar)
     (node#sidebar-open :when ~(not state.sidebar)
       :use hButton
       :anchors {:right 0 :top 44}
       :with {:icon chevron-left :width 14 :back transparent :tooltip "Sidebar (N)" :run toggleSidebar :textColor @dim})
     (label#modal-hint :when ~(some? state.modal)
       :live polite
       :text ~state.modalText
       :textColor @text
       :anchors {:left 52 :bottom 12}
       :fitText true)
     (node :when ~(and (nil? state.modal) (some? state.lastOp)) :use redoPanel)))

(defcomponent redoPanel "The redoPanel component."
  {}
  `(node#redo-panel :anchors {:left 48 :bottom 8}
     :width 230
     :height ~(if (and state.redoOpen (some? state.lastOp.values)) 94 24)
     :color @panel
     :radius 8
     :role region
     :label "Adjust Last Operation"
     ~(redo-header)
     (node :when ~(and state.redoOpen (some? state.lastOp.values))
       :anchors {:left 4 :right 0 :top 26}
       :height 62
       :color transparent
       (node :use vectorField
         :with {:title ~(or-else state.lastOp.label
                                 (cond (= state.lastOp.kind "scale") "Scale"
                                       (= state.lastOp.kind "rotate") "Rotation"
                                       :else "Move"))
                :values ~state.lastOp.values
                :key redo
                :unit ~(cond (= state.lastOp.kind "scale") "" (= state.lastOp.kind "rotate") "°" :else " m")
                :step ~(cond (= state.lastOp.kind "scale") 0.005 (= state.lastOp.kind "rotate") 0.5 :else 0.01)
                :digits ~(if (= state.lastOp.kind "rotate") 1 3)
                :exact ~(= state.lastOp.kind "scale")}))))

(defcomponent viewportHeader "The viewportHeader component."
  {}
  `(row#viewport-header :anchors {:left 6 :right 6 :top 6} :height 28 :color transparent :alignItems center :gap 0
     ~(header-modes)
     {:grow 1 :height 1 :color transparent}
     ~(header-transform)
     {:grow 1 :height 1 :color transparent}
     ~(header-display)))

(defcomponent viewportToolbar "The viewportToolbar component."
  {}
  `(column#toolbar :anchors {:left 8 :top 42}
     :width 34
     :height ~(- (sum computed.tools (fn [it] (+ (* (count it) 30) 9))) 3)
     :gap 0
     :color @toolGroup
     :radius 8
     :role toolbar
     :label "Tools"
     :arrows vertical
     ~(repeat [group computed.tools :index g]
       `(column :width 34
          :height ~(+ (* (count group) 30) (if (< g (- (count computed.tools) 1)) 9 6))
          :color transparent
          :gap 0
          {:height ~(if (= g 0) 3 0) :width 1 :color transparent}
          ~(repeat [t group]
            (tool-button t))
          (node :when ~(< g (- (count computed.tools) 1)) :height 9 :width 34 :color transparent
            {:x 9 :y 4 :width 16 :height 1 :color @faint})))))

(defcomponent measureLine "The measureLine component."
  {}
  `(node#measure :x 0
     :y 0
     :width 0
     :height 0
     :overflow visible
     :color transparent
     :role img
     :label ~(if (some? computed.measured) (str "Measured " (fixed computed.measured 3) " m") "Measuring")
     {:when ~(= (count state.measure.screen) 2)
      :x ~state.measure.screen.0.0
      :y ~(- state.measure.screen.0.1 1)
      :height 2
      :color @active
      :width ~(hypot (- state.measure.screen.1.0 state.measure.screen.0.0)
                     (- state.measure.screen.1.1 state.measure.screen.0.1))
      :rotate ~(degrees (atan2 (- state.measure.screen.1.1 state.measure.screen.0.1)
                               (- state.measure.screen.1.0 state.measure.screen.0.0)))
      :transformOrigin [0 50]}
     ~(repeat [p state.measure.screen]
       `{:x ~(- p.0 4) :y ~(- p.1 4) :width 8 :height 8 :radius 4 :color @active})
     (label#measure-length :when ~(some? computed.measured)
       :text ~(str (fixed computed.measured 3) " m")
       :textColor @text
       :color @toolGroup
       :radius 4
       :x ~(+ (/ (+ state.measure.screen.0.0 state.measure.screen.1.0) 2) 6)
       :y ~(- (/ (+ state.measure.screen.0.1 state.measure.screen.1.1) 2) 22)
       :fitText true
       :padding 4)))

(defcomponent navigator "The navigator component."
  {}
  `(node#navigator :dir ltr :anchors {:right ~(if state.sidebar 254 14) :top 44} :width 84 :height 212 :color transparent
     ~(nav-ball)
     (column#nav-buttons :anchors {:top 94} :x 28 :width 28 :height 112 :gap 0 :color @navButton :radius 8
       ~(repeat [b [{:id "zoom" :icon "zoom-in" :title "Zoom in/out in the view"} {:id "pan" :icon "hand" :title "Move the view"} {:id "camera" :icon "video" :title "Toggle the camera view (Numpad 0)"} {:id "ortho" :icon "grid-3-x-3" :title "Switch the current view from perspective/orthographic projection (Numpad 5)"}]]
         (nav-button b)))))

(defview viewport-view "#viewport." []
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
     :label "3D Viewport"
     :role img
     :onSelect pick
     :onTransform transformed))

(defview redo-header "#redo-header." []
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
     (label :text ~state.lastOp.title
       :textColor @text
       :align {:horizontal start :vertical center}
       :anchors {:left 22 :right 4 :top 0 :bottom 0})))

(defview header-modes "#header-modes." []
  `(row#header-modes :height 28
     :fitContent width
     :color @toolGroup
     :radius 8
     :alignItems center
     :gap 2
     :role toolbar
     :label "Mode and menus"
     (node :use gap)
     (node#editor-type :use hButton
       :with {:icon axis-3-d
              :chevron true
              :width 38
              :tooltip "Editor Type"
              :open state.open.editor0
              :items ~(map (fn [it]
                             (merge it
                                    {:checked (= it.id "viewport")
                                     :run "swapEditor"
                                     :with {:from "viewport" :to it.id}}))
                           computed.editorTypes)})
     (node :use gap :with {:width 6})
     (node#mode :use hButton
       :with {:icon ~(if (= state.mode "edit") "pen-tool" "box")
              :text ~(if (= state.mode "edit") "Edit Mode" "Object Mode")
              :chevron true
              :width 116
              :tooltip "Interaction Mode"
              :open state.open.mode
              :items ~computed.modeItems})
     (node :use gap :with {:width 6})
     ~(repeat [m [["view" "View"] ["select" "Select"] ["add" "Add"] ["object" "Object"]]]
       `(node :id ~(str "menu-" m.0)
          :use hButton
          :with {:text ~m.1
                 :width ~(+ (* (count m.1) 7) 14)
                 :back transparent
                 :open ~(str "state.open." m.0)
                 :items ~(get menus m.0)
                 :menuWidth 230}))
     (node :use gap)))

(defview header-transform "#header-transform." []
  `(row#header-transform :height 28
     :fitContent width
     :color @toolGroup
     :radius 8
     :alignItems center
     :gap 2
     :role toolbar
     :label "Transform settings"
     (node :use gap)
     (node#orientation :use hButton
       :with {:icon axis-3-d
              :text ~(+ (upper-case (slice state.orientation 0 1)) (slice state.orientation 1))
              :chevron true
              :width 86
              :tooltip "Transform Orientation"
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
              :tooltip "Transform Pivot Point"
              :open state.open.pivot
              :items ~computed.pivotItems
              :menuWidth 190})
     (node :use gap)
     (node#snap :use hButton
       :with {:icon magnet
              :on ~state.snap
              :width 24
              :tooltip "Snap (Shift Tab)"
              :run toggleSnap
              :radius {:topLeft 4 :bottomLeft 4}})
     (node#snap-target :use hButton
       :with {:icon grid-3-x-3
              :chevron true
              :width 34
              :tooltip "Snap To: Increment"
              :open state.open.snapTarget
              :items ~computed.snapItems
              :menuWidth 190
              :radius {:topRight 4 :bottomRight 4}})
     (node :use gap)
     (node#proportional :use hButton
       :with {:icon circle-dot-dashed
              :width 24
              :tooltip "Proportional Editing isn't available yet: it moves a mesh's vertices"
              :disabled true})
     (node :use gap)))

(defview header-display "#header-display." []
  `(row#header-display :height 28
     :fitContent width
     :color @toolGroup
     :radius 8
     :alignItems center
     :gap 2
     :role toolbar
     :label "Display"
     (node :use gap)
     (node#visibility :use hButton
       :with {:icon eye
              :chevron true
              :width 38
              :tooltip "Selectability & Visibility"
              :open state.open.visibility
              :items ~computed.visibilityItems
              :menuWidth 180})
     (node :use gap)
     (node#gizmos :use hButton
       :with {:icon move-3-d
              :on ~state.gizmos
              :width 24
              :tooltip "Show Gizmo"
              :run toggleOption
              :with {:key gizmos}
              :radius {:topLeft 4 :bottomLeft 4}})
     (node#gizmos-menu :use hButton
       :with {:icon ""
              :chevron true
              :width 16
              :tooltip "Gizmos"
              :open state.open.gizmos
              :items ~computed.gizmoItems
              :menuWidth 180
              :radius {:topRight 4 :bottomRight 4}})
     (node :use gap)
     (node#overlays :use hButton
       :with {:icon blend
              :on ~state.overlays
              :width 24
              :tooltip "Show Overlays (Shift Alt Z)"
              :run toggleOverlays
              :radius {:topLeft 4 :bottomLeft 4}})
     (node#overlays-menu :use hButton
       :with {:icon ""
              :chevron true
              :width 16
              :tooltip "Overlays"
              :open state.open.overlays
              :items ~computed.overlayItems
              :menuWidth 190
              :radius {:topRight 4 :bottomRight 4}})
     (node :use gap)
     (node#xray :use hButton
       :with {:icon square-dashed-bottom :on ~state.xray :width 24 :tooltip "Toggle X-Ray (Alt Z)" :run toggleXray})
     (node :use gap)
     ~(repeat [s computed.shadingModes :index i]
       `(node :id ~(str "shading-" s.id)
          :use hButton
          :with {:icon ~s.icon
                 :on ~(= state.shading s.id)
                 :width 24
                 :tooltip ~(str "Viewport Shading: " s.title)
                 :run setOption
                 :with {:key shading :value ~s.id}
                 :radius ~(if (= i 0) {:topLeft 4 :bottomLeft 4} 0)}))
     (node#shading-menu :use hButton
       :with {:icon ""
              :chevron true
              :width 16
              :tooltip "Shading"
              :open state.open.shading
              :items ~computed.shadingItems
              :menuWidth 230
              :radius {:topRight 4 :bottomRight 4}})
     (node :use gap)))

(defview tool-button "#tool-." [t]
  `(node :id ~(str "tool-" t.id)
     :use hButton
     :x 2
     :with {:icon ~t.icon
            :on ~(= state.tool t.id)
            :width 30
            :radius 6
            :tooltip ~(str t.title
                           (cond (= t.disabled true) " isn't available yet"
                                 (not= t.key "") (str " (" t.key ")")
                                 :else ""))
            :back transparent
            :disabled ~(= t.disabled true)
            :run setTool
            :with {:tool ~t.id}}
     :height 30))

(defview nav-ball "#nav-ball." []
  `(node#nav-ball :width 84
     :height 84
     :radius 14
     :color transparent
     :hover {:color @navBall}
     :role slider
     :label "Navigation gizmo: drag to orbit"
     :onPress {:set state.dragLast :to [0 0]}
     :onDrag [{:camera "#viewport" :orbit [~(* (- state.dragLast.0 event.dx) 0.5) ~(* (- state.dragLast.1 event.dy)
                                                                                      0.5)]}
              {:set state.dragLast :to ~[event.dx event.dy]}]
     ~(repeat [a (filter (fn [it] (not= it.label "")) computed.navAxes) :key a.name]
       `{:x 42
         :y 41
         :width ~(* (hypot a.x a.y) 30)
         :height 2
         :color ~a.color
         :rotate ~(degrees (atan2 a.y a.x))
         :transformOrigin [0 50]})
     ~(repeat [a computed.navAxes :key a.name]
       `(node :x ~(- (+ 42 (* a.x 30)) 9)
          :y ~(- (+ 42 (* a.y 30)) 9)
          :width 18
          :height 18
          :radius 5
          :color ~a.color
          :borderWidth ~(if (= a.label "") 1 0)
          :borderColor ~(slice a.color 0 7)
          :role button
          :label ~(str "View " (+ (upper-case (slice a.view 0 1)) (slice a.view 1)))
          :hover {:color ~(slice a.color 0 7)}
          :onClick {:run viewAxis :with {:view ~a.view}}
          (label :text ~a.label :textColor "#151515" :font small :align center :fill true)))))

(defview nav-button "#nav-." [b]
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
     :onDrag [{:run navDrag
               :with {:id ~b.id :dx ~(- event.dx state.dragLast.0) :dy ~(- event.dy state.dragLast.1)}}
              {:set state.dragLast :to ~[event.dx event.dy]}]
     :onClick {:run navClick :with {:id ~b.id}}
     (label :font icons :icon ~b.icon :text "" :textColor @text :align center :fill true)))

