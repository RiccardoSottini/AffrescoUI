;;; index.screen.lisp --- Orrery, a small planetarium, all in this file and the sky from the ground in sky.lisp and what it does in orrery.lisp: the 3D view is a scene node bound to computed values, and each body's thumbnail is a node drawn into the image body:<id> (snapshot)
;;;
;;; The bodies strip at the bottom is Sinopia's slide sorter, installed through the bodies.dock point and
;;; orrery-sorter.adapter.lisp.

(include "features.lisp"
         "sky.lisp"
         "orrery.lisp"
         "../../shared/addons/assistant/assistant.lisp")

;;; Definitions

(defstate time 0)

(defstate duration 420)

(defstate playing false)

(defstate speed 0.01)

(defstate theme dark)

(defstate selected earth)

(defstate camera {:tx 0 :tz 0 :yaw 0.4 :pitch 0.75 :distance 15})

(defstate grab nil)

(defstate bodies
  [{:id sun
    :name "Sun"
    :parent ""
    :radius 0
    :period 1
    :phase 0
    :size 0.9
    :hex "#ffc94d"
    :color [1.0 0.5841 0.0742 1]
    :gx 10
    :gy 75
    :km 696000}
   {:id mercury
    :name "Mercury"
    :parent sun
    :radius 2.0
    :period 88
    :phase 0.3
    :size 0.16
    :hex "#b3aca3"
    :color [0.4508 0.4125 0.3663 1]
    :gx 170
    :gy 0
    :km 2440}
   {:id venus
    :name "Venus"
    :parent sun
    :radius 3.0
    :period 224.7
    :phase 0.6
    :size 0.26
    :hex "#e8c27a"
    :color [0.807 0.5395 0.1946 1]
    :gx 170
    :gy 52
    :km 6052}
   {:id earth
    :name "Earth"
    :parent sun
    :radius 4.3
    :period 365.25
    :phase 0
    :size 0.28
    :hex "#3f86e0"
    :color [0.0497 0.2384 0.7454 1]
    :gx 170
    :gy 104
    :km 6371}
   {:id moon
    :name "Moon"
    :parent earth
    :radius 0.65
    :period 27.32
    :phase 0.5
    :nodeDay 36.9
    :eclipseYear 354.4
    :eclipseSeason 18
    :size 0.09
    :hex "#d4d4d4"
    :color [0.6584 0.6584 0.6584 1]
    :gx 330
    :gy 104
    :km 1737}
   {:id mars
    :name "Mars"
    :parent sun
    :radius 5.8
    :period 687
    :phase 0.15
    :size 0.21
    :hex "#d2643c"
    :color [0.6445 0.1274 0.0452 1]
    :gx 170
    :gy 156
    :km 3390}])

(defstate events
  [{:id mercury-1 :time 81.2 :kind conjunction :label "Mercury inferior conjunction"}
   {:id mars-1 :time 116.8 :kind opposition :label "Mars at opposition"}
   {:id mercury-2 :time 197.1 :kind conjunction :label "Mercury inferior conjunction"}
   {:id venus-1 :time 234.6 :kind conjunction :label "Venus inferior conjunction"}
   {:id mercury-3 :time 313.1 :kind conjunction :label "Mercury inferior conjunction"}])

(defstate stars
  [{:id star-0 :x -81 :y -122 :width 4 :height 4 :radius 2.0 :color "#cfd8ff"}
   {:id star-1 :x -197 :y 13 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-2 :x 188 :y -100 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-3 :x -38 :y -91 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-4 :x -203 :y 23 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-5 :x 35 :y -36 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-6 :x 26 :y -128 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-7 :x 19 :y 25 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-8 :x -183 :y 25 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-9 :x -185 :y 74 :width 3 :height 3 :radius 1.5 :color "#cfd8ff"}
   {:id star-10 :x 55 :y -1 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-11 :x 128 :y -12 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-12 :x -92 :y 103 :width 4 :height 4 :radius 2.0 :color "#fff4d6"}
   {:id star-13 :x -192 :y -70 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-14 :x 106 :y -74 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-15 :x 5 :y -117 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-16 :x 199 :y -27 :width 4 :height 4 :radius 2.0 :color "#cfd8ff"}
   {:id star-17 :x 122 :y 26 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-18 :x 90 :y 33 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-19 :x -198 :y -142 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-20 :x 91 :y -152 :width 4 :height 4 :radius 2.0 :color "#ffffff"}
   {:id star-21 :x 68 :y 173 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-22 :x 100 :y 135 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-23 :x 203 :y -51 :width 3 :height 3 :radius 1.5 :color "#cfd8ff"}
   {:id star-24 :x -3 :y -99 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-25 :x 110 :y -36 :width 3 :height 3 :radius 1.5 :color "#cfd8ff"}
   {:id star-26 :x -153 :y -34 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-27 :x 147 :y 127 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-28 :x 224 :y 64 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-29 :x -161 :y -113 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-30 :x -224 :y 116 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-31 :x -100 :y -124 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-32 :x 51 :y -63 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-33 :x -20 :y 130 :width 4 :height 4 :radius 2.0 :color "#ffd9c2"}
   {:id star-34 :x -47 :y -37 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-35 :x -201 :y -151 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-36 :x -155 :y -56 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-37 :x -230 :y -122 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-38 :x 52 :y -150 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-39 :x -162 :y -87 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-40 :x -12 :y -135 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-41 :x -9 :y -66 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-42 :x 115 :y 84 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-43 :x 8 :y -103 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-44 :x -163 :y 15 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-45 :x 220 :y 127 :width 4 :height 4 :radius 2.0 :color "#ffffff"}
   {:id star-46 :x 8 :y 143 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-47 :x 15 :y 98 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}])

(defstate chart {:camera {:x -215 :y -140 :zoom 1} :selection []})

(defstate notes
  [{:id note-1
    :x -205
    :y 120
    :width 120
    :height 22
    :radius 4
    :color @note
    :text "Drag me; add notes above"
    :textSize 10
    :textColor "#1d2026"}])

(defstate draft "")

(defstate nextId 2)

(defstate follow "")

(defstate fly 0)

(defstate showOrbits true)

(defstate showLabels true)

(defstate showStars true)

(defstate showShading true)

(defstate showGlow true)

(defstate renderOpen false)

(defstate viewMenu false)

(defstate zoom 1.1)

(defstate showBrowser false)

(defstate browserTab bodies)

(defstate expandedBodies [sun earth])

(defstate bookmarks
  [{:id overview
    :label "Whole system"
    :camera {:tx 0 :tz 0 :yaw 0.4 :pitch 0.75 :distance 15}
    :follow ""
    :selected earth
    :time 0
    :zoom 1.1}])

(defstate bookmark overview)

(defstate flying "")

(defstate flights 0)

(defstate landed 0)

(defstate nextPlace 1)

(defstate sceneView
  {:camera {:target [0 0 0] :yaw 0.4 :pitch 0.75 :distance 15 :fieldOfView 0.8533} :selection [earth]})

(defstate limitMag 5)

(defstate markers [])

(defstate muted [])

(defstate locked [])

(defstate eclipse "")

(defcomputed ids (map (fn [it] it.id) state.bodies))

(defcomputed local
  (map (fn [it]
           (merge it
                  {:f (if (= it.parent "") 0 (mod (+ it.phase (/ state.time it.period)) 1))
                   :pi (indexOf computed.ids it.parent)}))
         state.bodies))

(defcomputed rel
  (map (fn [it] (merge it {:lx (* it.radius (cos (* 2 (pi) it.f))) :lz (* (- it.radius) (sin (* 2 (pi) it.f)))}))
         computed.local))

(defcomputed up1 (map (fn [it] it.pi) computed.local))

(defcomputed up2 (map (fn [it] (if (>= it 0) (get computed.up1 it) (- 1))) computed.up1))

(defcomputed up3 (map (fn [it] (if (>= it 0) (get computed.up1 it) (- 1))) computed.up2))

(defcomputed depth
  (map-indexed (fn [index it]
                   (cond (< it 0) 0 (< (get computed.up2 index) 0) 1 (< (get computed.up3 index) 0) 2 :else 3))
                 computed.up1))

(defcomputed below
  (map-indexed (fn [index it]
                   (cond (>= (indexOf computed.up3 index) 0) 3
                         (>= (indexOf computed.up2 index) 0) 2
                         (>= (indexOf computed.up1 index) 0) 1
                         :else 0))
                 computed.up1))

(defcomputed positions
  (map-indexed (fn [index it]
                   (merge it
                          {:cx (+ (if (>= it.pi 0) (get-in computed.rel [it.pi :lx]) 0)
                                  (if (>= (get computed.up2 index) 0)
                                      (get-in computed.rel [(get computed.up2 index) :lx])
                                      0)
                                  (if (>= (get computed.up3 index) 0)
                                      (get-in computed.rel [(get computed.up3 index) :lx])
                                      0))
                           :cz (+ (if (>= it.pi 0) (get-in computed.rel [it.pi :lz]) 0)
                                  (if (>= (get computed.up2 index) 0)
                                      (get-in computed.rel [(get computed.up2 index) :lz])
                                      0)
                                  (if (>= (get computed.up3 index) 0)
                                      (get-in computed.rel [(get computed.up3 index) :lz])
                                      0))}))
                 computed.rel))

(defcomputed placed (map (fn [it] (merge it {:x (+ it.lx it.cx) :z (+ it.lz it.cz)})) computed.positions))

(defcomputed chartItems
  (concat state.stars
            (map (fn [it]
                   {:id (str "ring-" it.id)
                    :x (- (* it.cx 22) (* it.radius 22))
                    :y (- (* it.cz 22) (* it.radius 22))
                    :width (* it.radius 44)
                    :height (* it.radius 44)
                    :radius (* it.radius 22)
                    :color "transparent"
                    :borderColor "#2f3a55"
                    :borderWidth 1})
                 (filter (fn [it] (not= it.parent "")) computed.placed))
            (map (fn [it]
                   {:id (str "body-" it.id)
                    :x (- (* it.x 22) (if (= it.parent "") 9 (+ 4 (* it.size 10))))
                    :y (- (* it.z 22) (if (= it.parent "") 9 (+ 4 (* it.size 10))))
                    :width (if (= it.parent "") 18 (+ 8 (* it.size 20)))
                    :height (if (= it.parent "") 18 (+ 8 (* it.size 20)))
                    :radius (if (= it.parent "") 9 (+ 4 (* it.size 10)))
                    :color it.hex
                    :borderColor (if (= it.id state.selected) "#fbbf24" "transparent")
                    :borderWidth 2})
                 computed.placed)
            (map (fn [it]
                   {:id (str "label-" it.id)
                    :x (+ (* it.x 22) 10)
                    :y (- (* it.z 22) 10)
                    :width 110
                    :height 20
                    :color "transparent"
                    :borderWidth 0
                    :text it.name
                    :textSize 16
                    :textColor "#aab2c5"})
                 (filter (fn [it] (= it.parent "sun")) computed.placed))
            state.notes))

(defcomputed catalogue
  (map (fn [it] {:id it.id :name it.name :parent it.parent :thumbnail (str "body:" (or-else it.look it.id))})
         state.bodies))

(defcomputed graphNodes
  (map-indexed (fn [index it]
                   {:id it.id
                    :title it.name
                    :x (or-else it.gx 400)
                    :y (or-else it.gy (+ 20 (* index 40)))
                    :width 120
                    :inputs (if (= it.parent "") [] [{:id "orbits" :label "orbits" :kind "orbit" :links 1}])
                    :outputs [{:id "satellites" :label "satellites" :kind "orbit"}]})
                 state.bodies))

(defcomputed graphLinks
  (map (fn [it] {:from it.parent :output "satellites" :to it.id :input "orbits"})
         (filter (fn [it] (not= it.parent "")) state.bodies)))

(defcomputed tracks
  [{:id "eclipses"
      :label "Eclipses"
      :locked (>= (indexOf state.locked "eclipse") 0)
      :muted (>= (indexOf state.muted "eclipse") 0)
      :clips (map (fn [it] {:id it.id :label it.label :start (- it.time 2) :duration 4 :color "#7c3aed"})
                  (filter (fn [it] (= it.kind "eclipse")) computed.events))}
     {:id "conjunctions"
      :label "Conjunctions"
      :locked (>= (indexOf state.locked "conjunction") 0)
      :muted (>= (indexOf state.muted "conjunction") 0)
      :clips (map (fn [it] {:id it.id :label it.label :start (- it.time 2) :duration 4 :color "#be185d"})
                  (filter (fn [it] (= it.kind "conjunction")) computed.events))}
     {:id "oppositions"
      :label "Oppositions"
      :locked (>= (indexOf state.locked "opposition") 0)
      :muted (>= (indexOf state.muted "opposition") 0)
      :clips (map (fn [it] {:id it.id :label it.label :start (- it.time 2) :duration 4 :color "#b45309"})
                  (filter (fn [it] (= it.kind "opposition")) computed.events))}])

(defcomputed next
  (find (fn [it] (and (> it.time (+ state.time 0.5)) (< (indexOf state.muted it.kind) 0))) computed.events))

(defcomputed previous
  (get (sort-by (fn [it] it.time)
                  >
                  (filter (fn [it] (and (< it.time (- state.time 0.5)) (< (indexOf state.muted it.kind) 0)))
                          computed.events))
         0))

(defcomputed goal (if (< state.speed 0) computed.previous computed.next))

(defcomputed selectedBody (find (fn [it] (= it.id state.selected)) state.bodies))

(defcomputed selectedPosition (find (fn [it] (= it.id state.selected)) computed.placed))

(defcomputed followed (find (fn [it] (= it.id state.follow)) computed.placed))

(defcomputed flyPosition (if (not= state.flying "") (find (fn [it] (= it.id state.flying)) computed.placed) nil))

(defcomputed pane {:w (max (- window.width 461) 200) :h (max (- window.height 324) 160)})

(defcomputed view
  {:w computed.pane.w
     :h computed.pane.h
     :f (* state.zoom (min computed.pane.w computed.pane.h))
     :distance state.camera.distance
     :cy (cos state.camera.yaw)
     :sy (sin state.camera.yaw)
     :cp (cos state.camera.pitch)
     :sp (sin state.camera.pitch)})

(defcomputed stars
  (map-indexed (fn [index it]
                   {:u (- (* (mod (abs (* (sin (* index 12.9898)) 43758.5453)) 1) 2) 1)
                    :t (* (mod (abs (* (sin (* index 78.233)) 12543.917)) 1) 2 (pi))
                    :mag (+ 0.5 (* 6 (sqrt (mod (abs (* (sin (* index 39.425)) 24634.633)) 1))))})
                 (range 400)))

(defcomputed starVectors
  (map (fn [it]
           {:x (* (sqrt (- 1 (* it.u it.u))) (cos it.t))
            :y it.u
            :z (* (sqrt (- 1 (* it.u it.u))) (sin it.t))
            :mag it.mag})
         computed.stars))

(defcomputed target
  (if (and (not= state.follow "") computed.followed)
        {:x computed.followed.x :z computed.followed.z}
        {:x (+ state.camera.tx (* (if computed.flyPosition (- computed.flyPosition.x state.camera.tx) 0) state.fly))
         :z (+ state.camera.tz (* (if computed.flyPosition (- computed.flyPosition.z state.camera.tz) 0) state.fly))}))

(defcomputed turned
  (map (fn [it]
           (merge it
                  {:x1 (- (* (- it.x computed.target.x) computed.view.cy)
                          (* (- it.z computed.target.z) computed.view.sy))
                   :z1 (+ (* (- it.x computed.target.x) computed.view.sy)
                          (* (- it.z computed.target.z) computed.view.cy))}))
         computed.placed))

(defcomputed projected
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

(defcomputed shaded
  (map (fn [it]
           (merge it
                  {:k (if (= it.parent "")
                          1
                          (/ (+ 1
                                (/ (+ (* (- computed.sun.x1 it.x1) (- it.x1))
                                      (* (- computed.sun.z1 it.z1)
                                         (- (* computed.view.distance computed.view.cp) it.z1)))
                                   (max (* (hypot (- computed.sun.x1 it.x1) (- computed.sun.z1 it.z1))
                                           (sqrt (+ (- (+ (* it.x1 it.x1)
                                                          (* computed.view.distance computed.view.distance))
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

(defcomputed sight (find (fn [it] (= it.id state.selected)) computed.projected))

(defcomputed sun (find (fn [it] (= it.parent "")) computed.turned))

(defcomputed range
  (if computed.sight
        (sqrt (+ (- (+ (* computed.sight.x1 computed.sight.x1) (* computed.view.distance computed.view.distance))
                    (* 2 computed.view.distance computed.sight.z1 computed.view.cp))
                 (* computed.sight.z1 computed.sight.z1)))
        0))

(defcomputed info
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

(defcomputed apparent
  (if (and computed.sight (> computed.range 0)) (degrees (* 2 (atan (/ computed.sight.size computed.range)))) 0))

(defcomputed apparentText
  (cond (>= computed.apparent 1) (str (fixed computed.apparent 2) "°")
          (>= (* computed.apparent 60) 1) (str (fixed (* computed.apparent 60) 1) "′")
          :else (str (fixed (* computed.apparent 3600) 1) "″")))

(defcomputed clock
  (str (formatDate (+ (dateMs "2030-01-01") (* state.time 86400000)) "yyyy-MM-dd HH:mm:ss" "utc") " UTC"))

(defcomputed realTime (< (abs (- (* (abs state.speed) 1728000) 1)) 0.01))

(defcomputed rate
  (if state.playing
        (+ (if (< state.speed 0) "Reversed, " "")
           (if computed.realTime
               "Real time"
               (str (formatNumber (* (abs state.speed) 1728000) 0 0)
                    "x faster"
                    (if (< (abs state.speed) 0.0005)
                        ""
                        (str ": "
                             (fixed (* (abs state.speed) 20) (if (< (abs state.speed) 0.05) 2 0))
                             " days a second")))))
        "Paused"))

(defcomputed viewMenu
  (map (fn [it]
           (merge it
                  {:checked (cond (= it.run "toggleTheme") (= state.theme "light")
                                  (= it.run "toggleOrbits") state.showOrbits
                                  (= it.run "toggleLabels") state.showLabels
                                  (= it.run "toggleBrowser") state.showBrowser
                                  (= it.run "toggleStars") state.showStars
                                  (= it.run "toggleShading") state.showShading
                                  (= it.run "toggleGlow") state.showGlow
                                  (= it.run "toggleMarker") (>= (indexOf state.markers state.selected) 0)
                                  (= it.run "follow") (and (not= state.follow "") (some? state.follow))
                                  :else false)}))
         menus.view))

(defcomputed toolbarItems
  [{:title "Back to day 0" :icon "skip-back" :keyLabel "Ctrl+0" :run "rewind"}
     {:title (if state.playing "Pause" "Play")
      :icon (if state.playing "pause" "play")
      :keyLabel "Space"
      :run "togglePlay"}
     {:title (if (< state.speed 0) "Previous event" "Next event")
      :icon (if (< state.speed 0) "skip-back" "skip-forward")
      :keyLabel "Ctrl+N"
      :run "nextEvent"}
     {:separator true}
     {:title "Slower" :icon "chevrons-left" :keyLabel "K" :run "slower"}
     {:title "Reverse" :icon "arrow-left-right" :keyLabel "J" :checked (< state.speed 0) :run "reverse"}
     {:title "Faster" :icon "chevrons-right" :keyLabel "L" :run "faster"}
     {:title "Real time"
      :icon "timer"
      :keyLabel "Shift+K"
      :checked (and state.playing computed.realTime)
      :run "realTime"}
     {:separator true}
     {:title "Go to the selected body" :icon "rocket" :keyLabel "G" :run "goTo"}
     {:title "Centre the selected body" :icon "crosshair" :keyLabel "C" :run "centre"}
     {:title "Follow the selected body"
      :icon "locate-fixed"
      :keyLabel "F"
      :checked (and (not= state.follow "") (some? state.follow))
      :run "follow"}
     {:title "Mark the selected body"
      :icon "scan"
      :keyLabel "M"
      :checked (>= (indexOf state.markers state.selected) 0)
      :run "toggleMarker"}
     {:separator true}
     {:title "Orbits" :icon "orbit" :keyLabel "O" :checked state.showOrbits :run "toggleOrbits"}
     {:title "Labels" :icon "tag" :keyLabel "P" :checked state.showLabels :run "toggleLabels"}
     {:title "Browser" :icon "list-tree" :keyLabel "B" :checked state.showBrowser :run "toggleBrowser"}
     {:separator true}
     {:title "Render options" :icon "sliders-horizontal" :keyLabel "Ctrl+," :run "openRender"}])

(defcomputed statusItems
  [{:text (str "Day " (/ (round (* state.time 10)) 10)) :color "@accent"}
     {:text (if computed.goal
                (+ (if (< state.speed 0) "Previous: " "Next: ")
                   computed.goal.label
                   " on day "
                   (round computed.goal.time)
                   " ("
                   (if (< state.speed 0)
                       (str (round (- state.time computed.goal.time)) " days ago)")
                       (str "in " (round (- computed.goal.time state.time)) " days)")))
                "No more events in this window")}
     {:side "end"
      :text (if computed.selectedBody
                (+ computed.selectedBody.name
                   (if computed.selectedBody.parent
                       (str ", orbits "
                            (or-else (:name (find (fn [it] (= it.id computed.selectedBody.parent)) state.bodies))
                                     computed.selectedBody.parent)
                            " every "
                            computed.selectedBody.period
                            " days")
                       ", at the centre"))
                "Nothing selected")}])

(defcomputed fov
  {:deg (degrees (* 2 (atan (/ (min computed.view.w computed.view.h) (* 2 computed.view.f)))))
     :times (/ state.zoom 1.1)})

(defcomputed fovText
  (str "FOV: "
         (floor computed.fov.deg)
         "° "
         (pad (floor (* (- computed.fov.deg (floor computed.fov.deg)) 60)) 2 "0")
         "′ ("
         (fixed computed.fov.times 2)
         "x)"))

(defcomputed bodyCells
  (map-indexed (fn [index it]
                   (if (= (get-in state.bodies [(mod index (count state.bodies)) :parent])
                          (get-in state.bodies [(floor (/ index (count state.bodies))) :id]))
                       (mod index (count state.bodies))
                       (- 1)))
                 (range (* (count state.bodies) (count state.bodies)))))

(defcomputed bodyKids
  (map-indexed (fn [index it]
                   (filter (fn [it] (>= it 0))
                           (slice computed.bodyCells
                                  (* index (count state.bodies))
                                  (* (+ index 1) (count state.bodies)))))
                 state.bodies))

(defcomputed bodyRows
  (map (fn [it]
           {:id it.id
            :label it.name
            :icon (cond (= it.parent "") "sun" (= it.parent "sun") "circle" :else "moon")
            :iconColor it.hex})
         state.bodies))

(defcomputed bodyLevel1
  (map-indexed (fn [index it]
                   (if (> (count (get computed.bodyKids index)) 0)
                       (merge it {:children (map (fn [it] (get computed.bodyRows it)) (get computed.bodyKids index))})
                       it))
                 computed.bodyRows))

(defcomputed bodyLevel2
  (map-indexed (fn [index it]
                   (if (> (count (get computed.bodyKids index)) 0)
                       (merge it
                              {:children (map (fn [it] (get computed.bodyLevel1 it)) (get computed.bodyKids index))})
                       it))
                 computed.bodyLevel1))

(defcomputed bodyLevel3
  (map-indexed (fn [index it]
                   (if (> (count (get computed.bodyKids index)) 0)
                       (merge it
                              {:children (map (fn [it] (get computed.bodyLevel2 it)) (get computed.bodyKids index))})
                       it))
                 computed.bodyLevel2))

(defcomputed bodyTree
  (map (fn [it] (get computed.bodyLevel3 it))
         (filter (fn [it] (= (get-in state.bodies [it :parent]) "")) (range (count state.bodies)))))

(defcomputed bookmarkRows (map (fn [it] {:id it.id :label it.label :icon "bookmark"}) state.bookmarks))

(defcomputed eclipseOrbits
  (map (fn [it]
           (merge it
                  {:d (- (/ 1 it.period) (/ 1 it.host.period))
                   :o (- it.phase it.host.phase)
                   :from (ceil (* 2
                                  (min (- it.phase it.host.phase)
                                       (+ (- it.phase it.host.phase)
                                          (* (- (/ 1 it.period) (/ 1 it.host.period)) state.duration)))))
                   :to (floor (* 2
                                 (max (- it.phase it.host.phase)
                                      (+ (- it.phase it.host.phase)
                                         (* (- (/ 1 it.period) (/ 1 it.host.period)) state.duration)))))}))
         (filter (fn [it] (and (= it.host.parent "sun") (not= it.period it.host.period)))
                 (map (fn [it] (merge it {:host (get state.bodies (indexOf computed.ids it.parent))}))
                      (filter (fn [it] (and (not= it.parent "") (not= it.parent "sun"))) state.bodies)))))

(defcomputed eclipseSpan (max (concat [0] (map (fn [it] (+ (- it.to it.from) 1)) computed.eclipseOrbits))))

(defcomputed eclipses
  (sort-by (fn [it] it.time)
             (map (fn [it]
                    {:id (str it.m.id "-" it.n)
                     :time (/ (round (* it.time 10)) 10)
                     :solar (not= (mod it.n 2) 0)
                     :moon it.m.id
                     :moonName it.m.name
                     :host it.m.host.id
                     :hostName it.m.host.name
                     :hours (/ (* (if (not= (mod it.n 2) 0) 0.0186 0.0405) 24) (* 2 (pi) (abs it.m.d)))
                     :label (if (not= (mod it.n 2) 0)
                                (str it.m.name " eclipses the Sun")
                                (str it.m.name " in the shadow of " it.m.host.name))})
                  (filter (fn [it]
                            (or (nil? it.m.eclipseYear)
                                (< (* (abs (- (- it.time it.m.nodeDay)
                                              (/ (* (round (/ (* (- it.time it.m.nodeDay) 2) it.m.eclipseYear))
                                                    it.m.eclipseYear)
                                                 2)))
                                      2)
                                   it.m.eclipseSeason)))
                          (map (fn [it] (merge it {:time (/ (- (/ it.n 2) it.m.o) it.m.d)}))
                               (filter (fn [it] (<= it.n it.m.to))
                                       (map (fn [it]
                                              {:m (get computed.eclipseOrbits (floor (/ it computed.eclipseSpan)))
                                               :n (+ (get-in computed.eclipseOrbits
                                                             [(floor (/ it computed.eclipseSpan)) :from])
                                                     (mod it computed.eclipseSpan))})
                                            (range (* (count computed.eclipseOrbits) computed.eclipseSpan)))))))))

(defcomputed events
  (sort-by (fn [it] it.time)
             (concat state.events
                     (map (fn [it]
                            {:id it.id
                             :time it.time
                             :kind "eclipse"
                             :label (if it.solar "Solar eclipse" "Lunar eclipse")})
                          computed.eclipses))))

(defcomputed eclipseRows
  (map (fn [it]
           {:id it.id
            :label (str (formatDate (+ (dateMs "2030-01-01") (* (- it.time (/ it.hours 48)) 86400000))
                                    "yyyy-MM-dd HH:mm"
                                    "utc")
                        "   "
                        (floor it.hours)
                        " h "
                        (round (* (mod it.hours 1) 60))
                        " min   "
                        (if it.solar "Solar, " "Lunar, ")
                        it.hostName
                        " and the "
                        it.moonName)
            :icon (if it.solar "sun" "moon")})
         computed.eclipses))

(defcomputed renderOptions
  [{:id "orbits" :title "Orbits" :on state.showOrbits :run "toggleOrbits"}
     {:id "labels" :title "Labels" :on state.showLabels :run "toggleLabels"}
     {:id "stars" :title "Stars" :on state.showStars :run "toggleStars"}
     {:id "shading" :title "Lit side" :on state.showShading :run "toggleShading"}
     {:id "glow" :title "Sun glow" :on state.showGlow :run "toggleGlow"}])

(defcomputed sceneView
  {:camera {:target [computed.target.x 0 computed.target.z]
              :yaw state.camera.yaw
              :pitch state.camera.pitch
              :distance computed.view.distance
              :fieldOfView (* 2 (atan (/ computed.view.h (* 2 computed.view.f))))}
     :selection (if (not= state.selected "") [state.selected] [])})

(defcomputed orbitRings
  (map (fn [it] {:id it.id :cx it.cx :cz it.cz :radius it.radius})
         (filter (fn [it] (not= it.parent "")) computed.placed)))

(defcomputed orbitSegments
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

(defcomputed starNodes
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

(defcomputed bodyNodes
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

(defcomputed bodyMaterials
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

(defcomputed sceneGraph
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

(defcomputed magText
  (str "Limiting magnitude: "
         (fixed state.limitMag 1)
         " ("
         (count (filter (fn [it] (<= it.mag state.limitMag)) computed.stars))
         " stars)"))

(defcomputed marks (filter (fn [it] (and (>= (indexOf state.markers it.id) 0) (> it.depth 0.3))) computed.projected))

(defcomputed marked
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

(defstyle button
  (label :height 30
    :radius 6
    :padding 8
    :color @raised
    :textColor @text
    :align center
    :cursor hand
    :focusable true
    :hover {:color @border}
    :focused {:borderWidth 2 :borderColor @accent}))

(defstyle heading (label :height 26 :padding 6 :textColor @muted :font small))

(defmenu time
  {:faster {:run faster :title "10x faster" :group "1_rate" :order 1}
   :slower {:run slower :title "10x slower" :group "1_rate" :order 2}
   :freeze {:run togglePlay :title "Freeze" :group "1_rate" :order 3}
   :realTime {:run realTime :group "1_rate" :order 4}
   :reverse {:run reverse :title "Reverse time" :group "1_rate" :order 5}
   :now {:run now :title "Back to day 0" :group "2_set" :order 1}
   :eclipses {:run openEclipses :group "2_set" :order 2}})

(defmenu view
  {:goTo {:run goTo :group "1_go" :order 1}
   :centre {:run centre :group "1_go" :order 2}
   :follow {:run follow :group "1_go" :order 3}
   :orbits {:run toggleOrbits :group "2_show" :order 1}
   :labels {:run toggleLabels :group "2_show" :order 2}
   :browser {:run toggleBrowser :group "2_show" :order 3}
   :narrow {:run zoomIn :group "3_fov" :order 1}
   :widen {:run zoomOut :group "3_fov" :order 2}
   :bookmark {:run addBookmark :group "4_places" :order 1}
   :deleteBookmark {:run deleteBookmark :group "4_places" :order 2}
   :eclipses {:run openEclipses :group "4_places" :order 3}
   :renderOptions {:run openRender :group "2_show" :order 7}
   :theme {:run toggleTheme :group "2_show" :order 8}
   :stars {:run toggleStars :group "2_show" :order 4}
   :shading {:run toggleShading :group "2_show" :order 5}
   :glow {:run toggleGlow :group "2_show" :order 6}
   :mark {:run toggleMarker :group "1_go" :order 4}
   :fainter {:run fainter :group "3_fov" :order 3}
   :brighter {:run brighter :group "3_fov" :order 4}})

(deftokens tokens "Tokens."
  {:background {:light "#ebf0ee" :dark "#0d1414"}
   :panel {:light "#f7faf9" :dark "#131c1c"}
   :raised {:light "#dfe8e5" :dark "#1b2827"}
   :border {:light "#c3d0cc" :dark "#263634"}
   :text {:light "#172120" :dark "#e3ecea"}
   :muted {:light "#536662" :dark "#8fa39f"}
   :accent {:light "#0f766e" :dark "#2dd4bf"}
   :sky {:light "#0b1213" :dark "#070b0c"}
   :note {:light "#fcd9b8" :dark "#fcd9b8"}
   :mark {:light "#e11d74" :dark "#f472b6"}
   :onSky "#e3ecea"
   :onSkyMuted "#93a8a3"
   :hud "#0b1514e6"
   :hudBorder "#2dd4bf38"
   :assistant.accent @accent
   :assistant.accentHover {:light "#0d9488" :dark "#5eead4"}
   :assistant.onAccent {:light "#f2fffc" :dark "#04201c"}
   :assistant.surface @panel
   :assistant.raised @raised
   :assistant.border @border
   :assistant.text @text
   :assistant.dim @muted})

(defkey "mod+p" togglePlay)

(defkey "mod+n" nextEvent)

(defkey "mod+0" rewind)

(defkey "l" faster)

(defkey "k" slower)

(defkey "j" reverse)

(defkey "space" togglePlay)

(defkey "shift+1" now)

(defkey "shift+k" realTime)

(defkey "g" goTo)

(defkey "c" centre)

(defkey "f" followSelected)

(defkey "o" toggleOrbits)

(defkey "p" toggleLabels)

(defkey "b" toggleBrowser)

(defkey "mod+d" addBookmark)

(defkey "period" zoomIn)

(defkey "comma" zoomOut)

(defkey "mod+comma" openRender)

(defkey "equal" fainter)

(defkey "minus" brighter)

(defkey "m" toggleMarker)

(defpart index "What index adds."
  :watch {:state.camera [{:run syncScene}]
          :state.zoom [{:run syncScene}]
          :state.follow [{:run syncScene}]
          :state.fly [{:run syncScene}]
          :state.time [{:run syncScene}]
          :state.selected [{:run syncScene}]
          :state.bodies [{:run syncScene}]}
  :onLoad [{:run syncScene}]
  :actions {:togglePlay [{:toggle state.playing}]
            :rewind [{:set state.playing :to false} {:set state.time :to 0}]
            :tick [{:if ~state.playing
                    :then [{:set state.time :to ~(clamp (+ state.time state.speed) 0 state.duration)}
                           {:if ~(or (and (< state.speed 0) (<= state.time 0))
                                     (and (> state.speed 0) (>= state.time state.duration)))
                            :then [{:set state.playing :to false}]}]}]
            :deleteNotes [{:set state.notes :to ~(filter (fn [it] (not (contains state.chart.selection it.id))) state.notes)}]
            :clearNotes [{:set state.notes :to []}]
            :viewPress [{:set state.grab :to ~state.camera}]
            :viewRelease [{:set state.grab :to nil}]
            :reverse [{:set state.speed :to ~(- state.speed)}]
            :now [{:set state.time :to 0} {:set state.speed :to 0.01}]
            :toggleOrbits {:title "Show orbits" :icon orbit :do [{:toggle state.showOrbits}]}
            :toggleLabels {:title "Show labels" :icon tag :do [{:toggle state.showLabels}]}
            :toggleStars {:title "Show stars" :icon sparkles :do [{:toggle state.showStars}]}
            :toggleShading {:title "Show the lit side" :icon moon :do [{:toggle state.showShading}]}
            :toggleGlow {:title "Show the Sun's glow" :icon sun :do [{:toggle state.showGlow}]}
            :toggleTheme {:title "Light theme"
                          :icon sun-moon
                          :do [{:set state.theme :to ~(if (= state.theme "dark") "light" "dark")}]}
            :openRender {:title "Render options" :icon sliders-horizontal :do [{:set state.renderOpen :to true}]}
            :zoomIn {:title "Narrow the field of view" :icon zoom-in :do [{:set state.zoom :to ~(min (* state.zoom 1.25) 20)}]}
            :zoomOut {:title "Widen the field of view" :icon zoom-out :do [{:set state.zoom :to ~(max (/ state.zoom 1.25) 0.5)}]}
            :fainter {:title "Show fainter stars" :icon sparkle :do [{:set state.limitMag :to ~(min (+ state.limitMag 0.5) 6.5)}]}
            :brighter {:title "Show only brighter stars"
                       :icon star
                       :do [{:set state.limitMag :to ~(max (- state.limitMag 0.5) 1)}]}
            :toggleBrowser {:title "Show the browser" :icon list-tree :do [{:toggle state.showBrowser}]}
            :openEclipses {:title "Eclipse finder"
                           :icon moon
                           :do [{:set state.showBrowser :to true} {:set state.browserTab :to eclipses}]}
            :syncScene [{:set state.sceneView :to ~computed.sceneView}]}
  )

;;; Views

(defview menu-bar "#menu-bar." []
  `(node#menu-bar :when ~(not window.nativeMenuBar)
     :use menuBar
     :width 56
     :with {:open state.menuBar
            :menus ~[{:name "view" :title "View" :items computed.viewMenu}
                     {:name "time"
                      :title "Time"
                      :items (map (fn [it]
                                    (merge it
                                           {:checked (cond (= it.run "realTime") (and state.playing computed.realTime)
                                                           (= it.run "reverse") (< state.speed 0)
                                                           (= it.run "togglePlay") (not state.playing)
                                                           :else nil)}))
                                  menus.time)}]
            :width 260
            :color @panel
            :textColor @text
            :dimColor @muted
            :highlight @raised}))

(defview info-lines "#info-lines." []
  `(column#info-lines :x 14 :y 8 :width 230 :height 110 :gap 2
     (label#info-name :height 22 :fitText true :font title :textColor @onSky :text ~computed.info.name)
     (label#info-distance :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :text ~(str "Distance: " (fixed computed.info.distance 2) " AU"))
     (label#info-radius :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :text ~(str "Radius: " computed.selectedBody.km " km"))
     (label#info-apparent :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :text ~(str "Apparent diameter: " computed.apparentText))
     (label#info-phase :height 18
       :fitText true
       :font small
       :textColor @onSkyMuted
       :visible ~(not= computed.selectedBody.parent "")
       :text ~(str "Phase: " (round (* computed.info.lit 100)) "% lit"))))

(defview view-time "#view-time." []
  `(column#view-time :anchors {:left ~(max (/ (- (- window.width 461) 300) 2) 12) :top 10}
     :width 300
     :gap 0
     :justify center
     :color @hud
     :radius 22
     :borderWidth 1
     :borderColor @hudBorder
     :children [(label#time-date :height 20
                  :width 300
                  :align {:horizontal center :vertical center}
                  :font small
                  :textColor @onSky
                  :text ~computed.clock) (label#time-rate :height 18
                                           :width 300
                                           :align {:horizontal center :vertical center}
                                           :font small
                                           :textColor ~(if state.playing "@accent" "@onSkyMuted")
                                           :text ~computed.rate)]
     :height 44))

(defview browser-bodies "#browser-bodies." []
  `(tree#browser-bodies :width 220
     :height 220
     :visible ~(= state.browserTab "bodies")
     :items ~computed.bodyTree
     :expanded ~state.expandedBodies
     :value ~state.selected
     :rowHeight 24
     :onChange [{:set state.selected :to ~event.value}]
     :onOpen [{:set state.selected :to ~event.value} ~(handler goTo)]
     :onExpand [{:set state.expandedBodies
                 :to ~(if event.expanded
                          (concat (filter (fn [it] (not= it event.id)) state.expandedBodies) [event.id])
                          (filter (fn [it] (not= it event.id)) state.expandedBodies))}]
     :color @panel
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @raised
     :accentColor @accent))

(defview places-bar "#places-bar." []
  `(row#places-bar :width 220 :height 34 :gap 6 :alignItems center
     (label#add-bookmark :style button
       :width 170
       :height 28
       :icon bookmark-plus
       :text "Bookmark this view"
       :font small
       :role button
       :onClick addBookmark)
     (label#delete-bookmark :style button
       :width 32
       :height 28
       :icon trash-2
       :role button
       :label "Delete the bookmark"
       :tooltip "Delete the bookmark"
       :onClick deleteBookmark)))

(defview browser "#browser." []
  `(column#browser :anchors {:right 12 :top 66}
     :width ~(if (= state.browserTab "eclipses") 360 220)
     :height ~(clamp (- window.height 526) 150 250)
     :visible ~state.showBrowser
     :color @panel
     :radius 10
     :borderWidth 1
     :borderColor @border
     (tabs#browser-tabs :width ~(if (= state.browserTab "eclipses") 360 220)
       :height 30
       :tabs [{:id bodies :label "Bodies" :closable false}
              {:id places :label "Places" :closable false}
              {:id eclipses :label "Eclipses" :closable false}]
       :bind state.browserTab
       :color @panel
       :textColor @muted
       :activeTextColor @text
       :activeBackground @raised
       :accentColor @accent)
     ~(browser-bodies)
     ~(browser-places)
     (list#browser-eclipses :width ~(if (= state.browserTab "eclipses") 360 220)
       :height 220
       :visible ~(= state.browserTab "eclipses")
       :items ~computed.eclipseRows
       :value ~state.eclipse
       :rowHeight 24
       :onChange [~(handler goToEclipse)]
       :onOpen [~(handler goToEclipse)]
       :color @panel
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @raised
       :accentColor @accent)))

(defview browser-places "#browser-places, in browser." []
  `(column#browser-places :width 220 :height 220 :visible ~(= state.browserTab "places") :color @panel
     (list#browser-bookmarks :width 220
       :height 186
       :items ~computed.bookmarkRows
       :value ~state.bookmark
       :rowHeight 24
       :onChange [~(handler goToBookmark)]
       :onOpen [~(handler goToBookmark)]
       :color @panel
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @raised
       :accentColor @accent)
     ~(places-bar)))

(defview view-pane "The view is a picture of the sky: its markers, labels and readouts keep left to right in every locale." []
  `(panel#view-pane :dir ltr
     :note "The view is a picture of the sky: its markers, labels and readouts keep left to right in every locale."
     :grow 1
     :alignSelf stretch
     :color @sky
     :onPress viewPress
     :onDrag viewDrag
     :onRelease viewRelease
     :onScroll viewScroll
     ~(view-scene)
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
       `(panel :id ~(str "thumb-" b.id) :snapshot ~{:image (str "body:" b.id) :scale 1} :width 96 :height 64 :color "#07090e"
          (panel :id ~(str "thumb-disc-" b.id)
            :x 25
            :y 9
            :width 46
            :height 46
            :radius 999
            :color ~b.hex
            :shadow {:blur 14 :color ~(if (= b.parent "") "#ffc94d99" "#00000000")})))
     ~(view-info)
     ~(view-time)
     ~(browser)
     ~(view-hint)
     ~(view-optics)))

(defview view-hint "#view-hint, in view-pane." []
  `(label#view-hint :anchors {:right 14 :bottom 46}
     :fitText true
     :height 20
     :font small
     :textColor @onSkyMuted
     :text "Drag to turn, scroll to zoom; G go to, C centre, F follow; B browser"
     :visible ~(and (not state.showBrowser) (>= (- window.width 461) 600))))

(defview view-info "#view-info, in view-pane." []
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

(defview view-optics "#view-optics, in view-pane." []
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

(defview view-scene "#view-scene, in view-pane." []
  `(scene#view-scene :fill true
     :bind state.sceneView
     :scene ~computed.sceneGraph
     :grid false
     :axes false
     :background "#070b0c"
     :outlineColor @accent
     :controls {:orbit none :pan none :zoom false :select left :selects false}
     :onSelect [{:if ~(and (= event.button "left")
                           (>= (indexOf computed.ids (if (= event.id "halo") "sun" (or-else event.id ""))) 0))
                 :then [{:set state.selected :to ~(if (= event.id "halo") "sun" event.id)}]}]))

(defview chart-bar "#chart-bar." []
  `(row#chart-bar :width "100%" :height 40 :gap 6 :alignItems center :color @panel
     ~(sky-mode-ground)
     ~(sky-mode-above)
     (textarea#note-text :visible ~(= state.skyMode "above")
       :bind state.draft
       :grow 1
       :height 28
       :radius 4
       :color @background
       :textColor @text
       :label "Annotation text"
       :placeholder "Note"
       :onSubmit addNote)
     ~(add-note)
     {:id mark
      :visible ~(= state.skyMode "above")
      :style button
      :icon crosshair
      :role button
      :label "Mark the selected body"
      :tooltip "Mark the selected body"
      :onClick markSelected
      :width 30}
     {:id clear-notes
      :visible ~(= state.skyMode "above")
      :style button
      :icon trash-2
      :role button
      :label "Clear the annotations"
      :tooltip "Clear the annotations"
      :onClick clearNotes
      :width 30}))

(defview add-note "#add-note, in chart-bar." []
  `{:id add-note
    :visible ~(= state.skyMode "above")
    :style button
    :icon sticky-note
    :role button
    :label "Add a note in the middle of the chart"
    :tooltip "Add a note in the middle of the chart"
    :onClick addNote
    :width 30})

(defview sky-mode-above "#sky-mode-above, in chart-bar." []
  `{:id sky-mode-above
    :style button
    :role button
    :text "From above"
    :label "The chart from above"
    :selected ~(= state.skyMode "above")
    :color ~(if (= state.skyMode "above") "@raised" "@panel")
    :fitText true
    :onClick skyAbove})

(defview sky-mode-ground "#sky-mode-ground, in chart-bar." []
  `{:id sky-mode-ground
    :style button
    :role button
    :text "From the ground"
    :label "The sky from the ground"
    :selected ~(= state.skyMode "ground")
    :color ~(if (= state.skyMode "ground") "@raised" "@panel")
    :fitText true
    :onClick skyGround})

(defview chart-info "#chart-info." []
  `(column#chart-info :visible ~(some? computed.selectedPosition)
     :height ~(if (= state.skyMode "ground") 62 42)
     :anchors {:left 8 :top 6}
     :width 260
     :gap 2
     (label#chart-info-name :height 22 :fitText true :font title :text ~computed.selectedBody.name :textColor @onSky)
     (label#chart-info-line :height 18
       :fitText true
       :font small
       :textColor ~(if (= state.skyMode "ground") "#dfe7f2" "@onSkyMuted")
       :text ~(cond (= state.skyMode "ground") (or-else computed.skyInfo.0 "Where you stand")
                    (= computed.selectedBody.parent "") "The centre of the chart"
                    :else (str (fixed (/ (hypot computed.selectedPosition.x computed.selectedPosition.z) 4.3) 2)
                         " AU from the Sun, orbits "
                         (if (= computed.selectedBody.parent "sun")
                             "the Sun"
                             (:name (find (fn [it] (= it.id computed.selectedBody.parent)) state.bodies)))
                         " every "
                         computed.selectedBody.period
                         " days")))
     (label#chart-info-sky :visible ~(= state.skyMode "ground")
       :height 18
       :fitText true
       :font small
       :textColor "#dfe7f2"
       :text ~(or-else computed.skyInfo.1 ""))))

(defview chart-frame "#chart-frame." []
  `(panel#chart-frame :width "100%" :height ~(clamp (- window.height 524) 120 300)
     (canvas#chart :visible ~(= state.skyMode "above")
       :bind state.chart
       :color @sky
       :rulers false
       :snap false
       :accentColor @accent
       :items ~computed.chartItems
       :label "Sky chart"
       :onTransform moveNotes
       :keys {:delete deleteNotes :backspace deleteNotes}
       :anchors {:left 0 :right 0 :top 0 :bottom 0})
     (node#sky-view :use
                    skyFromGround
                    :visible
                    ~(= state.skyMode "ground")
                    :anchors
                    {:left 0 :right 0 :top 0 :bottom 0})
     ~(chart-info)))

(defview orbits-view "#orbits." []
  `(graph#orbits :width "100%"
     :grow 1
     :color @background
     :minimap false
     :nodes ~computed.graphNodes
     :links ~computed.graphLinks
     :selectedIds ~[state.selected]
     :kindColors {:orbit "#2dd4bf"}
     :nodeColor @raised
     :headerColor @border
     :textColor @text
     :accentColor @accent
     :zoom 0.8
     :onMove moveGraphNodes
     :onConnect reparent
     :onDisconnect orbitAround
     :onSelect [{:if ~(> (count event.ids) 0) :then [{:set state.selected :to ~event.ids.0}]}]))

(defview events-view "#events." []
  `(timeline#events :width "100%"
     :height 128
     :tracks ~computed.tracks
     :time ~state.time
     :duration ~state.duration
     :fps 1
     :timeFormat frames
     :pixelsPerSecond 2.6
     :headerWidth 130
     :markers ~(map (fn [it] {:time it.time :label ""})
                    (filter (fn [it] (and (= it.kind "eclipse") (< (indexOf state.muted "eclipse") 0)))
                            computed.events))
     :headerColor @panel
     :textColor @text
     :accentColor @accent
     :color @background
     :label "Upcoming events, by day; M mutes a kind of event, L locks its events in place"
     :onChange [{:if ~(= event.type "track")
                 :then [{:set state.muted
                         :to ~(if event.muted
                                  (concat (filter (fn [it] (not= it (slice event.id 0 (- (count event.id) 1))))
                                                  state.muted)
                                          [(slice event.id 0 (- (count event.id) 1))])
                                  (filter (fn [it] (not= it (slice event.id 0 (- (count event.id) 1)))) state.muted))}
                        {:set state.locked
                         :to ~(if event.locked
                                  (concat (filter (fn [it] (not= it (slice event.id 0 (- (count event.id) 1))))
                                                  state.locked)
                                          [(slice event.id 0 (- (count event.id) 1))])
                                  (filter (fn [it] (not= it (slice event.id 0 (- (count event.id) 1)))) state.locked))}]}]
     :onScrub [{:set state.playing :to false} {:set state.time :to ~event.time}]
     :onSelect [{:if ~(find (fn [it] (= it.id event.ids.0)) computed.events)
                 :then [{:set state.playing :to false}
                        {:set state.time :to ~(:time (find (fn [it] (= it.id event.ids.0)) computed.events))}]}]))

(defview layout "#layout." []
  `(column#layout :fill true
     (row#top-bar :width "100%" :height 36 :alignItems center :color @panel
       (label#app-title :font title :text "Orrery" :textColor @accent :width 84 :height 30 :align center)
       ~(menu-bar)
       (node#toolbar :use toolbar
         :with {:items ~computed.toolbarItems
                :width ~(- window.width (if window.nativeMenuBar 84 140))
                :label "Time and view"
                :color @panel
                :textColor @text
                :dimColor @muted
                :highlight @raised
                :checkedColor @border}))
     (row#main :width "100%" :grow 1 :gap 1 :color @border
       ~(view-pane)
       (column#side :width 460 :height "100%" :gap 1 :color @border
         ~(chart-bar)
         ~(chart-frame)
         (row#graph-bar :width "100%" :height 28 :color @panel
           (label#graph-title :style heading :text "What orbits what (drag a link to move a body)" :grow 1))
         ~(orbits-view)))
     ~(events-view)
     (column#dock :point bodies.dock :width "100%" :height 136 :color @panel :children [])
     (node#status-bar :use statusBar
       :width "100%"
       :with {:items ~computed.statusItems :label "Time and selection" :color @panel :textColor @muted :highlight @raised})))

(defview render "#render." []
  `(node#render :use dialog
     :with {:open state.renderOpen
            :title "Render options"
            :message "What the 3D view draws"
            :buttons [{:title "Done"}]
            :width 340
            :height 330}
     (column#render-options :gap 6
       :children [~(repeat [o computed.renderOptions :key o.id]
                    `(row :id ~(str "render-" o.id) :width "100%" :height 30
                       (label :id ~(str "render-" o.id "-title") :text ~o.title :textColor @text :width 200 :height 30)
                       (toggle :id ~(str "render-" o.id "-toggle")
                         :width 44
                         :height 24
                         :label ~o.title
                         :checked ~o.on
                         :activeColor @accent
                         :trackColor @border
                         :onChange {:run ~o.run})))]
       :anchors {:left 20 :right 20 :top 76 :bottom 64})))

;;; The screen

(defscreen main
  :window {:title "Orrery"
           :width 1280
           :height 860
           :minWidth 1000
           :minHeight 700
           :menuBar [{:menu view :title "View"} {:menu time :title "Time"}]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 11 :fallback [icons]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 18 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 14}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio user]}
  :theme ~state.theme
  (node#root :color @background :every {:ms 50 :do tick}
    ~(layout)
    ~(render)))
