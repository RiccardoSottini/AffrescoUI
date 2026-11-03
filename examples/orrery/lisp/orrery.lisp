;;; orrery.lisp --- what Orrery does: the bodies, the notes, the camera's flights and the bookmarks
;;;
;;; The screen (index.screen.lisp) says what is shown and keeps the one-step actions (the toggles, the time controls);
;;; this file holds what has logic. A function reads the running app through paths (`state.bodies`, `computed.target`,
;;; `event.from`) and changes it only through steps (`set`, `push`, `animate`...). A handler runs one by name, as it
;;; would an action: `:onConnect reparent`, `~(handler goToEclipse)`.

;;; The bodies

(defn body "The body of an id, or nil." [id]
  (find (fn [b] (= (:id b) id)) state.bodies))

(defn at "Where a body is in the list, or -1." [id]
  (indexOf computed.ids id))

(defn selectBody"Selects body args.id: what the bodies dock's select gives." [args]
  (set "state.selected" (:id args)))

(defn reorderBody "Moves body args.id to the place of body args.onto." [args]
  (let [id (:id args)
        onto (:onto args)]
    (when (and (not= id onto) (contains computed.ids id) (contains computed.ids onto))
      (let [others (filter (fn [b] (not= (:id b) id)) state.bodies)
            place (at onto)]
        (set "state.bodies" (concat (take place others) [(body id)] (drop place others)))))))

(defn cloneBody "Adds a twin of body args.id on the same orbit, half a turn away; not of the Sun." [args]
  (let [id (:id args)
        twin (body id)
        named (str id "-" state.nextId)]
    (when (not= (or (:parent twin) "") "")
      (push "state.bodies"
            (merge twin {:id named
                         :name (str (:name twin) " " state.nextId)
                         :phase (rem (+ (:phase twin) 0.5) 1)
                         :look (or (:look twin) id)
                         :gx (+ (or (:gx twin) 400) 24)
                         :gy (+ (or (:gy twin) 20) 24)}))
      (set "state.selected" named)
      (increment "state.nextId" :by 1))))

(defn orbiting "A body made to orbit another: a body whose radius was set by its place keeps it in home." [b parent radius]
  (merge b {:parent parent :home (or (:home b) (:radius b)) :radius radius}))

(defn reparent "A link drawn in the graph: the body it reaches orbits the one it starts from, if no tree gets deeper than three."
  []
  (let [from event.from
        to event.to
        f (at from)
        t (at to)]
    (when (and (>= f 0) (>= t 0) (not= from to)
               (< (indexOf [(get computed.up1 f) (get computed.up2 f) (get computed.up3 f)] t) 0)
               (<= (+ (get computed.depth f) 1 (get computed.below t)) 3))
      (let [siblings (count (filter (fn [b] (= (:parent b) from)) state.bodies))]
        (set "state.bodies"
             (map (fn [b]
                    (if (= (:id b) to)
                      (orbiting b from (if (= from "sun") (or (:home b) (:radius b)) (+ 0.65 (* 0.35 siblings))))
                      b))
                  state.bodies))))))

(defn orbitAround "A link taken away in the graph: the body orbits the Sun again, where it was." []
  (let [to event.to]
    (when (and (> (at to) 0)
               (not= (or (:parent (body to)) "") "sun")
               (<= (+ 1 (get computed.below (at to))) 3))
      (set "state.bodies"
           (map (fn [b] (if (= (:id b) to) (merge b {:parent "sun" :radius (or (:home b) (:radius b))}) b))
                state.bodies)))))

(defn placeEach "Each item of a list that a move names (by id) gets what fields gives of that move." [items moves fields]
  (reduce (fn [all move] (map (fn [it] (if (= (:id it) (:id move)) (merge it (fields move)) it)) all)) items moves))

(defn moveGraphNodes "Nodes dragged in the graph keep their places." []
  (when (not (empty? event.nodes))
    (set "state.bodies" (placeEach state.bodies event.nodes (fn [n] {:gx (:x n) :gy (:y n)})))))

;;; The notes on the chart

(defn note "A note on the chart, in the notes' colours." [id x y width height text]
  {:id id :x x :y y :width width :height height :radius 4 :color "@note" :text text :textSize 10 :textColor "#1d2026"})

(defn addNote "A note with the draft's text, in the middle of the chart." []
  (push "state.notes"
        (note (str "note-" state.nextId)
              (round (- (+ state.chart.camera.x (/ 230 state.chart.camera.zoom)) 50))
              (round (- (+ state.chart.camera.y (/ 150 state.chart.camera.zoom)) 11))
              100 22
              (if (or (nil? state.draft) (= state.draft "")) "Note" state.draft)))
  (increment "state.nextId" :by 1)
  (set "state.draft" ""))

(defn markSelected "A ring around the selected body on the chart, and a note naming it and the day." []
  (let [p computed.selectedPosition]
    (when p
      (push "state.notes" {:id (str "mark-" state.nextId)
                           :x (round (- (* (:x p) 22) 16))
                           :y (round (- (* (:z p) 22) 16))
                           :width 32 :height 32 :radius 16
                           :color "transparent" :borderColor "@mark" :borderWidth 2 :text ""})
      (push "state.notes" (note (str "note-" state.nextId)
                                (round (+ (* (:x p) 22) 18))
                                (round (- (* (:z p) 22) 26))
                                130 20
                                (str (:name computed.selectedBody) ", day " (round state.time))))
      (increment "state.nextId" :by 1))))

(defn moveNotes "Notes moved or resized on the chart keep their places." []
  (when (not (empty? event.items))
    (set "state.notes" (placeEach state.notes event.items
                                  (fn [m] {:x (:x m) :y (:y m) :width (:width m) :height (:height m)})))))

;;; Time

(def slowest "The slowest speed, in days a frame: real time." 0.000000578704)

(defn nextEvent "Plays to the next event, or the previous one when time runs backwards." []
  (when computed.goal
    (set "state.playing" false)
    (animate "state.time" :to computed.goal.time :ms 600 :easing "inOut")))

(defn faster "Ten times faster, up to ten days a frame." []
  (set "state.speed" (if (< state.speed 0) (max (* state.speed 10) -10) (min (* state.speed 10) 10))))

(defn slower "Ten times slower, down to real time." []
  (set "state.speed" (if (< state.speed 0) (min (/ state.speed 10) (- slowest)) (max (/ state.speed 10) slowest))))

(defcommand realTime "Plays at real time, in the direction time runs." :title "Real time" :icon timer []
  (set "state.speed" (if (< state.speed 0) (- slowest) slowest))
  (set "state.playing" true))

;;; The camera

(defn viewDrag "Turns the camera as the view is dragged." []
  (when state.grab
    (set "state.camera" (merge state.camera {:yaw (- state.grab.yaw (* event.dx 0.008))
                                             :pitch (clamp (+ state.grab.pitch (* event.dy 0.006)) 0.05 1.5)}))))

(defn viewScroll "Moves the camera nearer or farther." []
  (set "state.camera" (merge state.camera {:distance (clamp (* state.camera.distance (+ 1 (* event.dy 0.06))) 0.5 40)}))
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

(defn land "Ends a flight, if no other started since: the camera looks at where it went, and follows it if asked." [follow]
  (increment "state.landed" :by 1)
  (when (and (= state.landed state.flights) computed.flyPosition)
    (set "state.camera.tx" computed.flyPosition.x)
    (set "state.camera.tz" computed.flyPosition.z)
    (set "state.fly" 0)
    (when follow (set "state.follow" state.flying))
    (set "state.flying" "")))

(defcommand goTo "Flies to the selected body, near it, and follows it." :title "Go to the selected body" :icon rocket []
  (when computed.selectedPosition
    (takeOff 900)
    (animate "state.camera.distance" :to (max (* computed.selectedBody.size 8) 1.5) :ms 900 :easing "inOut")
    (after 950 land true)))

(defcommand centre "Turns to the selected body, from where the camera is." :title "Centre the selected body" :icon crosshair
  []
  (when computed.selectedPosition
    (takeOff 500)
    (after 550 land false)))

(defcommand follow "Follows the selected body, or stops following it." :title "Follow the selected body" :icon locate-fixed
  []
  (cond (and (not= state.follow "") (= state.follow state.selected)) (hold)
        computed.selectedPosition (do (takeOff 500) (after 550 land true))))

(defcommand followSelected "Follows the selected body, if it isn't already." :title "Follow the selected body"
  :icon locate-fixed
  []
  (when (and (not= state.selected "") (not= state.follow state.selected))
    (follow)))

(defcommand toggleMarker "Marks the selected body in the view, or takes its mark away." :title "Mark the selected body"
  :icon crosshair
  []
  (if (or (>= (indexOf state.markers state.selected) 0) (< (at state.selected) 0))
    (set "state.markers" (filter (fn [m] (not= m state.selected)) state.markers))
    (push "state.markers" state.selected)))

;;; Places and eclipses

(defn bookmark "The bookmark of an id, or nil." [id]
  (find (fn [b] (= (:id b) id)) state.bookmarks))

(defcommand addBookmark "Keeps where the camera is, what it follows, the day and the zoom as a place." :title "Bookmark this view"
  :icon bookmark-plus
  []
  (set "state.bookmarks"
       (conj state.bookmarks {:id (str "place-" state.nextPlace)
                              :label (str (if computed.selectedBody (:name computed.selectedBody) "Space") ", day "
                                          (floor state.time))
                              :camera (merge state.camera {:tx computed.target.x :tz computed.target.z})
                              :follow state.follow
                              :selected state.selected
                              :time state.time
                              :zoom state.zoom}))
  (set "state.bookmark" (str "place-" state.nextPlace))
  (increment "state.nextPlace" :by 1)
  (set "state.browserTab" "places")
  (set "state.showBrowser" true))

(defn goToBookmark "Goes back to the place picked in the list." []
  (let [place (bookmark event.value)]
    (when place
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
      (animate "state.camera.distance" :to state.camera.distance :ms 1))))

(defcommand deleteBookmark "Deletes the bookmark picked; the last one left is picked." :title "Delete the bookmark" :icon trash-2
  []
  (when (bookmark state.bookmark)
    (set "state.bookmarks" (filter (fn [b] (not= (:id b) state.bookmark)) state.bookmarks))
    (set "state.bookmark" (if (> (count state.bookmarks) 0) (:id (last state.bookmarks)) ""))))

(defn goToEclipse "Goes to the eclipse picked in the finder: its planet followed, the sky on what is hidden." []
  (let [eclipse (find (fn [e] (= (:id e) event.value)) computed.eclipses)]
    (when eclipse
      (set "state.eclipse" event.value)
      (set "state.playing" false)
      (set "state.selected" (:host eclipse))
      (when (not= state.follow state.selected) (follow))
      (set "state.skyTrack" (if (:solar eclipse) "sun" (:moon eclipse)))
      (animate "state.time" :to (:time eclipse) :ms 600 :easing "inOut"))))
