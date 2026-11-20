;;; computed.lisp --- what Streak works out: the theme and its palette, the layout, the days shown, each habit's
;;; entries, scores and streaks, the selected habit's charts and goals, the sync's state, and the CSV

(ns streak.computed)

;;; Values
(defcomputed theme "The theme shown: light, dark or black, the system's when it is system."
  (if (= (if (= state.theme :system) window.colorScheme state.theme) :dark)
      (if state.pureBlack "black" "dark")
      "light"))

(defcomputed pal "The habits' twenty colours, for the theme."
  (if (= computed.theme "light")
      ["#c8443a"
       "#d4602a"
       "#d9822b"
       "#cf921c"
       "#b49a16"
       "#8a9a1c"
       "#5c9e3a"
       "#2f8a55"
       "#17877a"
       "#1592a6"
       "#2a7fc0"
       "#3366b8"
       "#4a4fa8"
       "#7048b0"
       "#9446a8"
       "#c23d78"
       "#7a5a44"
       "#3a3a3a"
       "#7a7a7a"
       "#a8a8a8"]
      ["#e88a80"
       "#f09a72"
       "#f5b56e"
       "#f5cc66"
       "#e8dc72"
       "#cfdc78"
       "#a8d888"
       "#86cfa0"
       "#6cc8bc"
       "#6ad0e0"
       "#7cc0f0"
       "#8aaaf0"
       "#9a9ef0"
       "#b39af0"
       "#cc94e0"
       "#f08ab4"
       "#b8977f"
       "#e2e2e2"
       "#a2a2a2"
       "#c2c2c2"]))

(defcomputed onColor "The colour of a tick on a habit's colour." (if (= computed.theme "light") "#ffffff" "#1b1b1b"))

(defcomputed reminderHour "The hour of the reminder's time being edited, as a number."
  (or (parse-long
        (first (str/split state.edit.time ":")))
      0))

(defcomputed reminderMinute "The minute of the reminder's time being edited, as a number."
  (or (parse-long
        (second (str/split state.edit.time ":")))
      0))

(defcomputed wide "Whether the list and the detail show side by side." (>= window.width 960))

(defcomputed picking "Whether rows are picked." (pos? (count state.picked)))

(defcomputed pickedArchived "Whether every row picked is archived." (every? #(or (not (contains state.picked (:id %))) (:archived %)) data.ordered))

(defcomputed listWidth "How wide the list is." (if computed.wide (clamp (floor (* window.width 0.46)) 460 640) window.width))

(defcomputed detailWidth "How wide the detail is." (if computed.wide (dec (- window.width computed.listWidth)) window.width))

(defcomputed cardWidth "How wide the detail's cards are." (min 760 (- computed.detailWidth 32)))

(defcomputed cols "How many days a row shows, from 3 to 14." (clamp (floor (/ (- computed.listWidth 228) 44)) 3 14))

(defcomputed days "The days a row shows, today last (or first, reversed)."
  (cond->> (map #(addDays state.today (- %)) (range computed.cols)) state.reverseDays reverse))

(defcomputed weekStart "The first day of this week." (addDays state.today (- (mod (+ (- (weekday state.today) state.firstWeekday) 7) 7))))

(defcomputed weekdays "The weekdays from the first one." (map #(mod (+ state.firstWeekday %) 7) (range 7)))

(defcomputed note "How the next values work out each habit's history."
  "Per habit, from its first day to today: the entries (1 yes, 0 no, -1 skipped, or the number), their running total, the share of the target met over the frequency's window, and the score.")

(defcomputed parsed "Each habit's entries, as numbers."
  (map (fn [it]
         (cond (= it.entries "") []
               (= it.kind :measurable) (map #(or-else (number %) 0) (split it.entries " "))
               :else (map #(case % :y 1 :s -1 0) (split it.entries ""))))
       data.ordered))

(defcomputed vals "Each habit's entries from its first day to today, a day missing a 0."
  (map-indexed (fn [index it]
                 (slice (concat (get computed.parsed index)
                                ; style: keep, a binding has no constantly
                                (map (fn [it] 0)
                                     (range (max 0
                                                 (- (inc (daysBetween it.from state.today))
                                                    (count (get computed.parsed index)))))))
                        0
                        (max 0 (inc (daysBetween it.from state.today)))))
               data.ordered))

(defcomputed prefix "Each habit's running totals." (map #(scan % (fn [acc it] (+ acc (max it 0))) 0) computed.vals))

(defcomputed ratio "Each habit's days, how far each went towards its frequency."
  (map-indexed (fn [index it]
                 (scan (get computed.prefix index)
                       (fn [acc it]
                         {:h acc.h
                          :w acc.w
                          :need acc.need
                          :most acc.most
                          :r (if acc.most
                                 (if (<= (- it
                                            (if (>= index acc.w) (get-in computed.prefix [acc.h (- index acc.w)]) 0))
                                         acc.need)
                                     1
                                     (max 0
                                          (- 2
                                             (/ (- it
                                                   (if (>= index acc.w)
                                                       (get-in computed.prefix [acc.h (- index acc.w)])
                                                       0))
                                                acc.need))))
                                 (min 1
                                      (/ (- it
                                            (if (>= index acc.w) (get-in computed.prefix [acc.h (- index acc.w)]) 0))
                                         acc.need)))})
                       {:h index
                        :w (max 1 it.frequency.days)
                        :need (max 0.001
                                   (if (= it.kind :measurable) (or-else (number it.target) 1) it.frequency.times))
                        :most (and (= it.kind :measurable) (= it.targetType :atMost))}))
               data.ordered))

(defcomputed score "Each habit's score, day by day."
  (map-indexed (fn [index it]
                 (scan (get computed.ratio index)
                       (fn [acc it]
                         {:h acc.h
                          :m acc.m
                          :s (if (= (get-in computed.vals [acc.h index]) -1)
                                 acc.s
                                 (+ (* acc.s acc.m) (* it.r (- 1 acc.m))))})
                       {:h index
                        :m (pow 0.5
                                (/ (sqrt (if (= it.kind :measurable)
                                             (/ 1 (max 1 it.frequency.days))
                                             (/ it.frequency.times (max 1 it.frequency.days))))
                                   13))
                        :s 0}))
               data.ordered))

(defcomputed now "Each habit's score today." (map #(if (pos? (count %)) (get-in % [(dec (count %)) :s]) 0) computed.score))

(defcomputed done "Whether each habit is done today."
  (map #(and (pos? (count %)) (>= (get-in % [(dec (count %)) :r]) 1)) computed.ratio))

(defcomputed visible "The habits shown: not archived, not done today, as the filter says."
  (filter (fn [it]
            (and (or state.showArchived (not (get-in data.ordered [it :archived])))
                 (or (not state.hideCompleted) (not (get computed.done it)))))
          (range (count data.ordered))))

(defcomputed order "The habits shown, in the order the sort says."
  (case state.sort
    :name (sort-by #(lower-case (get-in data.ordered [% :name])) computed.visible)
    :color (sort-by #(get-in data.ordered [% :color]) computed.visible)
    :score (sort-by #(- (get computed.now %)) computed.visible)
    :status (sort-by #(if (get computed.done %) 1 0) computed.visible)
    computed.visible))

(defcomputed sel "Where the selected habit is, or -1." (indexOf (map :id data.ordered) state.selected))

(defcomputed h "The selected habit." (get data.ordered computed.sel))

(defcomputed col "The selected habit's colour." (if (>= computed.sel 0) (get computed.pal computed.h.color) "#757575"))

(defcomputed accent "The colour the dialog or the detail is drawn in." (if state.edit.open (get computed.pal state.edit.color) computed.col))

(defcomputed n "How many days the selected habit has." (if (>= computed.sel 0) (count (get computed.vals computed.sel)) 0))

(defcomputed v "The selected habit's entries." (if (>= computed.sel 0) (get computed.vals computed.sel) []))

(defcomputed p "The selected habit's running totals." (if (>= computed.sel 0) (get computed.prefix computed.sel) []))

(defcomputed s "The selected habit's scores." (if (>= computed.sel 0) (get computed.score computed.sel) []))

(defcomputed r "The selected habit's days towards its frequency." (if (>= computed.sel 0) (get computed.ratio computed.sel) []))

(defcomputed dates "The selected habit's dates." (map #(addDays computed.h.from %) (range computed.n)))

(defcomputed keys "Each date's week, month, quarter and year."
  (map-indexed (fn [index it]
                 {:day index
                  :week (floor (/ (- (daysBetween "2000-01-02" it) state.firstWeekday) 7))
                  :month (slice it 0 7)
                  :quarter (+ (slice it 0 4) (floor (/ (dec (number (slice it 5 7))) 3)))
                  :year (slice it 0 4)})
               computed.dates))

(defcomputed scoreEnds "The last days of the score's periods."
  (filter (fn [it]
            (or (= it (dec computed.n))
                (not= (get-in computed.keys [it state.scorePeriod])
                      (get-in computed.keys [(inc it) state.scorePeriod]))))
          (range computed.n)))

(defcomputed scoreShown "The score's periods shown: the last 30 days, or 20 periods." (slice computed.scoreEnds (case state.scorePeriod :day -30 -20)))

(defcomputed scoreLabels "The score's axis labels."
  (map-indexed (fn [index it]
                 {:a (case state.scorePeriod
                       :year (slice (get computed.dates it) 0 4)
                       :quarter (t "quarter" {:n (inc (floor (/ (dec (number (slice (get computed.dates it) 5 7))) 3)))})
                       :month (formatDate (dateMs (get computed.dates it)) "MMM" "utc")
                       (formatDate (dateMs (get computed.dates it)) "d" "utc"))
                  :b (cond (and (pos? index)
                                (= (slice (get computed.dates it) 0 (if (#{:month :quarter} state.scorePeriod) 4 7))
                                   (slice (get computed.dates (get computed.scoreShown (dec index)))
                                          0
                                          (if (#{:month :quarter} state.scorePeriod) 4 7)))) ""
                           (#{:month :quarter} state.scorePeriod) (slice (get computed.dates it) 0 4)
                           (= state.scorePeriod :year) ""
                           :else (formatDate (dateMs (get computed.dates it)) "MMM" "utc"))})
               computed.scoreShown))

(defcomputed historyEnds "The last days of the history's periods."
  (filter (fn [it]
            (or (= it (dec computed.n))
                (not= (get-in computed.keys [it state.historyPeriod])
                      (get-in computed.keys [(inc it) state.historyPeriod]))))
          (range computed.n)))

(defcomputed historyShown "The history's periods shown." (slice computed.historyEnds (case state.historyPeriod :day -30 -20)))

(defcomputed historyLabels "The history's axis labels."
  (map-indexed (fn [index it]
                 {:a (case state.historyPeriod
                       :year (slice (get computed.dates it) 0 4)
                       :quarter (t "quarter" {:n (inc (floor (/ (dec (number (slice (get computed.dates it) 5 7))) 3)))})
                       :month (formatDate (dateMs (get computed.dates it)) "MMM" "utc")
                       (formatDate (dateMs (get computed.dates it)) "d" "utc"))
                  :b (cond (and (pos? index)
                                (= (slice (get computed.dates it) 0 (if (#{:month :quarter} state.historyPeriod) 4 7))
                                   (slice (get computed.dates (get computed.historyShown (dec index)))
                                          0
                                          (if (#{:month :quarter} state.historyPeriod) 4 7)))) ""
                           (#{:month :quarter} state.historyPeriod) (slice (get computed.dates it) 0 4)
                           (= state.historyPeriod :year) ""
                           :else (formatDate (dateMs (get computed.dates it)) "MMM" "utc"))})
               computed.historyShown))

(defcomputed scorePts "The score at each period shown." (map #(get-in computed.s [% :s]) computed.scoreShown))

(defcomputed scorePath "The score as a path."
  (if (empty? computed.scorePts)
      ""
      (str "M "
           (join " L "
                 (map-indexed (fn [index it]
                                (str (fixed (/ (* (+ index 0.5) 1000) (count computed.scorePts)) 1)
                                     " "
                                     (fixed (- 100 (* it 100)) 1)))
                              computed.scorePts)))))

(defcomputed historyVals "How much each period of the history holds."
  (map-indexed (fn [index it]
                 (- (get computed.p it)
                    (if (pos? (- (+ index (count computed.historyEnds)) (count computed.historyShown)))
                        (get computed.p
                             (get computed.historyEnds
                                  (dec (- (+ index (count computed.historyEnds)) (count computed.historyShown)))))
                        0)))
               computed.historyShown))

(defcomputed historyMax "The history's highest bar." (max (concat [1] computed.historyVals)))

(defcomputed stats "The overview: the score now, its change in a month and a year, and the total."
  (if (zero? computed.n)
      {:score 0 :month 0 :year 0 :total 0}
      {:score (get-in computed.s [(dec computed.n) :s])
       :month (- (get-in computed.s [(dec computed.n) :s])
                 (if (> computed.n 30) (get-in computed.s [(- computed.n 31) :s]) 0))
       :year (- (get-in computed.s [(dec computed.n) :s])
                (if (> computed.n 365) (get-in computed.s [(- computed.n 366) :s]) 0))
       :total (get computed.p (dec computed.n))}))

(defcomputed weeks "How many weeks the calendar shows."
  (if (>= computed.sel 0)
      (clamp (inc (ceil (/ (inc (daysBetween computed.h.from computed.weekStart)) 7)))
             1
             (floor (/ (- computed.cardWidth 65) 25)))
      0))

(defcomputed runs "The selected habit's runs of days done."
  (scan computed.r
        (fn [acc it]
          (cond (= (get computed.v index) -1) acc
                (>= it.r 1) {:len (inc acc.len) :start (if (pos? acc.len) acc.start index)}
                :else {:len 0 :start (inc index)}))
        {:len 0 :start 0}))

(defcomputed streaks "The longest streaks, at most ten, in order of their ends."
  (sort-by :to
           >
           (slice (->> (range computed.n)
                       (filter (fn [it]
                                 (and (pos? (get-in computed.runs [it :len]))
                                      (or (= it (dec computed.n)) (zero? (get-in computed.runs [(inc it) :len]))))))
                       ; style: keep, gives a map
                       (map (fn [it]
                              {:len (get-in computed.runs [it :len])
                               :from (get computed.dates (get-in computed.runs [it :start]))
                               :to (get computed.dates it)}))
                       (sort-by #(- (:len %))))
                  0
                  6)))

(defcomputed streakMax "The longest streak's length." (max (concat [1] (map :len computed.streaks))))

(defcomputed months "The last twelve months." (map #(slice (addMonths (str (slice state.today 0 7) "-01") (- % 11)) 0 7) (range 12)))

(defcomputed notes "The selected habit's notes, newest first."
  (if (>= computed.sel 0)
      (sort-by :key > (filter #(not= (str/trim (:value %)) "") (entries (or-else computed.h.notes {}))))
      []))

(defcomputed goals "A measurable habit's goals: this week's, month's, quarter's and year's."
  (if (or (neg? computed.sel) (not= computed.h.kind :measurable))
      []
      ; style: keep, gives a map
      (map (fn [it]
             {:period it
              :start (case it
                       :day state.today
                       :week computed.weekStart
                       :month (str (slice state.today 0 8) "01")
                       :quarter (+ (slice state.today 0 5)
                                   (pad (inc (* (floor (/ (dec (number (slice state.today 5 7))) 3)) 3)) 2 "0")
                                   "-01")
                       (str (slice state.today 0 5) "01-01"))})
           ["day" "week" "month" "quarter" "year"])))

(defcomputed goalRows "How far each goal is."
  ; style: keep, gives a map
  (map (fn [it]
         {:period it.period
          :value (- (get computed.p (dec computed.n))
                    (if (pos? (- (dec computed.n) (daysBetween it.start state.today)))
                        (get computed.p (- (- computed.n 2) (daysBetween it.start state.today)))
                        0))
          :goal (* (/ (number computed.h.target) (max 1 computed.h.frequency.days))
                   (case it.period
                     :day 1
                     :week 7
                     :year (daysBetween it.start (addMonths it.start 12))
                     (daysBetween it.start (addMonths it.start (if (= it.period :month) 1 3)))))})
       computed.goals))

(defcomputed palette "The command palette's rows: the commands, but those an agent runs with arguments (habitAdd...)."
  ; style: keep, gives a map
  (map (fn [it] {:title (t (str "cmd." it.name)) :detail (or-else it.keyLabel "") :run it.name})
       (filter #(and (:enabled %) (not= (:name %) "commandPalette") (not= (slice (:name %) 0 5) "habit")) commands)))

(defcomputed signedIn "Whether the person is signed in to the server." (= (or-else auth.streakServer.state "signedOut") "signedIn"))

(defcomputed online "Whether the server's channel is open." (or-else connections.streakServer.online false))

(defcomputed syncData "What the server keeps: the habits." {:habits data.ordered})

(defcomputed syncState "The sync's state: error, online, offline or signed out."
  (cond (not= state.sync.error "") "error" computed.online "online" computed.signedIn "offline" :else "signedOut"))

(defcomputed syncText "What the sync says in the settings."
  (t "sync.commits" {:state (t (str "sync." computed.syncState) {:message state.sync.error}) :n state.sync.commits}))

(defcomputed csvRows "Each habit's days as CSV lines."
  (map-indexed (fn [index it]
                 (scan (get computed.vals index)
                       (fn [acc it]
                         {:h acc.h
                          :line (str (get-in data.ordered [acc.h :name])
                                     ","
                                     (addDays (get-in data.ordered [acc.h :from]) index)
                                     ","
                                     (if (= (get-in data.ordered [acc.h :kind]) :measurable)
                                         (text it)
                                         (case it 1 "YES" -1 "SKIP" "NO")))})
                       {:h index}))
               data.ordered))

(defcomputed freqText "How often the selected habit is, and its target, in words."
  (cond (neg? computed.sel) ""
        (= computed.h.kind :measurable) (t "target"
           {:type computed.h.targetType
            :value computed.h.target
            :unit computed.h.unit
            :per (case computed.h.frequency.days 1 "day" 7 "week" "month")})
        (and (= computed.h.frequency.days 1) (= computed.h.frequency.times 1)) (t "freq.daily")
        (and (= computed.h.frequency.times 1) (not (#{7 30} computed.h.frequency.days)))
        (t "freq.everyN" {:n computed.h.frequency.days})
        :else (t (if (= computed.h.frequency.days 7) "freq.week" "freq.month") {:n computed.h.frequency.times})))

