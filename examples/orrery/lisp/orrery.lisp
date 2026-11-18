;;; orrery.lisp --- what Orrery does: its commands and functions
;;;
;;; The views (view.lisp, chart.lisp, browser.lisp...) say what is shown; this file says what happens: the bodies,
;;; the notes, time, the camera's flights, the bookmarks, and what the views' toggles and lists choose. A function reads
;;; the running app through paths (`state.bodies`, `computed.target`, `event.from`) and changes it only through steps
;;; (`set`, `push`, `animate`...). A handler runs one by name (`:onConnect reparent`) or with its arguments
;;; (`~(handler chooseBody event.value)`); keys.lisp binds the keys.

(ns orrery.orrery)

;;; The bodies

(defn body "The body of an id, or nil." [id] (find #(= (:id %) id) state.bodies))

(defn at "Where a body is in the list, or -1." [id] (indexOf computed.ids id))

(defn selectBody "Selects body args.id: what the bodies dock's select gives." [args]
  (set "state.selected" (:id args)))

(defn reorderBody "Moves body args.id to the place of body args.onto." [args]
  (let [{:keys [id onto]} args known (into #{} computed.ids)]
    (when (and (not= id onto) (known id) (known onto))
      (let [others (remove #(= (:id %) id) state.bodies) place (at onto)]
        (set "state.bodies" (concat (take place others) [(body id)] (drop place others)))))))

(defn cloneBody "Adds a twin of body args.id on the same orbit, half a turn away; not of the Sun." [args]
  (let [id (:id args) twin (body id) named (str id "-" state.nextId)]
    (when-not (empty? (:parent twin))
      (push "state.bodies"
            (merge twin
                   {:id named
                    :name (str (:name twin) " " state.nextId)
                    :phase (rem (+ (:phase twin) 0.5) 1)
                    :look (or (:look twin) id)
                    :gx (-> (:gx twin) (or 400) (+ 24))
                    :gy (-> (:gy twin) (or 20) (+ 24))}))
      (set "state.selected" named)
      (increment "state.nextId" :by 1))))

(defn orbiting "A body made to orbit another: a body whose radius was set by its place keeps it in home."
  [b parent radius]
  (merge b {:parent parent :home (or (:home b) (:radius b)) :radius radius}))

(defn reparent
  "A link drawn in the graph: the body it reaches orbits the one it starts from, if no tree gets deeper than three."
  []
  (let [from event.from to event.to f (at from) t (at to)]
    (when (and (>= f 0)
               (>= t 0)
               (not= from to)
               (not-any? #{t} [(get computed.up1 f) (get computed.up2 f) (get computed.up3 f)])
               (<= (+ (get computed.depth f) 1 (get computed.below t)) 3))
      (let [siblings (count (filter #(= (:parent %) from) state.bodies))]
        (set "state.bodies"
             (map (fn [b]
                    (if (= (:id b) to)
                      (orbiting b
                                from
                                (if (= from "sun") (or (:home b) (:radius b)) (-> siblings (* 0.35) (+ 0.65))))
                      b))
                  state.bodies))))))

(defn orbitAround "A link taken away in the graph: the body orbits the Sun again, where it was." []
  (let [to event.to
        place (at to)]
    (when (and (pos? place)
               (not= (:parent (body to)) "sun")
               (<= (inc (get computed.below place)) 3))
      (set "state.bodies"
           (map #(if (= (:id %) to) (merge % {:parent "sun" :radius (or (:home %) (:radius %))}) %)
                state.bodies)))))

(defn placeEach "Each item of a list that a move names (by id) gets what fields gives of that move."
  [items moves fields]
  (reduce (fn [all move] (map #(if (= (:id %) (:id move)) (merge % (fields move)) %) all)) items moves))

(defn moveGraphNodes "Nodes dragged in the graph keep their places." []
  (when (seq event.nodes)
    (set "state.bodies" (placeEach state.bodies event.nodes (fn [{:keys [x y]}] {:gx x :gy y})))))

;;; The notes on the chart

(defn note "A note on the chart, in the notes' colours." [id x y width height text]
  {:id id
   :x x
   :y y
   :width width
   :height height
   :radius 4
   :color "@note"
   :text text
   :textSize 10
   :textColor "#1d2026"})

(defn addNote "A note with the draft's text, in the middle of the chart." []
  (let [{:keys [x y zoom]} state.chart.camera]
    (push "state.notes"
          (note (str "note-" state.nextId)
                (-> (/ 230 zoom) (+ x) (- 50) round)
                (-> (/ 150 zoom) (+ y) (- 11) round)
                100
                22
                (or (not-empty state.draft) "Note"))))
  (increment "state.nextId" :by 1)
  (set "state.draft" ""))

(defn markSelected "A ring around the selected body on the chart, and a note naming it and the day." []
  (when-some [{:keys [x z]} computed.selectedPosition]
    (push "state.notes"
            {:id (str "mark-" state.nextId)
             :x (round (- (* x 22) 16))
             :y (round (- (* z 22) 16))
             :width 32
             :height 32
             :radius 16
             :color "transparent"
             :borderColor "@mark"
             :borderWidth 2
             :text ""})
    (push "state.notes"
            (note (str "note-" state.nextId)
                  (round (+ (* x 22) 18))
                  (round (- (* z 22) 26))
                  130
                  20
                  (str (:name computed.selectedBody) ", day " (round state.time))))
    (increment "state.nextId" :by 1)))

(defn moveNotes "Notes moved or resized on the chart keep their places." []
  (when (seq event.items)
    (set "state.notes"
         (placeEach state.notes
                    event.items
                    (fn [{:keys [x y width height]}] {:x x :y y :width width :height height})))))

(defn deleteNotes "Deletes the notes chosen on the chart." []
  (let [chosen (into #{} state.chart.selection)]
    (set "state.notes" (remove #(chosen (:id %)) state.notes))))

(defn clearNotes "Deletes every note on the chart." [] (set "state.notes" []))

;;; Time

(def slowest "The slowest speed, in days a frame: real time." 0.000000578704)

(defn nextEvent "Plays to the next event, or the previous one when time runs backwards." []
  (when-some [goal computed.goal]
    (set "state.playing" false)
    (animate "state.time" :to (:time goal) :ms 600 :easing "inOut")))

(defn faster "Ten times faster, up to ten days a frame." []
  (let [speed (* state.speed 10)]
    (set "state.speed" (if (neg? state.speed) (max speed -10) (min speed 10)))))

(defn slower "Ten times slower, down to real time." []
  (let [speed (/ state.speed 10)]
    (set "state.speed"
         (if (neg? state.speed) (min speed (- slowest)) (max speed slowest)))))

(defcommand realTime "Plays at real time, in the direction time runs."
  :title ~(t "command.realTime")
  :description ~(t "command.realTime.about")
  :icon timer
  []
  (set "state.speed" (if (neg? state.speed) (- slowest) slowest))
  (set "state.playing" true))

(defn togglePlay "Plays the days, or pauses them." [] (toggle "state.playing"))

(defn rewind "Pauses and goes back to day 0." [] (set "state.playing" false) (set "state.time" 0))

(defn tick "Every 50 ms while playing: time moves on at its rate, and stops at either end." []
  (when state.playing
    (let [time (clamp (+ state.time state.speed) 0 state.duration)]
      (set "state.time" time)
      (when (or (and (neg? state.speed) (<= time 0)) (and (pos? state.speed) (>= time state.duration)))
        (set "state.playing" false)))))

(defn reverse "Runs time the other way." [] (set "state.speed" (- state.speed)))

(defn dayZero
  ;; Not `now`: a function of the program named so would be what (now) calls everywhere, the add-ons' too.
  "Back to day 0, at the starting rate."
  []
  (set "state.time" 0)
  (set "state.speed" 0.01))

;;; The camera

(defn viewDrag "Turns the camera as the view is dragged." []
  (when-some [grab state.grab]
    (set "state.camera"
         (assoc state.camera
                :yaw (- (:yaw grab) (* event.dx 0.008))
                :pitch (clamp (+ (:pitch grab) (* event.dy 0.006)) 0.05 1.5)))))

(defn viewScroll "Moves the camera nearer or farther." []
  (set "state.camera"
       (assoc state.camera :distance (-> event.dy (* 0.06) inc (* state.camera.distance) (clamp 0.5 40))))
  (animate "state.camera.distance" :to state.camera.distance :ms 1))

(defn hold "Stops where the camera looks now: no flight, nothing followed." []
  (set "state.camera.tx" computed.target.x)
  (set "state.camera.tz" computed.target.z)
  (set "state.follow" "")
  (set "state.flying" "")
  (set "state.fly" 0))

(defn takeOff "Starts a flight to the selected body, of ms milliseconds." [ms]
  (hold)
  (set "state.flying" state.selected)
  (increment "state.flights" :by 1)
  (animate "state.fly" :to 1 :ms ms :easing "inOut"))

(defn land "Ends a flight, if no other started since: the camera looks at where it went, and follows it if asked."
  [follow]
  (increment "state.landed" :by 1)
  (when (and (= state.landed state.flights) (some? computed.flyPosition))
    (set "state.camera.tx" computed.flyPosition.x)
    (set "state.camera.tz" computed.flyPosition.z)
    (set "state.fly" 0)
    (when follow (set "state.follow" state.flying))
    (set "state.flying" "")))

(defcommand goTo "Flies to the selected body, near it, and follows it."
  :title ~(t "command.goTo")
  :description ~(t "command.goTo.about")
  :icon rocket
  []
  (when-some [_ computed.selectedPosition]
    (takeOff 900)
    (animate "state.camera.distance" :to (max (* computed.selectedBody.size 8) 1.5) :ms 900 :easing "inOut")
    (after 950 land true)))

(defcommand centre "Turns to the selected body, from where the camera is."
  :title ~(t "command.centre")
  :description ~(t "command.centre.about")
  :icon crosshair
  []
  (when-some [_ computed.selectedPosition] (takeOff 500) (after 550 land false)))

(defcommand follow "Follows the selected body, or stops following it."
  :title ~(t "command.follow")
  :description ~(t "command.follow.about")
  :icon locate-fixed
  []
  (cond (and (not= state.follow "") (= state.follow state.selected)) (hold)
        (some? computed.selectedPosition) (do (takeOff 500) (after 550 land true))))

(defcommand followSelected "Follows the selected body, if it isn't already."
  :title ~(t "command.followSelected")
  :description ~(t "command.followSelected.about")
  :icon locate-fixed
  []
  (when (and (not= state.selected "") (not= state.follow state.selected)) (follow)))

(defcommand toggleMarker "Marks the selected body in the view, or takes its mark away."
  :title ~(t "command.toggleMarker")
  :description ~(t "command.toggleMarker.about")
  :icon crosshair
  []
  (if (or (some #{state.selected} state.markers) (neg? (at state.selected)))
    (set "state.markers" (remove #{state.selected} state.markers))
    (push "state.markers" state.selected)))

;;; Places and eclipses

(defn bookmark "The bookmark of an id, or nil." [id] (find #(= (:id %) id) state.bookmarks))

(defcommand addBookmark "Keeps where the camera is, what it follows, the day and the zoom as a place."
  :title ~(t "command.addBookmark")
  :description ~(t "command.addBookmark.about")
  :icon bookmark-plus
  []
  (set "state.bookmarks"
       (conj state.bookmarks
             {:id (str "place-" state.nextPlace)
              :label (str (or (:name computed.selectedBody) "Space") ", day " (floor state.time))
              :camera (assoc state.camera :tx computed.target.x :tz computed.target.z)
              :follow state.follow
              :selected state.selected
              :time state.time
              :zoom state.zoom}))
  (set "state.bookmark" (str "place-" state.nextPlace))
  (increment "state.nextPlace" :by 1)
  (set "state.browserTab" :places)
  (set "state.showBrowser" true))

(defn goToBookmark "Goes back to the place picked in the list." []
  (when-some [place (bookmark event.value)]
    (increment "state.flights" :by 1)
    (animate "state.fly" :to 0 :ms 1)
    (hold)
    (set "state.bookmark" event.value)
    (set "state.playing" false)
    (set "state.selected" (:selected place))
    (set "state.camera" (:camera place))
    (set "state.time" (or (:time place) state.time))
    (set "state.zoom" (or (:zoom place) state.zoom))
    (set "state.follow" (:follow place))
    (animate "state.camera.distance" :to state.camera.distance :ms 1)))

(defcommand deleteBookmark "Deletes the bookmark picked; the last one left is picked."
  :title ~(t "command.deleteBookmark")
  :description ~(t "command.deleteBookmark.about")
  :icon trash-2
  []
  (when-some [_ (bookmark state.bookmark)]
    (set "state.bookmarks" (remove #(= (:id %) state.bookmark) state.bookmarks))
    (set "state.bookmark" (or (some-> state.bookmarks last :id) ""))))

(defn goToEclipse "Goes to the eclipse picked in the finder: its planet followed, the sky on what is hidden." []
  (when-some [{:keys [host solar moon time]} (find #(= (:id %) event.value) computed.eclipses)]
    (set "state.eclipse" event.value)
    (set "state.playing" false)
    (set "state.selected" host)
    (when (not= state.follow state.selected) (follow))
    (set "state.skyTrack" (if solar "sun" moon))
    (animate "state.time" :to time :ms 600 :easing "inOut")))

;;; The 3D view

(defn viewPress "Holds the camera where it is, as a drag starts." [] (set "state.grab" state.camera))

(defn viewRelease "Lets go of the camera, as a drag ends." [] (set "state.grab" nil))

(defn syncScene "Gives the scene the camera worked out from the state." []
  (set "state.sceneView" computed.sceneView))

(defpart scene "The scene follows the camera, the day, the selection and the bodies, from the start."
  :watch {:state.camera syncScene
          :state.zoom syncScene
          :state.follow syncScene
          :state.fly syncScene
          :state.time syncScene
          :state.selected syncScene
          :state.bodies syncScene}
  :onLoad syncScene)

;;; What the views show

(defcommand toggleOrbits "Shows or hides the orbits in the 3D view."
  :title ~(t "command.toggleOrbits")
  :description ~(t "command.toggleOrbits.about")
  :icon "orbit"
  []
  (toggle "state.showOrbits"))

(defcommand toggleLabels "Shows or hides the bodies' names in the 3D view."
  :title ~(t "command.toggleLabels")
  :description ~(t "command.toggleLabels.about")
  :icon "tag"
  []
  (toggle "state.showLabels"))

(defcommand toggleStars "Shows or hides the background stars in the 3D view."
  :title ~(t "command.toggleStars")
  :description ~(t "command.toggleStars.about")
  :icon "sparkles"
  []
  (toggle "state.showStars"))

(defcommand toggleShading "Shows each body's lit side, or lights it all."
  :title ~(t "command.toggleShading")
  :description ~(t "command.toggleShading.about")
  :icon "moon"
  []
  (toggle "state.showShading"))

(defcommand toggleGlow "Shows or hides the glow around the Sun."
  :title ~(t "command.toggleGlow")
  :description ~(t "command.toggleGlow.about")
  :icon "sun"
  []
  (toggle "state.showGlow"))

(defcommand toggleTheme "Switches between the dark and the light theme."
  :title ~(t "command.toggleTheme")
  :description ~(t "command.toggleTheme.about")
  :icon "sun-moon"
  []
  (set "state.theme" (if (= state.theme :dark) :light :dark)))

(defcommand openRender "Opens the options of what the 3D view draws."
  :title ~(t "command.openRender")
  :description ~(t "command.openRender.about")
  :icon "sliders-horizontal"
  []
  (set "state.renderOpen" true))

(defcommand zoomIn "Narrows the 3D view's field of view, as a telescope does."
  :title ~(t "command.zoomIn")
  :description ~(t "command.zoomIn.about")
  :icon "zoom-in"
  []
  (set "state.zoom" (min (* state.zoom 1.25) 20)))

(defcommand zoomOut "Widens the 3D view's field of view."
  :title ~(t "command.zoomOut")
  :description ~(t "command.zoomOut.about")
  :icon "zoom-out"
  []
  (set "state.zoom" (max (/ state.zoom 1.25) 0.5)))

(defcommand fainter "Shows fainter stars, half a magnitude more."
  :title ~(t "command.fainter")
  :description ~(t "command.fainter.about")
  :icon "sparkle"
  []
  (set "state.limitMag" (min (+ state.limitMag 0.5) 6.5)))

(defcommand brighter "Shows only brighter stars, half a magnitude less."
  :title ~(t "command.brighter")
  :description ~(t "command.brighter.about")
  :icon "star"
  []
  (set "state.limitMag" (max (- state.limitMag 0.5) 1)))

(defcommand toggleBrowser "Shows or hides the browser over the view."
  :title ~(t "command.toggleBrowser")
  :description ~(t "command.toggleBrowser.about")
  :icon "list-tree"
  []
  (toggle "state.showBrowser"))

(defcommand openEclipses "Opens the browser at the eclipses."
  :title ~(t "command.openEclipses")
  :description ~(t "command.openEclipses.about")
  :icon "moon"
  []
  (set "state.showBrowser" true)
  (set "state.browserTab" :eclipses))

;;; The sky from the ground

(defcommand toggleSkyLines "Shows or hides the constellations' lines."
  :title ~(t "command.toggleSkyLines")
  :description ~(t "command.toggleSkyLines.about")
  []
  (toggle "state.skyLayers.lines"))

(defcommand toggleSkyNames "Shows or hides the constellations' names."
  :title ~(t "command.toggleSkyNames")
  :description ~(t "command.toggleSkyNames.about")
  []
  (toggle "state.skyLayers.names"))

(defcommand toggleSkyGrid "Shows or hides the azimuthal grid."
  :title ~(t "command.toggleSkyGrid")
  :description ~(t "command.toggleSkyGrid.about")
  []
  (toggle "state.skyLayers.grid"))

(defcommand toggleSkyGround "Shows or hides the ground."
  :title ~(t "command.toggleSkyGround")
  :description ~(t "command.toggleSkyGround.about")
  []
  (toggle "state.skyLayers.ground"))

(defcommand toggleSkyCardinal "Shows or hides the cardinal points."
  :title ~(t "command.toggleSkyCardinal")
  :description ~(t "command.toggleSkyCardinal.about")
  []
  (toggle "state.skyLayers.cardinal"))

(defcommand toggleSkyAtmosphere "Shows the sky's daylight, or a night sky all day."
  :title ~(t "command.toggleSkyAtmosphere")
  :description ~(t "command.toggleSkyAtmosphere.about")
  []
  (toggle "state.skyLayers.atmosphere"))

(defcommand toggleSkyLabels "Shows or hides the planets' names."
  :title ~(t "command.toggleSkyLabels")
  :description ~(t "command.toggleSkyLabels.about")
  []
  (toggle "state.skyLayers.labels"))

(defcommand skyGround "Shows the sky from the ground, where you stand on the Earth."
  :title ~(t "command.skyGround")
  :description ~(t "command.skyGround.about")
  []
  (set "state.skyMode" :ground))

(defcommand skyAbove "Shows the chart of the system from above."
  :title ~(t "command.skyAbove")
  :description ~(t "command.skyAbove.about")
  []
  (set "state.skyMode" :above))

(defcommand skyCentre "Keeps the sky view on the selected body, or lets it go."
  :title ~(t "command.skyCentre")
  :description ~(t "command.skyCentre.about")
  []
  (set "state.skyTrack" (if (#{state.skyTrack "earth"} state.selected) "" state.selected)))

(defn skyPress "Holds the sky view where it is, as a drag starts, and stops tracking." []
  (set "state.sky" computed.skyView)
  (set "state.skyTrack" "")
  (set "state.skyGrab" computed.skyView))

(defn skyDrag "Turns the sky view as the pointer drags it." [event]
  (when-some [grab state.skyGrab]
    (let [fov state.sky.fov]
      (set "state.sky"
           (assoc state.sky
                  :azimuth (- (:azimuth grab) (/ (* event.dx fov) 460))
                  :altitude (-> event.dy (* fov) (/ 460) (+ (:altitude grab)) (clamp (- 0.3) 1.45)))))))

(defn skyScroll "Zooms the sky view as the wheel turns." [event]
  (set "state.sky.fov" (-> event.dy (* 0.06) inc (* state.sky.fov) (clamp 0.09 1.75))))

;;; What the views choose

(defn chooseBody "Selects a body, by its id." [id] (set "state.selected" id))

(defn flyToBody "Selects a body and flies to it." [id] (chooseBody id) (goTo))

(defn expandBody "Unfolds a body's satellites in the browser's tree, or folds them." [id expanded]
  (let [others (remove #{id} state.expandedBodies)]
    (set "state.expandedBodies" (if expanded (concat others [id]) others))))

(defn pickInScene "Selects the body clicked in the 3D view; the Sun's halo is the Sun." [button id]
  (let [body (if (= id "halo") "sun" (or id ""))]
    (when (and (= button :left) (some #{body} computed.ids)) (chooseBody body))))

(defn pickInGraph "Selects the first body chosen in the orbit graph." [ids]
  (when-first [id ids] (chooseBody id)))

(defn toggleRender "Turns one thing the 3D view draws on or off, by its id in the render options." [id]
  (case id
    :orbits (toggleOrbits)
    :labels (toggleLabels)
    :stars (toggleStars)
    :shading (toggleShading)
    :glow (toggleGlow)
    nil))

(defn kindOf "A track's kind of event: its id without the plural's s." [track]
  (slice track 0 (dec (count track))))

(defn trackChanged "Mutes or locks a kind of event, as its track on the timeline says." [event]
  (when (= event.type :track)
    (let [kind (kindOf event.id) muted (remove #{kind} state.muted) locked (remove #{kind} state.locked)]
      (set "state.muted" (if event.muted (concat muted [kind]) muted))
      (set "state.locked" (if event.locked (concat locked [kind]) locked)))))

(defn scrubTo "Pauses at the day the timeline is scrubbed to." [time]
  (set "state.playing" false)
  (set "state.time" time))

(defn goToEvent "Pauses at the day of the event chosen on the timeline." [id]
  (when-some [event (find #(= (:id %) id) computed.events)]
    (set "state.playing" false)
    (set "state.time" (:time event))))
