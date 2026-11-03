;;; view.lisp --- the 3D view: the bodies, their orbits and the stars, from the camera
;;;
;;; The scene node is bound to computed values; the labels, the marks and the readouts are drawn over it. Each body's
;;; thumbnail is a node drawn into the image body:<id>, for the features that show the bodies.

;;; The camera

(defcomputed pane "The 3D view's size." {:w (max (- window.width 461) 200) :h (max (- window.height 324) 160)})

(defcomputed view "The 3D view's camera: its size, focal length, the point it looks at and its turn and tilt."
  {:w computed.pane.w
   :h computed.pane.h
   :f (* state.zoom (min computed.pane.w computed.pane.h))
   :distance state.camera.distance
   :cy (cos state.camera.yaw)
   :sy (sin state.camera.yaw)
   :cp (cos state.camera.pitch)
   :sp (sin state.camera.pitch)})

(defcomputed target "The point the camera looks at: the body it follows, else the camera's own."
  (if (and (not= state.follow "") computed.followed)
    {:x computed.followed.x :z computed.followed.z}
    {:x (+ state.camera.tx (* (if computed.flyPosition (- computed.flyPosition.x state.camera.tx) 0) state.fly))
     :z (+ state.camera.tz (* (if computed.flyPosition (- computed.flyPosition.z state.camera.tz) 0) state.fly))}))

(defcomputed sceneView "The 3D view's camera, as the scene takes it."
  {:camera {:target [computed.target.x 0 computed.target.z]
            :yaw state.camera.yaw
            :pitch state.camera.pitch
            :distance computed.view.distance
            :fieldOfView (* 2 (atan (/ computed.view.h (* 2 computed.view.f))))}
   :selection (if (not= state.selected "") [state.selected] [])})

(defcomputed fov "The 3D view's field of view, in degrees, across and up."
  {:deg (degrees (* 2 (atan (/ (min computed.view.w computed.view.h) (* 2 computed.view.f)))))
   :times (/ state.zoom 1.1)})

(defcomputed fovText "The field of view, as the view's readout says it."
  (t "view.fov"
     {:degrees (text (floor computed.fov.deg))
      :minutes (pad (floor (* (- computed.fov.deg (floor computed.fov.deg)) 60)) 2 "0")
      :times (fixed computed.fov.times 2)}))

(defcomputed magText "The faintest stars shown, as the view's readout says it."
  (t "view.magnitude"
     {:mag (fixed state.limitMag 1) :n (text (count (filter (fn [it] (<= it.mag state.limitMag)) computed.stars)))}))

;;; The bodies and the stars on the view

(defcomputed stars "The background stars, each with its direction."
  (map-indexed (fn [index it]
                 {:u (- (* (mod (abs (* (sin (* index 12.9898)) 43758.5453)) 1) 2) 1)
                  :t (* (mod (abs (* (sin (* index 78.233)) 12543.917)) 1) 2 (pi))
                  :mag (+ 0.5 (* 6 (sqrt (mod (abs (* (sin (* index 39.425)) 24634.633)) 1))))})
               (range 400)))

(defcomputed starVectors "The background stars' directions as unit vectors."
  (map (fn [it]
         {:x (* (sqrt (- 1 (* it.u it.u))) (cos it.t))
          :y it.u
          :z (* (sqrt (- 1 (* it.u it.u))) (sin it.t))
          :mag it.mag})
       computed.stars))

(defcomputed turned "Each body in the camera's frame."
  (map (fn [it]
         (merge it
                {:x1 (- (* (- it.x computed.target.x) computed.view.cy)
                        (* (- it.z computed.target.z) computed.view.sy))
                 :z1 (+ (* (- it.x computed.target.x) computed.view.sy)
                        (* (- it.z computed.target.z) computed.view.cy))}))
       computed.placed))

(defcomputed projected "Each body on the view, farthest first, with its size on screen."
  (sort-by (fn [it] it.depth)
           >
           (map (fn [it]
                  (merge it
                         {:depth (- computed.view.distance (* it.z1 computed.view.cp))
                          :px (+ (/ computed.view.w 2)
                                 (/ (* it.x1 computed.view.f) (- computed.view.distance (* it.z1 computed.view.cp))))
                          :py (+ (/ computed.view.h 2)
                                 (/ (* it.z1 computed.view.sp computed.view.f)
                                    (- computed.view.distance (* it.z1 computed.view.cp))))
                          :r (min (max 3
                                       (/ (* it.size computed.view.f)
                                          (- computed.view.distance (* it.z1 computed.view.cp))))
                                  (* computed.view.h 0.4))}))
                computed.turned)))

(defcomputed shaded "Each body's lit side, as the Sun lights it."
  (map (fn [it]
         (merge it
                {:k (if (= it.parent "")
                      1
                      (/ (+ 1
                            (/ (+ (* (- computed.sun.x1 it.x1) (- it.x1))
                                  (* (- computed.sun.z1 it.z1)
                                     (- (* computed.view.distance computed.view.cp) it.z1)))
                               (max (* (hypot (- computed.sun.x1 it.x1) (- computed.sun.z1 it.z1))
                                       (sqrt
                                         (+ (- (+ (* it.x1 it.x1) (* computed.view.distance computed.view.distance))
                                               (* 2 computed.view.distance it.z1 computed.view.cp))
                                            (* it.z1 it.z1))))
                                    0.000001)))
                         2))
                 :ux (/ (- computed.sun.x1 it.x1)
                        (max (hypot (- computed.sun.x1 it.x1) (* (- computed.sun.z1 it.z1) computed.view.sp))
                             0.000001))
                 :uy (/ (* (- computed.sun.z1 it.z1) computed.view.sp)
                        (max (hypot (- computed.sun.x1 it.x1) (* (- computed.sun.z1 it.z1) computed.view.sp))
                             0.000001))}))
       computed.projected))

(defcomputed sight "The selected body, on the view." (find (fn [it] (= it.id state.selected)) computed.projected))

(defcomputed sun "The Sun, in the camera's frame." (find (fn [it] (= it.parent "")) computed.turned))

(defcomputed range "How far the selected body is from the camera."
  (if computed.sight
    (sqrt (+ (- (+ (* computed.sight.x1 computed.sight.x1) (* computed.view.distance computed.view.distance))
                (* 2 computed.view.distance computed.sight.z1 computed.view.cp))
             (* computed.sight.z1 computed.sight.z1)))
    0))

(defcomputed info "What the view's card says of the selected body: name, distance, how much is lit."
  (if computed.sight
    {:name computed.selectedBody.name
     :distance (/ computed.range 4.3)
     :lit (if (= computed.sight.parent "")
            1
            (/ (+ 1
                  (/ (+ (* (- computed.sun.x1 computed.sight.x1) (- computed.sight.x1))
                        (* (- computed.sun.z1 computed.sight.z1)
                           (- (* computed.view.distance computed.view.cp) computed.sight.z1)))
                     (max (* (hypot (- computed.sun.x1 computed.sight.x1) (- computed.sun.z1 computed.sight.z1))
                             computed.range)
                          0.000001)))
               2))}
    nil))

(defcomputed apparent "How wide the selected body looks, in degrees."
  (if (and computed.sight (> computed.range 0)) (degrees (* 2 (atan (/ computed.sight.size computed.range)))) 0))

(defcomputed apparentText "How wide the selected body looks, in degrees, minutes or seconds."
  (cond (>= computed.apparent 1) (str (fixed computed.apparent 2) "°")
        (>= (* computed.apparent 60) 1) (str (fixed (* computed.apparent 60) 1) "′")
        :else (str (fixed (* computed.apparent 3600) 1) "″")))

(defcomputed marks "The marked bodies in front of the camera."
  (filter (fn [it] (and (>= (indexOf state.markers it.id) 0) (> it.depth 0.3))) computed.projected))

(defcomputed marked "The marks' corners, drawn around the marked bodies."
  (map-indexed (fn [index it]
                 {:id (str (get-in computed.marks [(floor (/ index 4)) :id]) "-" (mod index 4))
                  :x (cond (< (mod index 4) 2) (- (get-in computed.marks [(floor (/ index 4)) :px]) 1)
                           (= (mod index 4) 2) (- (- (get-in computed.marks [(floor (/ index 4)) :px])
                                 (get-in computed.marks [(floor (/ index 4)) :r]))
                              12)
                           :else (+ (get-in computed.marks [(floor (/ index 4)) :px])
                              (get-in computed.marks [(floor (/ index 4)) :r])
                              5))
                  :y (cond (= (mod index 4) 0) (- (- (get-in computed.marks [(floor (/ index 4)) :py])
                                 (get-in computed.marks [(floor (/ index 4)) :r]))
                              12)
                           (= (mod index 4) 1) (+ (get-in computed.marks [(floor (/ index 4)) :py])
                              (get-in computed.marks [(floor (/ index 4)) :r])
                              5)
                           :else (- (get-in computed.marks [(floor (/ index 4)) :py]) 1))
                  :w (if (< (mod index 4) 2) 2 7)
                  :h (if (< (mod index 4) 2) 7 2)})
               (range (* (count computed.marks) 4))))

;;; The scene

(defcomputed orbitRings "Each orbit: its centre and its radius."
  (map (fn [it] {:id it.id :cx it.cx :cz it.cz :radius it.radius})
       (filter (fn [it] (not= it.parent "")) computed.placed)))

(defcomputed orbitSegments "The orbits as the scene's lines, while they are shown."
  (if state.showOrbits
    (map-indexed (fn [index it]
                   {:id (str "orbit-" (get-in computed.orbitRings [(floor (/ index 64)) :id]) "-" (mod index 64))
                    :translation [(+ (get-in computed.orbitRings [(floor (/ index 64)) :cx])
                                     (* (get-in computed.orbitRings [(floor (/ index 64)) :radius])
                                        (cos (/ (* (+ (mod index 64) 0.5) (pi)) 32))))
                                  0
                                  (+ (get-in computed.orbitRings [(floor (/ index 64)) :cz])
                                     (* (get-in computed.orbitRings [(floor (/ index 64)) :radius])
                                        (sin (/ (* (+ (mod index 64) 0.5) (pi)) 32))))]
                    :rotationDegrees [0 (- 90 (* (+ (mod index 64) 0.5) 5.625)) 0]
                    :scale [(/ (* (get-in computed.orbitRings [(floor (/ index 64)) :radius]) (pi)) 62)
                            (/ (* computed.view.distance 0.6) computed.view.f)
                            (/ (* computed.view.distance 0.6) computed.view.f)]
                    :mesh "cube"
                    :material (if (= (get-in computed.orbitRings [(floor (/ index 64)) :id]) state.selected)
                                "orbit-on"
                                "orbit")})
                 (range (* (count computed.orbitRings) 64)))
    []))

(defcomputed starNodes "The background stars as the scene's points, while they are shown."
  (if state.showStars
    (map-indexed (fn [index it]
                   {:id (str "star-" index)
                    :translation [(+ computed.target.x (* it.x 300))
                                  (* it.y 300)
                                  (+ computed.target.z (* it.z 300))]
                    :scale [(/ (* 310 (- 1.3 (* it.mag 0.13))) computed.view.f)
                            (/ (* 310 (- 1.3 (* it.mag 0.13))) computed.view.f)
                            (/ (* 310 (- 1.3 (* it.mag 0.13))) computed.view.f)]
                    :mesh "cube"
                    :material "star"})
                 (filter (fn [it] (<= it.mag state.limitMag)) computed.starVectors))
    []))

(defcomputed bodyNodes "The bodies as the scene's spheres."
  (map (fn [it]
         {:id (str "" it.id)
          :name it.name
          :translation [it.x 0 it.z]
          :scale [(max it.size (/ (* 3 it.depth) computed.view.f))
                  (max it.size (/ (* 3 it.depth) computed.view.f))
                  (max it.size (/ (* 3 it.depth) computed.view.f))]
          :mesh "sphere"
          :material (str "m-" it.id)
          :visible (> it.depth 0.3)})
       computed.projected))

(defcomputed bodyMaterials "Each body's colour and glow, as the scene's materials."
  (map (fn [it]
         (cond (= it.parent "") {:id (str "m-" it.id)
                :baseColor [0 0 0 1]
                :emissive (if state.showGlow [1.5 0.85 0.22] [1.1 0.65 0.18])
                :roughness 1}
               state.showShading {:id (str "m-" it.id) :baseColor it.color :roughness 0.9}
               :else {:id (str "m-" it.id)
                :baseColor [0 0 0 1]
                :emissive [(* it.color.0 0.8) (* it.color.1 0.8) (* it.color.2 0.8)]
                :roughness 1}))
       state.bodies))

(defcomputed sceneGraph "The 3D scene: the bodies, the orbits, the stars and the Sun's light."
  {:nodes (concat computed.bodyNodes
                  [{:id "sunlight"
                    :translation (if computed.sun [computed.sun.x 0 computed.sun.z] [0 0 0])
                    :light {:type "point" :color [1 0.95 0.85] :intensity 60}}]
                  (if (and state.showGlow computed.sun)
                    [{:id "halo"
                      :translation [computed.sun.x 0 computed.sun.z]
                      :scale [1.7 1.7 1.7]
                      :mesh "sphere"
                      :material "halo"}]
                    [])
                  computed.orbitSegments
                  computed.starNodes)
   :materials (concat computed.bodyMaterials
                      [{:id "orbit" :baseColor [0 0 0 1] :emissive [0.1 0.09 0.07] :roughness 1}
                       {:id "orbit-on" :baseColor [0 0 0 1] :emissive [0.18 0.8 0.7] :roughness 1}
                       {:id "star" :baseColor [0 0 0 1] :emissive [0.75 0.8 0.9] :roughness 1}
                       {:id "halo"
                        :baseColor [1 0.6 0.1 0.12]
                        :emissive [0.5 0.3 0.05]
                        :alphaMode "blend"
                        :roughness 1}])})

(defcomputed renderOptions "What the 3D view can draw, each on or off."
  [{:id "orbits" :title (t "view.orbits") :on state.showOrbits :run "toggleOrbits"}
   {:id "labels" :title (t "view.labels") :on state.showLabels :run "toggleLabels"}
   {:id "stars" :title (t "view.stars") :on state.showStars :run "toggleStars"}
   {:id "shading" :title (t "view.litSide") :on state.showShading :run "toggleShading"}
   {:id "glow" :title (t "view.sunGlow") :on state.showGlow :run "toggleGlow"}])

;;; The view pane

(defview view-pane
  "The view is a picture of the sky: its markers, labels and readouts keep left to right in every locale."
  []
  `(panel#view-pane :dir ltr
     :note "The view is a picture of the sky: its markers, labels and readouts keep left to right in every locale."
     :grow 1
     :alignSelf stretch
     :color @sky
     :onPress viewPress
     :onDrag viewDrag
     :onRelease viewRelease
     :onScroll viewScroll
     ~(scene-view)
     ~(repeat [t computed.marked :key t.id]
        `(panel :id ~(str "view-marker-" t.id) :x ~t.x :y ~t.y :width ~t.w :height ~t.h :color "#f0abfc"))
     ~(repeat [b computed.projected :key b.id]
        `(label :id ~(str "view-label-" b.id)
           :x ~(if (and (not= b.parent "") (not= b.parent "sun")) (- (- b.px b.r) 4) (+ b.px b.r 6))
           :y ~(if (and (not= b.parent "") (not= b.parent "sun")) (+ b.py b.r 2) (- b.py 9))
           :height 18
           :fitText true
           :font small
           :text ~b.name
           :textColor ~(if (= b.id state.selected) "@accent" "#b9c7c2")
           :visible ~(and state.showLabels (> b.depth 0.3))))
     ~(repeat [b (filter (fn [it] (= (or-else it.look "") "")) state.bodies) :key b.id]
        `(panel :id ~(str "thumb-" b.id)
           :snapshot ~{:image (str "body:" b.id) :scale 1}
           :width 96
           :height 64
           :color "#07090e"
           (panel :id ~(str "thumb-disc-" b.id)
             :x 25
             :y 9
             :width 46
             :height 46
             :radius 999
             :color ~b.hex
             :shadow {:blur 14 :color ~(if (= b.parent "") "#ffc94d99" "#00000000")})))
     ~(info-card)
     ~(clock-card)
     ~(browser)
     ~(view-hint)
     ~(optics)))

(defview scene-view "The 3D scene, bound to the computed camera; a left click selects a body." []
  `(scene#view-scene :fill true
     :bind state.sceneView
     :scene ~computed.sceneGraph
     :grid false
     :axes false
     :background "#070b0c"
     :outlineColor @accent
     :controls {:orbit none :pan none :zoom false :select left :selects false}
     :onSelect ~(handler pickInScene event.button event.id)))

(defview info-card "The card of the selected body, at the bottom left." []
  `(panel#view-info :anchors {:left 12 :bottom 12}
     :width 252
     :height 116
     :color @hud
     :radius 10
     :borderWidth 1
     :borderColor @hudBorder
     :visible ~(some? computed.info)
     (panel#info-stripe :x 0 :y 12 :width 3 :height 92 :radius 2 :color @accent)
     ~(info-lines)))

(defview info-lines "The selected body's name, distance, radius, apparent size and phase." []
  `(column#info-lines :x 14 :y 8 :width 230 :height 110 :gap 2
     (label#info-name :height 22 :fitText true :font title :textColor @onSky :text ~computed.info.name)
     (label#info-distance :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :text ~(t "info.distance" {:au (fixed computed.info.distance 2)}))
     (label#info-radius :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :text ~(t "info.radius" {:km (text computed.selectedBody.km)}))
     (label#info-apparent :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :text ~(t "info.apparent" {:size computed.apparentText}))
     (label#info-phase :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :visible ~(not= computed.selectedBody.parent "")
       :text ~(t "info.phase" {:percent (text (round (* computed.info.lit 100)))}))))

(defview clock-card "The day and the rate, at the top of the view." []
  `(column#view-time :anchors {:left ~(max (/ (- (- window.width 461) 300) 2) 12) :top 10}
     :width 300
     :gap 0
     :justify center
     :color @hud
     :radius 22
     :borderWidth 1
     :borderColor @hudBorder
     :height 44
     (label#time-date :height 20
       :width 300
       :align {:horizontal center :vertical center}
       :font small
       :textColor @onSky
       :text ~computed.clock)
     (label#time-rate :height 18
       :width 300
       :align {:horizontal center :vertical center}
       :font small
       :textColor ~(if state.playing "@accent" "@onSkyMuted")
       :text ~computed.rate)))

(defview optics "The field of view and the faintest stars shown, at the bottom right." []
  `(row#view-optics :anchors {:right 12 :bottom 12}
     :fitContent width
     :height 28
     :gap 12
     :alignItems center
     :color @hud
     :radius 14
     :borderWidth 1
     :borderColor @hudBorder
     (panel#optics-start :width 2 :height 2)
     (label#view-fov :fitText true :height 20 :font small :textColor @onSky :text ~computed.fovText)
     (label#view-mag :fitText true :height 20 :font small :textColor @onSkyMuted :text ~computed.magText)
     (panel#optics-end :width 2 :height 2)))

(defview view-hint "How to move about the view, while the browser is closed and the view is wide enough." []
  `(label#view-hint :anchors {:right 14 :bottom 46}
     :fitText true
     :height 20
     :font small
     :textColor @onSkyMuted
     :text ~(t "view.hint")
     :visible ~(and (not state.showBrowser) (>= (- window.width 461) 600))))

(defview render-dialog "The render options: what the 3D view draws." []
  `(node#render :use dialog
     :with {:open state.renderOpen
            :title ~(t "command.openRender")
            :message ~(t "render.message")
            :buttons [{:title ~(t "render.done")}]
            :width 340
            :height 330}
     (column#render-options :gap 6 :anchors {:left 20 :right 20 :top 76 :bottom 64}
       ~(repeat [o computed.renderOptions :key o.id] (render-option o)))))

(defview render-option "One thing the 3D view can draw, and the switch that turns it on or off." [o]
  `(row :id ~(str "render-" o.id) :width "100%" :height 30
     (label :id ~(str "render-" o.id "-title") :text ~o.title :textColor @text :width 200 :height 30)
     (toggle :id ~(str "render-" o.id "-toggle")
       :width 44
       :height 24
       :label ~o.title
       :checked ~o.on
       :activeColor @accent
       :trackColor @border
       :onChange ~(handler toggleRender o.id))))
