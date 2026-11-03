;;; time.lisp --- the day shown, how fast it runs, and the events along the way
;;;
;;; Time is in days from day 0 (1 January 2030); eclipses happen where the Moon crosses the Earth's orbit, and
;;; conjunctions and oppositions where two planets line up with the Sun.

;;; The day and the rate

(defcomputed clock "The day shown, as a date and a time in UTC."
  (t "time.clock"
     {:date (formatDate (+ (dateMs "2030-01-01") (* state.time 86400000)) "yyyy-MM-dd HH:mm:ss" "utc")}))

(defcomputed realTime "Whether time runs at real time." (< (abs (- (* (abs state.speed) 1728000) 1)) 0.01))

(defmacro speed-text
  ;; A macro: the text stays an expression, for the rate to say forwards or reversed.
  "How fast time runs: real time, or so many times faster, with how many days a second when that is a figure to read."
  []
  `(if computed.realTime
     (t "time.realTime")
     (t "time.timesFaster"
        {:times (formatNumber (* (abs state.speed) 1728000) 0 0)
         :perSecond (if (< (abs state.speed) 0.0005)
                      ""
                      (t "time.perSecond"
                         {:days (fixed (* (abs state.speed) 20) (if (< (abs state.speed) 0.05) 2 0))}))})))

(defcomputed rate "How fast time runs, said in words: paused, or the speed, reversed or not."
  (cond (not state.playing) (t "time.paused")
        (< state.speed 0) (t "time.reversed" {:speed (speed-text)})
        :else (speed-text)))

;;; Events

(defcomputed eclipseOrbits "The days on which the Moon crosses the Earth's orbit, within the time shown."
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

(defcomputed eclipses "The eclipses, solar and lunar, in order."
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

(defcomputed events "Every event, eclipses, conjunctions and oppositions, in order."
  (sort-by (fn [it] it.time)
           (concat state.events
                   (map (fn [it]
                          {:id it.id
                           :time it.time
                           :kind "eclipse"
                           :label (t (if it.solar "events.solarEclipse" "events.lunarEclipse"))})
                        computed.eclipses))))

(defcomputed next "The next event the timeline shows, not muted."
  (find (fn [it] (and (> it.time (+ state.time 0.5)) (< (indexOf state.muted it.kind) 0))) computed.events))

(defcomputed previous "The event before now, not muted."
  (get (sort-by (fn [it] it.time)
                >
                (filter (fn [it] (and (< it.time (- state.time 0.5)) (< (indexOf state.muted it.kind) 0)))
                        computed.events))
       0))

(defcomputed goal "The event time is running towards." (if (< state.speed 0) computed.previous computed.next))

(defcomputed eclipseRows "The eclipses, as the browser's list."
  (map (fn [it]
         {:id it.id
          :label (t (if it.solar "eclipse.solarRow" "eclipse.lunarRow")
                    {:date (formatDate (+ (dateMs "2030-01-01") (* (- it.time (/ it.hours 48)) 86400000))
                                       "yyyy-MM-dd HH:mm"
                                       "utc")
                     :hours (text (floor it.hours))
                     :minutes (text (round (* (mod it.hours 1) 60)))
                     :host it.hostName
                     :moon it.moonName})
          :icon (if it.solar "sun" "moon")})
       computed.eclipses))

(defcomputed tracks "The timeline's tracks: eclipses, conjunctions and oppositions, each with its events."
  [{:id "eclipses"
    :label (t "events.eclipses")
    :locked (>= (indexOf state.locked "eclipse") 0)
    :muted (>= (indexOf state.muted "eclipse") 0)
    :clips (map (fn [it] {:id it.id :label it.label :start (- it.time 2) :duration 4 :color "#7c3aed"})
                (filter (fn [it] (= it.kind "eclipse")) computed.events))}
   {:id "conjunctions"
    :label (t "events.conjunctions")
    :locked (>= (indexOf state.locked "conjunction") 0)
    :muted (>= (indexOf state.muted "conjunction") 0)
    :clips (map (fn [it] {:id it.id :label it.label :start (- it.time 2) :duration 4 :color "#be185d"})
                (filter (fn [it] (= it.kind "conjunction")) computed.events))}
   {:id "oppositions"
    :label (t "events.oppositions")
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
     :label ~(t "events.label")
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
