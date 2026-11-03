;;; sky.lisp --- the sky from the ground, as a planetarium shows it
;;;
;;; The stars, the bodies and invented constellations seen from the Earth at a latitude, in a perspective (gnomonic)
;;; view, over the ground and under a sky that brightens by day.
;;; Every position is a computed value: a direction (east, north, up) from right ascension and declination and the
;;; sidereal time, then projected through the view's azimuth, altitude and field of view. Great circles are straight
;;; lines in this projection, so the horizon is a horizontal line and a constellation's lines are straight.

;;; What the sky keeps

(defstate skyMode ground)

(defstate sky {:azimuth 3.1416 :altitude 0.15 :fov 1.05 :latitude 45})

(defstate skyGrab nil)

(defstate skyLayers {:lines true :names true :grid false :ground true :cardinal true :atmosphere true :labels true})

(defstate constellations
  [{:id kite :name "The Kite" :u -0.9992 :t 2.5589}
   {:id lantern :name "The Lantern" :u -0.2032 :t 2.5476}
   {:id heron :name "The Heron" :u 0.4002 :t 0.5322}
   {:id anvil :name "The Anvil" :u 0.2723 :t 3.8755}
   {:id harp :name "The Harp" :u 0.6981 :t 4.2281}
   {:id plough-horse :name "The Plough Horse" :u -0.6841 :t 4.0601}
   {:id lighthouse :name "The Lighthouse" :u 0.9577 :t 0.5277}
   {:id fox :name "The Fox" :u -0.4466 :t 4.1576}
   {:id weaver :name "The Weaver" :u 0.192 :t 6.1564}
   {:id bell :name "The Bell" :u -0.1803 :t 1.2753}
   {:id ferry :name "The Ferry" :u -0.8389 :t 5.0192}
   {:id owl :name "The Owl" :u 0.4082 :t 1.0047}
   {:id key :name "The Key" :u -0.7651 :t 2.3502}
   {:id sickle :name "The Sickle" :u -0.0042 :t 5.6924}])

(defstate constellationLines
  [[0 184]
   [184 13]
   [0 216]
   [239 360]
   [360 396]
   [239 220]
   [220 362]
   [269 233]
   [269 58]
   [269 206]
   [206 135]
   [86 203]
   [203 307]
   [307 125]
   [103 28]
   [103 72]
   [72 361]
   [28 217]
   [317 143]
   [143 7]
   [7 393]
   [381 208]
   [208 382]
   [382 254]
   [50 350]
   [350 341]
   [50 110]
   [353 29]
   [29 280]
   [280 245]
   [280 99]
   [99 35]
   [209 344]
   [344 181]
   [209 301]
   [123 185]
   [123 309]
   [185 339]
   [25 80]
   [25 351]
   [80 201]
   [101 64]
   [64 172]
   [172 34]
   [227 224]
   [224 15]
   [224 56]
   [56 42]])

(defstate skyTrack "")

;;; Where everything is on the sky

(defcomputed skyHeight "The sky view's height." (clamp (- window.height 524) 120 300))

(defcomputed skyTracked "The body the sky view tracks, if any."
  (if (= state.skyTrack "")
    nil
    (find (fn [it] (and (= it.kind "body") (= it.id state.skyTrack))) computed.skyLocal)))

(defcomputed skyView "The sky view's size and focal length."
  (if computed.skyTracked
    (merge state.sky
           {:azimuth (atan2 computed.skyTracked.e computed.skyTracked.n)
            :altitude (clamp (asin computed.skyTracked.up) (- 0.3) 1.45)})
    state.sky))

(defcomputed skyCamera "The sky view's camera: its centre, scale and direction."
  {:f (/ 230 (tan (/ computed.skyView.fov 2)))
   :cx 230
   :cy (/ computed.skyHeight 2)
   :sinAzimuth (sin computed.skyView.azimuth)
   :cosAzimuth (cos computed.skyView.azimuth)
   :sinAltitude (sin computed.skyView.altitude)
   :cosAltitude (cos computed.skyView.altitude)})

(defcomputed skyHome "The Earth, where the sky is seen from."
  (or-else (find (fn [it] (= it.id "earth")) computed.placed) {:x 0 :z 0}))

(defcomputed skyEcliptic "The ecliptic's tilt, turning the bodies' places into the sky's."
  (map (fn [it]
         {:id it.id
          :name it.name
          :hex it.hex
          :parent it.parent
          :longitude (atan2 (- computed.skyHome.z it.z) (- it.x computed.skyHome.x))})
       (filter (fn [it] (not= it.id "earth")) computed.placed)))

(defcomputed skyEarth "Where the Earth is now."
  {:sinLatitude (sin (radians state.sky.latitude))
   :cosLatitude (cos (radians state.sky.latitude))
   :sidereal (* 2 (pi) state.time 1.0027379)})

(defcomputed skyStars "The stars above the horizon, bright enough to see."
  (map-indexed (fn [index it]
                 {:kind "star"
                  :u (- (* (mod (abs (* (sin (* index 12.9898)) 43758.5453)) 1) 2) 1)
                  :t (* (mod (abs (* (sin (* index 78.233)) 12543.917)) 1) 2 (pi))
                  :mag (+ 0.5 (* 6 (sqrt (mod (abs (* (sin (* index 39.425)) 24634.633)) 1))))
                  :tint (floor (* (mod (abs (* (sin (* index 7.131)) 9631.77)) 1) 3))})
               (range 1200)))

(defcomputed skyObjects
  "Everything in the sky: the bodies, the stars, the figures and the grid, each with its direction."
  (if (not= state.skyMode "ground")
    []
    (concat (map (fn [it] (merge it {:r (/ (round (* (max 0.5 (- 2.8 (* it.mag 0.4))) 100)) 100)}))
                 computed.skyStars)
            (map (fn [it]
                   (merge it
                          {:kind "body"
                           :u (* (sin it.longitude) 0.3978)
                           :t (atan2 (* (sin it.longitude) 0.9175) (cos it.longitude))}))
                 computed.skyEcliptic)
            (map (fn [it] (merge it {:kind "name"})) state.constellations)
            [{:kind "cardinal" :id "N" :e 0 :n 1 :up 0}
             {:kind "cardinal" :id "E" :e 1 :n 0 :up 0}
             {:kind "cardinal" :id "S" :e 0 :n (- 1) :up 0}
             {:kind "cardinal" :id "W" :e (- 1) :n 0 :up 0}]
            (if state.skyLayers.ground
              (map (fn [it]
                     {:kind "land"
                      :e (sin (radians (* 5 it)))
                      :n (cos (radians (* 5 it)))
                      :up (sin
                            (radians
                              (max 0.4
                                   (+ 1.6
                                      (* 1.3 (sin (+ (radians (* 15 it)) 1)))
                                      (* 0.8 (sin (radians (* 35 it))))
                                      (* 0.5 (sin (+ (radians (* 85 it)) 2)))))))})
                   (range 73))
              [])
            (if state.skyLayers.grid
              (map (fn [it]
                     {:kind "gridLabel"
                      :id (str "g" it)
                      :text (if (< it 12) (str (* 30 it) "°") (str (round (+ 30 (* 30 (mod it 2)))) "°"))
                      :e (if (< it 12)
                           (sin (radians (* 30 it)))
                           (* (sin (radians (* 90 (floor (/ (- it 12) 2)))))
                              (cos (radians (+ 30 (* 30 (mod it 2)))))))
                      :n (if (< it 12)
                           (cos (radians (* 30 it)))
                           (* (cos (radians (* 90 (floor (/ (- it 12) 2)))))
                              (cos (radians (+ 30 (* 30 (mod it 2)))))))
                      :up (if (< it 12) 0.02 (sin (radians (+ 30 (* 30 (mod it 2))))))})
                   (range 20))
              [])
            (if state.skyLayers.grid
              (map (fn [it]
                     {:kind "grid"
                      :line (floor (/ it 37))
                      :e (* (cos (radians (* 30 (floor (/ it 37))))) (sin (radians (* 10 (mod it 37)))))
                      :n (* (cos (radians (* 30 (floor (/ it 37))))) (cos (radians (* 10 (mod it 37)))))
                      :up (sin (radians (* 30 (floor (/ it 37)))))})
                   (range 111))
              [])
            (if state.skyLayers.grid
              (map (fn [it]
                     {:kind "grid"
                      :line (+ 3 (floor (/ it 9)))
                      :e (* (cos (radians (* 10 (mod it 9)))) (sin (radians (* 30 (floor (/ it 9))))))
                      :n (* (cos (radians (* 10 (mod it 9)))) (cos (radians (* 30 (floor (/ it 9))))))
                      :up (sin (radians (* 10 (mod it 9))))})
                   (range 108))
              []))))

(defcomputed skyLocal "Each object's direction from where you stand: east, north and up."
  (map (fn [it]
         (if (or (= it.kind "cardinal") (= it.kind "grid") (= it.kind "land") (= it.kind "gridLabel"))
           it
           (merge it
                  {:e (* (- (sqrt (- 1 (* it.u it.u)))) (sin (- computed.skyEarth.sidereal it.t)))
                   :n (- (* computed.skyEarth.cosLatitude it.u)
                         (* computed.skyEarth.sinLatitude
                            (sqrt (- 1 (* it.u it.u)))
                            (cos (- computed.skyEarth.sidereal it.t))))
                   :up (+ (* computed.skyEarth.sinLatitude it.u)
                          (* computed.skyEarth.cosLatitude
                             (sqrt (- 1 (* it.u it.u)))
                             (cos (- computed.skyEarth.sidereal it.t))))})))
       computed.skyObjects))

(defcomputed skyDepth "Each object's depth along the view's direction."
  (map (fn [it]
         (merge it
                {:depth (+ (* (+ (* it.e computed.skyCamera.sinAzimuth) (* it.n computed.skyCamera.cosAzimuth))
                              computed.skyCamera.cosAltitude)
                           (* it.up computed.skyCamera.sinAltitude))
                 :across (- (* it.e computed.skyCamera.cosAzimuth) (* it.n computed.skyCamera.sinAzimuth))
                 :rise (- (* it.up computed.skyCamera.cosAltitude)
                          (* (+ (* it.e computed.skyCamera.sinAzimuth) (* it.n computed.skyCamera.cosAzimuth))
                             computed.skyCamera.sinAltitude))}))
       computed.skyLocal))

(defcomputed skyPlaced "Each object on the sky view."
  (map (fn [it]
         (merge it
                {:ahead (> it.depth 0.08)
                 :x (if (> it.depth 0.08)
                      (+ computed.skyCamera.cx (/ (* computed.skyCamera.f it.across) it.depth))
                      (- 1000))
                 :y (if (> it.depth 0.08)
                      (- computed.skyCamera.cy (/ (* computed.skyCamera.f it.rise) it.depth))
                      (- 1000))}))
       computed.skyDepth))

(defcomputed skyShown "The objects shown, in front and in the view."
  (filter (fn [it] (and it.ahead (> it.x (- 24)) (< it.x 484) (> it.y (- 24)) (< it.y (+ computed.skyHeight 24))))
          computed.skyPlaced))

(defcomputed skySun "The Sun, on the sky view." (find (fn [it] (= it.id "sun")) computed.skyPlaced))

(defcomputed daylight "How light the sky is, 0 at night to 1 by day."
  (if (and state.skyLayers.atmosphere computed.skySun)
    (* (pow (clamp (/ (+ computed.skySun.up 0.21) 0.31) 0 1) 1.6) (- 1 (* 0.92 computed.eclipseCover)))
    0))

(defcomputed skyLimit "The faintest stars shown, less by day." (- state.limitMag (* 9 computed.daylight)))

(defcomputed skyColor "The sky's colour, darker at night."
  (gradientColor {:kind "linear"
                  :stops [{:at 0 :color "#04060cff"} {:at 0.4 :color "#1b2a4cff"} {:at 1 :color "#5b8ed4ff"}]}
                 computed.daylight))

(defcomputed groundColor "The ground's colour, darker at night."
  (gradientColor {:kind "linear" :stops [{:at 0 :color "#14110cff"} {:at 1 :color "#7d6c49ff"}]} computed.daylight))

(defcomputed figureColor "The constellations' lines' colour."
  (gradientColor {:kind "linear" :stops [{:at 0 :color "#8f78b8ff"} {:at 1 :color "#8f78b850"}]} computed.daylight))

(defcomputed horizonY "Where the horizon is on the sky view."
  (+ computed.skyCamera.cy (* computed.skyCamera.f (tan computed.skyView.altitude))))

(defcomputed skyFigurePath "The constellations' lines, as a path."
  (if (and state.skyLayers.lines (> (count computed.skyPlaced) 1200))
    (join ""
          (map (fn [it]
                 (str "M"
                      (fixed (get-in computed.skyPlaced [it.0 :x]) 1)
                      " "
                      (fixed (get-in computed.skyPlaced [it.0 :y]) 1)
                      "L"
                      (fixed (get-in computed.skyPlaced [it.1 :x]) 1)
                      " "
                      (fixed (get-in computed.skyPlaced [it.1 :y]) 1)))
               (filter (fn [it]
                         (and (get-in computed.skyPlaced [it.0 :ahead]) (get-in computed.skyPlaced [it.1 :ahead])))
                       state.constellationLines)))
    ""))

(defcomputed skyGridPoints "The grid's points, on the sky view."
  (filter (fn [it] (= it.kind "grid")) computed.skyPlaced))

(defcomputed skyGridPath "The azimuthal grid, as a path."
  (join ""
        (map (fn [it]
               (str "M"
                    (fixed (get-in computed.skyGridPoints [it :x]) 1)
                    " "
                    (fixed (get-in computed.skyGridPoints [it :y]) 1)
                    "L"
                    (fixed (get-in computed.skyGridPoints [(+ it 1) :x]) 1)
                    " "
                    (fixed (get-in computed.skyGridPoints [(+ it 1) :y]) 1)))
             (filter (fn [it]
                       (and (= (get-in computed.skyGridPoints [it :line])
                               (get-in computed.skyGridPoints [(+ it 1) :line]))
                            (get-in computed.skyGridPoints [it :ahead])
                            (get-in computed.skyGridPoints [(+ it 1) :ahead])))
                     (range (max (- (count computed.skyGridPoints) 1) 0))))))

(defcomputed skySelected "The selected body, on the sky view."
  (find (fn [it] (and (= it.kind "body") (= it.id state.selected))) computed.skyPlaced))

(defcomputed skyInfo "What the sky view says of the selected body: where it is and when it rises."
  (if computed.skySelected
    [(t "sky.azAlt"
        {:az (fixed (mod (+ (degrees (atan2 computed.skySelected.e computed.skySelected.n)) 360) 360) 1)
         :alt (fixed (degrees (asin computed.skySelected.up)) 1)})
     (t "sky.raDec"
        {:ra (fixed (mod (+ (/ (degrees computed.skySelected.t) 15) 24) 24) 2)
         :dec (fixed (degrees (asin computed.skySelected.u)) 1)})]
    []))

(defcomputed skyBar "The sky view's bar: where you stand, the field of view and the day."
  [{:title (t "command.skyCentre")
    :icon "locate-fixed"
    :checked (and (not= state.skyTrack "") (= state.skyTrack state.selected))
    :run "skyCentre"}
   {:title (t "command.toggleSkyLines") :icon "waypoints" :checked state.skyLayers.lines :run "toggleSkyLines"}
   {:title (t "command.toggleSkyNames") :icon "baseline" :checked state.skyLayers.names :run "toggleSkyNames"}
   {:title (t "command.toggleSkyGrid") :icon "grid-3-x-3" :checked state.skyLayers.grid :run "toggleSkyGrid"}
   {:title (t "command.toggleSkyGround") :icon "mountain" :checked state.skyLayers.ground :run "toggleSkyGround"}
   {:title (t "command.toggleSkyCardinal")
    :icon "compass"
    :checked state.skyLayers.cardinal
    :run "toggleSkyCardinal"}
   {:title (t "command.toggleSkyAtmosphere")
    :icon "cloud-sun"
    :checked state.skyLayers.atmosphere
    :run "toggleSkyAtmosphere"}
   {:title (t "command.toggleSkyLabels") :icon "tag" :checked state.skyLayers.labels :run "toggleSkyLabels"}])

(defcomputed skyStarShapes "The stars as the sky view's shapes."
  (map (fn [it]
         {:tint it.tint
          :d (str "M"
                  (fixed (- it.x it.r) 1)
                  " "
                  (fixed it.y 1)
                  "a"
                  it.r
                  " "
                  it.r
                  " 0 1 0 "
                  (* 2 it.r)
                  " 0a"
                  it.r
                  " "
                  it.r
                  " 0 1 0 "
                  (* (- 2) it.r)
                  " 0")})
       (filter (fn [it] (and (= it.kind "star") (<= it.mag computed.skyLimit))) computed.skyShown)))

(defcomputed skyMoon "The Moon, on the sky view."
  (find (fn [it] (and (= it.kind "body") (= it.parent "earth"))) computed.skyPlaced))

(defcomputed eclipseNow "The eclipse happening now, if one is."
  (find (fn [it] (< (abs (- it.time state.time)) 0.3)) computed.eclipses))

(defcomputed eclipseCover "How much of the Sun the Moon covers now."
  (if (and computed.eclipseNow computed.eclipseNow.solar computed.skySun computed.skyMoon)
    (clamp (- 1
              (/ (acos (clamp (+ (* computed.skySun.e computed.skyMoon.e)
                                 (* computed.skySun.n computed.skyMoon.n)
                                 (* computed.skySun.up computed.skyMoon.up))
                              (- 1)
                              1))
                 0.025))
           0
           1)
    0))

(defcomputed skyLandPath "The ground's outline, as a path."
  (if (< (count (filter (fn [it] (and (= it.kind "land") it.ahead)) computed.skyPlaced)) 2)
    (if (< computed.horizonY computed.skyHeight)
      (str "M-10 "
           (fixed (max computed.horizonY (- 10)) 1)
           "L470 "
           (fixed (max computed.horizonY (- 10)) 1)
           "L470 "
           (+ computed.skyHeight 10)
           "L-10 "
           (+ computed.skyHeight 10)
           "Z")
      "")
    (str "M"
         (join "L"
               (map (fn [it] (str (fixed it.x 1) " " (fixed it.y 1)))
                    (sort-by (fn [it] it.x) (filter (fn [it] (and (= it.kind "land") it.ahead)) computed.skyPlaced))))
         "L"
         (fixed (max (get-in (sort-by (fn [it] (- it.x))
                                      (filter (fn [it] (and (= it.kind "land") it.ahead)) computed.skyPlaced))
                             [0 :x])
                     470)
                1)
         " "
         (+ computed.skyHeight 10)
         "L"
         (fixed (min (get-in (sort-by (fn [it] it.x)
                                      (filter (fn [it] (and (= it.kind "land") it.ahead)) computed.skyPlaced))
                             [0 :x])
                     (- 10))
                1)
         " "
         (+ computed.skyHeight 10)
         "Z")))

;;; The sky view

(defcomponent skyFromGround
  "The sky from where you stand on the Earth: the grid, the stars, the figures, the bodies, the ground."
  {}
  `(panel :fill true
     :color ~computed.skyColor
     :label ~(t "sky.label")
     :onPress skyPress
     :onDrag skyDrag
     :onScroll skyScroll
     ~@(sky-grid)
     (path#sky-figures :fill true :d ~computed.skyFigurePath :stroke ~computed.figureColor :strokeWidth 1)
     ~(star-path "sky-stars-blue" 0 "#c9d8ff")
     ~(star-path "sky-stars-white" 1 "#f4f4ff")
     ~(star-path "sky-stars-gold" 2 "#ffe2b8")
     ~(constellation-names)
     (path#sky-ground :visible ~state.skyLayers.ground
       :fill true
       :d ~computed.skyLandPath
       :fillColor ~computed.groundColor
       :stroke "#6e6347"
       :strokeWidth 1)
     ~(repeat [b (bodies-up) :key b.id] (sky-body b))
     ~(repeat [b (if state.skyLayers.labels (bodies-up) []) :key b.id] (sky-label b))
     ~(cardinal-points)
     ~(sky-bar)
     ~(sky-place)))

(defmacro bodies-up
  ;; A macro, not a function: the list it writes stays an expression, worked out at its speed.
  "The bodies on the sky view: those above the horizon, or all of them while the ground isn't drawn."
  []
  `(filter (fn [it] (and (= it.kind "body") (or (> it.up (- 0.01)) (not state.skyLayers.ground))))
           computed.skyShown))

(defview sky-grid "The azimuthal grid, and its labels but the four cardinal ones: two nodes, spliced in." []
  `[(path#sky-grid :fill true :d ~computed.skyGridPath :stroke "#5c5240" :strokeWidth 1)
    ~(repeat [g
              (filter (fn [it] (and (= it.kind "gridLabel") (< (indexOf ["0°" "90°" "180°" "270°"] it.text) 0)))
                      computed.skyShown)
              :key
              g.id]
       `(label :id ~(str "sky-grid-label-" g.id)
          :x ~(+ g.x 3)
          :y ~(- g.y 15)
          :height 14
          :fitText true
          :font small
          :text ~g.text
          :textColor "#a8976e"))])

(defview star-path "The stars of one tint, as one path of their shapes." [id tint colour]
  `(path :id ~id
     :fill true
     :d ~(join "" (map (fn [it] it.d) (filter (fn [it] (= it.tint tint)) computed.skyStarShapes)))
     :fillColor ~colour))

(defview constellation-names "The constellations' names, while they are shown." []
  (repeat [c (if state.skyLayers.names (filter (fn [it] (= it.kind "name")) computed.skyShown) []) :key c.id]
    `(label :id ~(str "sky-name-" c.id)
       :x ~(- c.x 60)
       :y ~(- c.y 8)
       :width 120
       :height 16
       :align center
       :font small
       :text ~c.name
       :textColor "#b49fd8")))

(defview sky-label "A body's name, beside its disc on the sky." [b]
  `(label :id ~(str "sky-label-" b.id)
     :x ~(+ b.x 10)
     :y ~(if (= b.parent "earth") (+ b.y 2) (- b.y 16))
     :height 16
     :fitText true
     :font small
     :text ~b.name
     :textColor ~(if (= b.id state.selected) "@accent" "#e9d9b0")))

(defview cardinal-points "The cardinal points on the horizon, while they are shown." []
  (repeat [p (if state.skyLayers.cardinal (filter (fn [it] (= it.kind "cardinal")) computed.skyShown) []) :key p.id]
    `(label :id ~(str "sky-cardinal-" p.id)
       :x ~(- p.x 10)
       :y ~(- p.y 24)
       :width 20
       :height 22
       :align center
       :font title
       :text ~p.id
       :textColor "#5eead4")))

(defview sky-bar "The sky's toolbar: centre on the selected body, and the layers it draws." []
  `(node#sky-bar :use toolbar
     :anchors {:right 8 :bottom 6}
     :with {:items ~computed.skyBar
            :width 272
            :label ~(t "sky.layers")
            :color "#0b1514d8"
            :textColor @onSky
            :dimColor @onSkyMuted
            :highlight "#1f3331"
            :checkedColor "#24514b"}))

(defview sky-place "Where you stand, the field of view and the day, over the sky's toolbar." []
  `(label#sky-place :anchors {:right 8 :bottom 40}
     :height 18
     :fitText true
     :font small
     :textColor @onSkyMuted
     :text ~(t "sky.place"
               {:latitude (text (abs state.sky.latitude))
                :side (t (if (< state.sky.latitude 0) "sky.south" "sky.north"))
                :fov (fixed (degrees state.sky.fov) 1)
                :clock computed.clock})))

(defview sky-body
  "A body on the sky: its disc (the Moon dark in a solar eclipse, red in a lunar one); a click selects it."
  [b]
  `(panel :id ~(str "sky-body-" b.id)
     :x ~(- b.x 12)
     :y ~(- b.y 12)
     :width 24
     :height 24
     :color ~(if (= b.id "sun") "#ffd76a30" "#00000000")
     :radius 12
     :role button
     :label ~b.name
     :onClick ~(handler chooseBody b.id)
     :borderWidth ~(if (= b.id state.selected) 1.5 0)
     :borderColor "#2dd4bf"
     (panel :id ~(str "sky-disc-" b.id)
       :x ~(if (or (= b.id "sun") (= b.parent "earth")) 5 9.5)
       :y ~(if (or (= b.id "sun") (= b.parent "earth")) 5 9.5)
       :width ~(if (or (= b.id "sun") (= b.parent "earth")) 14 5)
       :height ~(if (or (= b.id "sun") (= b.parent "earth")) 14 5)
       :radius 7
       :color ~(cond (and (= b.parent "earth") (> computed.eclipseCover 0)) "#16181c"
                     (and (= b.parent "earth") computed.eclipseNow (not computed.eclipseNow.solar)) "#9a4a2c"
                     :else b.hex)
       :borderColor ~(if (= b.id state.selected) "@accent" "#00000000")
       :borderWidth 1)))
