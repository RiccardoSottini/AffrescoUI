;;; time.lisp --- the day shown, how fast it runs, and the events along the way
;;;
;;; Time is in days from day 0 (1 January 2030); eclipses happen where the Moon crosses the Earth's orbit, and
;;; conjunctions and oppositions where two planets line up with the Sun.

;;; The day and the rate

(defcomputed clock
  (str (formatDate (+ (dateMs "2030-01-01") (* state.time 86400000)) "yyyy-MM-dd HH:mm:ss" "utc") " UTC"))

(defcomputed realTime "Whether time runs at real time."
  (< (abs (- (* (abs state.speed) 1728000) 1)) 0.01))

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

;;; Events

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

(defcomputed eclipseSpan "The longest run of those days."
  (max (concat [0] (map (fn [it] (+ (- it.to it.from) 1)) computed.eclipseOrbits))))

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

(defcomputed next
  (find (fn [it] (and (> it.time (+ state.time 0.5)) (< (indexOf state.muted it.kind) 0))) computed.events))

(defcomputed previous
  (get (sort-by (fn [it] it.time)
                  >
                  (filter (fn [it] (and (< it.time (- state.time 0.5)) (< (indexOf state.muted it.kind) 0)))
                          computed.events))
         0))

(defcomputed goal "The event time is running towards."
  (if (< state.speed 0) computed.previous computed.next))

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

;;; The timeline

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
