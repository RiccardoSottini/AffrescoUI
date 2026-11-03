;;; detail.lisp --- TODO

(defview detail-bar "#detailBar." []
  `(node#detailBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @c.bar :shadow {:y 1 :blur 3 :color "#00000020"}
     (node#backButton
     :style iconButton
     :icon arrow-left
     :label ~(t "back")
     :tooltip ~(t "back")
     :textColor @c.text
     :onClick {:set state.page :to list}
     :when ~(not computed.wide)
     :anchors {:left 4 :top 8}
     :mirror true)
     (node#detailBadge
     :when ~(>= computed.sel 0)
     :anchors {:left ~(if computed.wide 20 56) :top 21}
     :width 14
     :height 14
     :radius 5
     :color ~computed.col)
     (label#label-sel-0-h :text ~(if (>= computed.sel 0) computed.h.name (t "app"))
       :font title
       :textColor ~computed.col
       :anchors {:left ~(if computed.wide 44 80) :right 100 :top 16}
       :height 24
       :role heading)
     (row#detailActions :when ~(>= computed.sel 0) :anchors {:right 4 :top 8} :width 84 :height 40 :gap 4 :arrows horizontal
       (node#editButton
       :style iconButton
       :icon edit
       :label ~(t "edit")
       :tooltip ~(t "edit")
       :textColor @c.text
       :onClick editHabit)
       ~(detail-more))))

(defview detail-more "#detailMore." []
  `(node#detailMore :style iconButton
     :icon ellipsis
     :label ~(t "more")
     :tooltip ~(t "more")
     :textColor @c.text
     :onClick {:set state.menus.detail :to true}
     :children [(node#menu-detail-more :use menu
                  :with {:open state.menus.detail
                         :width 200
                         :placement below-end
                         :icons true
                         :items ~[{:title (if computed.h.archived (t "unarchive") (t "archive"))
                                   :icon (if computed.h.archived "archive-restore" "archive")
                                   :keyLabel (:keyLabel (find (fn [it] (= it.name "archiveHabit")) commands))
                                   :run "archiveHabit"}
                                  {:title (t "exportCsv") :icon "file-down" :run "exportHabit"}
                                  {:separator true}
                                  {:title (t "delete")
                                   :icon "trash-2"
                                   :keyLabel (:keyLabel (find (fn [it] (= it.name "confirmDelete")) commands))
                                   :run "confirmDelete"}]})]
     :isOpen ~state.menus.detail))

(defview detail-cards "#detailCards." []
  `(column#detailCards :when ~(>= computed.sel 0)
     :anchors {:left 0 :right 0 :top 56 :bottom 0}
     :scroll vertical
     :gap 12
     :alignItems center
     (node#panel-detail-cards :height 0 :width 10 :color transparent)
     (node#subtitle :style card :width ~computed.cardWidth :height ~(if (not= computed.h.description "") 92 70)
       (label#label-h-question :text ~computed.h.question
         :textColor ~computed.col
         :anchors {:left 16 :right 16 :top 12}
         :height 20)
       ~(subtitle-facts)
       (label#label-h-description :text ~computed.h.description
         :when ~(not= computed.h.description "")
         :font small
         :textColor @c.text
         :anchors {:left 16 :right 16 :top 64}
         :height 18))
     (node#overviewCard :style card :width ~computed.cardWidth :height 112 :role region :label ~(t "card.overview")
       (node#label-t-card-overview :style cardTitle :text ~(t "card.overview") :textColor ~computed.col)
       ~(overview-stats))
     (node#targetCard :style card
       :width ~computed.cardWidth
       :height 232
       :role region
       :label ~(t "card.target")
       :children [{:id label-t-card-target :style cardTitle :text ~(t "card.target") :textColor ~computed.col}
                  (column#goalRows :anchors {:left 16 :right 16 :top 48} :height 170 :gap 6
                    ~(repeat [g computed.goalRows]
                      (goal-row g)))]
       :when ~(= computed.h.kind "measurable"))
     ~(score-card)
     (node#historyCard :style card :width ~computed.cardWidth :height 240 :role region :label ~(t "card.history")
       (node#label-t-card-history :style cardTitle :text ~(t "card.history") :textColor ~computed.col)
       (node#period-button-history-card :use periodButton
         :with {:key historyPeriod :color ~computed.col}
         :anchors {:right 8 :top 8})
       ~(history-plot)
       (row#historyLabels :dir ltr :anchors {:left 16 :right 16 :bottom 8} :height 30
         ~(repeat [lb computed.historyLabels]
           `(node#historyLabel :grow 1 :anchors {:top 0 :bottom 0} :color transparent
              (node#label-lb-a-2 :style axis :text ~lb.a)
              (node#label-lb-b-2 :style axis2 :text ~lb.b)))))
     ~(calendar-card)
     (node#streaksCard :style card
       :width ~computed.cardWidth
       :height ~(+ (* (max 1 (count computed.streaks)) 26) 60)
       :role region
       :label ~(t "card.streaks")
       (node#label-t-card-streaks :style cardTitle :text ~(t "card.streaks") :textColor ~computed.col)
       (node#label-t-no-streaks
       :style caption
       :text ~(t "noStreaks")
       :when ~(= (count computed.streaks) 0)
       :anchors {:left 16 :top 46})
       (column#streakRows :anchors {:left 16 :right 16 :top 46} :height ~(* (count computed.streaks) 26) :gap 4
         ~(repeat [st computed.streaks]
           (streak-row st))))
     ~(frequency-card)
     (node#notesCard :style card
       :width ~computed.cardWidth
       :height ~(+ (* (max 1 (count computed.notes)) 44) 60)
       :role region
       :label ~(t "card.notes")
       (node#label-t-card-notes :style cardTitle :text ~(t "card.notes") :textColor ~computed.col)
       (node#label-t-no-notes
       :style caption
       :text ~(t "noNotes")
       :when ~(= (count computed.notes) 0)
       :anchors {:left 16 :top 46})
       (column#noteRows :anchors {:left 0 :right 0 :top 44} :height ~(* (count computed.notes) 44)
         ~(repeat [nt computed.notes]
           (note-row nt))))
     (node#panel-detail-cards-2 :height 12 :width 10 :color transparent)))

(defview subtitle-facts "#subtitleFacts." []
  `(row#subtitleFacts :anchors {:left 16 :right 16 :top 38} :height 20 :gap 20
     (node#label-freq-text :style caption :text ~computed.freqText :icon repeat :height 20)
     (node#label-h-reminder-null
     :style caption
     :text ~(if (nil? computed.h.reminder)
                 (t "reminderOff")
                 (formatTime (+ (dateMs "2000-01-01")
                                (* (+ (* (number (slice computed.h.reminder.time 0 2)) 60)
                                      (number (slice computed.h.reminder.time 3 5)))
                                   60000))
                             "short"
                             "utc"))
     :icon ~(if (nil? computed.h.reminder) "bell-off" "bell")
     :height 20)
     (node#label-t-archived-tag
     :style caption
     :text ~(t "archivedTag")
     :when ~computed.h.archived
     :icon archive
     :height 20)))

(defview overview-stats "#overviewStats." []
  `(row#overviewStats :anchors {:left 16 :right 16 :top 44} :height 56 :justify space-around :alignItems center
     (node#ring-overview-stats :use
                               ring
                               :with
                               {:value ~computed.stats.score :color ~computed.col :size 48 :stroke 2.5})
     (node#stat-overview-stats :use stat
       :with {:value ~(formatPercent computed.stats.score) :caption ~(t "stat.score") :color ~computed.col})
     (node#stat-overview-stats-2 :use stat
       :with {:value ~(+ (if (>= computed.stats.month 0) "+" "") (formatPercent computed.stats.month))
              :caption ~(t "stat.month")
              :color ~computed.col})
     (node#stat-overview-stats-3 :use stat
       :with {:value ~(+ (if (>= computed.stats.year 0) "+" "") (formatPercent computed.stats.year))
              :caption ~(t "stat.year")
              :color ~computed.col})
     (node#stat-overview-stats-4 :use stat
       :with {:value ~(formatNumber computed.stats.total 0 1) :caption ~(t "stat.total") :color ~computed.col})))

(defview goal-row "#goalRow." [g]
  `(node#goalRow :anchors {:left 0 :right 0}
     :height 28
     :color transparent
     :role progressbar
     :label ~(t (str "goal." g.period))
     :description ~(t "goalOf" {:value (formatNumber g.value 0 1) :goal (formatNumber g.goal 0 1)})
     (label#label-t-goal-g :text ~(t (str "goal." g.period))
       :font small
       :textColor @c.text
       :anchors {:left 0 :top 0}
       :fitText true)
     (node#label-t-goal-of
     :style caption
     :text ~(str (t "goalOf" {:value (formatNumber g.value 0 1) :goal (formatNumber g.goal 0 1)}) " " computed.h.unit)
     :anchors {:right 0 :top 0})
     (node#panel-goal-row :anchors {:left 0 :right 0 :top 19} :height 6 :radius 3 :color @c.track
       (node#panel-goal-row-2
       :anchors {:left 0 :top 0 :bottom 0}
       :radius 3
       :width ~(str (* (clamp (/ g.value (max 0.001 g.goal)) 0 1) 100) "%")
       :color ~(if (and (= computed.h.targetType "atMost") (> g.value g.goal)) "@c.danger" computed.col)))))

(defview score-card "#scoreCard." []
  `(node#scoreCard :style card :width ~computed.cardWidth :height 240 :role region :label ~(t "card.score")
     (node#label-t-card-score :style cardTitle :text ~(t "card.score") :textColor ~computed.col)
     (node#period-button-score-card :use periodButton
       :with {:key scorePeriod :color ~computed.col}
       :anchors {:right 8 :top 8})
     ~(score-plot)
     ~(repeat [g (range 5)]
       `(label#scoreAxis :font tiny
          :textColor @c.muted
          :text ~(str (- 100 (* g 25)) "%")
          :anchors {:right 12 :top ~(- (+ 56 (* g 35)) 6)}
          :height 12
          :width 34
          :align end))
     (row#scoreLabels :dir ltr :anchors {:left 16 :right 52 :bottom 8} :height 30
       ~(repeat [lb computed.scoreLabels]
         `(node#scoreLabel :grow 1 :anchors {:top 0 :bottom 0} :color transparent
            (node#label-lb-a :style axis :text ~lb.a)
            (node#label-lb-b :style axis2 :text ~lb.b))))))

(defview score-plot "#scorePlot." []
  `(node#scorePlot :dir ltr :anchors {:left 16 :right 52 :top 56 :bottom 44} :color transparent
     ~(repeat [g (range 5)]
       `{:id gridLine :anchors {:left 0 :right 0 :top ~(str (* g 25) "%")} :height 1 :color @c.divider})
     (path#scoreLine :fill 0
       :viewBox "0 0 1000 100"
       :fit fill
       :d ~computed.scorePath
       :stroke ~computed.col
       :strokeWidth 2
       :strokeJoin round
       :strokeCap round)
     (row#row-score-plot :fill 0
       ~(repeat [pt computed.scorePts :index i]
         `(node#scoreSlot :grow 1 :anchors {:top 0 :bottom 0} :color transparent :tooltip ~(formatPercent pt)
            (node#panel-score-slot
            :anchors {:left "50%" :top ~(str (- 100 (* pt 100)) "%")}
            :translate [-3.5 -3.5]
            :width 7
            :height 7
            :radius 4
            :color ~computed.col))))))

(defview history-plot "#historyPlot." []
  `(node#historyPlot :dir ltr :anchors {:left 16 :right 16 :top 56 :bottom 44} :color transparent
     (node#panel-history-plot :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @c.divider)
     (row#row-history-plot :fill 0
       ~(repeat [hv computed.historyVals :index i]
         `(node#historySlot :grow 1 :anchors {:top 0 :bottom 0} :color transparent :tooltip ~(formatNumber hv 0 1)
            (node#panel-history-slot
            :anchors {:left "22%" :right "22%" :bottom 1}
            :height ~(str (* (/ hv computed.historyMax) 86) "%")
            :color ~computed.col
            :radius {:topLeft 2 :topRight 2})
            (label#label-hv-0-format :text ~(if (> hv 0) (formatNumber hv 0 0) "")
              :font tiny
              :textColor ~computed.col
              :anchors {:left -8 :right -8 :bottom ~(str (* (/ hv computed.historyMax) 86) "%")}
              :translate [0 -2]
              :height 12
              :align center))))))

(defview calendar-card "#calendarCard." []
  `(node#calendarCard :style card :width ~computed.cardWidth :height 268 :role region :label ~(t "card.calendar")
     (node#label-t-card-calendar :style cardTitle :text ~(t "card.calendar") :textColor ~computed.col)
     (row#calendarWeeks :dir rtl :anchors {:left 16 :right 52 :top 48} :height 210 :gap 3
       ~(repeat [w (range computed.weeks)]
         `(column#calWeek :dir ltr :width 22 :anchors {:top 0 :bottom 0} :gap 3
            (node#label-number-format-date
            :style tiny
            :text ~(if (<= (number (formatDate (dateMs (addDays computed.weekStart (* (- 7) w))) "d" "utc")) 7)
                        (formatDate (dateMs (addDays computed.weekStart (* (- 7) w))) "MMM" "utc")
                        "")
            :height 14
            :width 40)
            ~(repeat [r (range 7)]
              (cal-cell w r)))))
     (column#calWeekdays :anchors {:right 12 :top 65} :width 36 :height 180 :gap 3
       ~(repeat [wd computed.weekdays]
         `{:id calWeekday
           :style tiny
           :text ~(formatDate (dateMs (addDays "2000-01-02" wd)) "EEE" "utc")
           :height 22
           :width 36
           :align {:horizontal start :vertical center}}))))

(defview cal-cell "#calCell." [w r]
  `(node#calCell :let {:d ~(addDays computed.weekStart (- r (* 7 w)))
                       :li ~(daysBetween computed.h.from d)
                       :cv ~(if (>= li 0) (get computed.v li) nil)
                       :cr ~(if (and (>= li 0) (some? cv)) (get-in computed.r [li :r]) 0)}
     :width 22
     :height 22
     :radius 3
     :visible ~(<= d state.today)
     :color ~(cond (nil? cv) "@c.track"
                   (= computed.h.kind "measurable") (if (> cv 0) (+ computed.col (cond (>= cr 1) "" (>= cr 0.5) "b0" :else "60")) "@c.track")
                   (= cv 1) computed.col
                   (= cv (- 1)) (str computed.col "60")
                   (>= cr 1) (str computed.col "60")
                   :else "@c.track")
     :cursor hand
     :role button
     :label ~(formatDate (dateMs d) "medium" "utc")
     :onClick ~(handler cellClick computed.sel d event.x event.y)
     :onContextMenu ~(handler openPopup computed.sel d event.x event.y)
     (label#label-format-date-date-3 :text ~(formatDate (dateMs d) "d" "utc")
       :font tiny
       :fill 0
       :align center
       :textColor ~(if (and (some? cv) (or (not= cv 0) (>= cr 1))) computed.onColor "@c.muted"))))

(defview streak-row "#streakRow." [st]
  `(row#streakRow :anchors {:left 0 :right 0}
     :height 22
     :justify center
     :alignItems center
     :gap 8
     :role text
     :label ~(str (formatDate (dateMs st.from) "medium" "utc")
                  " – "
                  (formatDate (dateMs st.to) "medium" "utc")
                  ": "
                  (t "days" {:n st.len}))
     (node#label-format-date-date-4
     :style tiny
     :text ~(formatDate (dateMs st.from) "d MMM yyyy" "utc")
     :width 80
     :height 14
     :align end)
     (node#panel-streak-row :height 22
       :width ~(max 28 (/ (* (- computed.cardWidth 224) st.len) computed.streakMax))
       :radius 2
       :color ~(if (= st.len computed.streakMax) computed.col (str computed.col "aa"))
       (label#label-text-st-len :text ~(text st.len) :font small :textColor ~computed.onColor :fill 0 :align center))
     (node#label-format-date-date-5
     :style tiny
     :text ~(formatDate (dateMs st.to) "d MMM yyyy" "utc")
     :width 80
     :height 14
     :align start)))

(defview frequency-card "#frequencyCard." []
  `(node#frequencyCard :style card :width ~computed.cardWidth :height 252 :role region :label ~(t "card.frequency")
     (node#label-t-card-frequency :style cardTitle :text ~(t "card.frequency") :textColor ~computed.col)
     (row#freqGrid :dir ltr :anchors {:left 16 :right 52 :top 48} :height 196
       ~(repeat [mo computed.months]
         `(column#freqMonth :grow 1 :anchors {:top 0 :bottom 0}
            ~(repeat [wd computed.weekdays]
              (freq-dot mo wd))
            (node#label-format-date-date-6
            :style tiny
            :text ~(formatDate (dateMs (str mo "-01")) "MMM" "utc")
            :anchors {:left 0 :right 0}
            :height 20
            :align center))))
     (column#freqWeekdays :anchors {:right 12 :top 48} :width 36 :height 168
       ~(repeat [wd computed.weekdays]
         `{:id freqWeekday
           :style tiny
           :text ~(formatDate (dateMs (addDays "2000-01-02" wd)) "EEE" "utc")
           :height 24
           :width 36
           :align {:horizontal start :vertical center}}))))

(defview freq-dot "#freqDot." [mo wd]
  `(node#freqDot :let {:c ~(count (filter (fn [it]
                                            (and (= (weekday (addDays (str mo "-01") it)) wd)
                                                 (> (or-else (get computed.v
                                                                  (daysBetween computed.h.from
                                                                               (addDays (str mo "-01") it)))
                                                             0)
                                                    0)))
                                          (range (daysInMonth (str mo "-01")))))}
     :anchors {:left 0 :right 0}
     :height 24
     :color transparent
     :tooltip ~(text c)
     (node#panel-freq-dot
     :anchors {:left "50%" :top 12}
     :translate ~[(- (if (= c 0) 2 (+ 4 (* c 2)))) (- (if (= c 0) 2 (+ 4 (* c 2))))]
     :width ~(if (= c 0) 4 (+ 8 (* c 4)))
     :height ~(if (= c 0) 4 (+ 8 (* c 4)))
     :radius 20
     :color ~(if (= c 0) "@c.track" (+ computed.col (get ["59" "80" "a6" "cc" "f2"] (- (min c 5) 1)))))))

(defview note-row "#noteRow." [nt]
  `(node#noteRow :anchors {:left 0 :right 0}
     :height 44
     :color transparent
     :hover {:color @c.hover}
     :focusable true
     :role button
     :cursor hand
     :label ~(str (formatDate (dateMs nt.key) "medium" "utc") ": " nt.value)
     :onClick ~(handler openPopup computed.sel nt.key event.x event.y)
     (label#label-format-date-date-7 :text ~(formatDate (dateMs nt.key) "EEE d MMM" "utc")
       :font small
       :textColor ~computed.col
       :anchors {:left 16 :top 14}
       :width 96
       :height 16)
     (label#label-nt-value :text ~nt.value :textColor @c.text :anchors {:left 120 :right 16 :top 13} :height 18)))

