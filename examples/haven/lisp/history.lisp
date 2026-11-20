;;; history.lisp --- Haven's logbook and history pages
;;;
;;; The logbook lists the events of a period of the last seven days, with a search; history draws a day of on and off
;;; states per device, and the living room's temperature, for today or one of the six days before it. The events are
;;; invented, in seed/.

(ns haven.history)

;;; The logbook

(defstate logQuery "" "The text the logbook is filtered by." :type (string :maxLength 64))

(defstate logPeriod twoDays "The days the logbook shows: today, yesterday, both, or the last seven."
  :type {:enum [today yesterday twoDays week]})

(defstate logPeriodOpen false "Whether the menu of the logbook's periods under its field is open.")

(defstate logRangeOpen false "Whether the menu of the logbook's periods under its bar's calendar is open.")

(defcomputed dayNames "Each day the records keep, by its name in them: today, yesterday, or how many days ago."
  {:today (t "log.today")
   :yesterday (t "log.yesterday")
   :ago2 (t "log.daysAgo" {:n 2})
   :ago3 (t "log.daysAgo" {:n 3})
   :ago4 (t "log.daysAgo" {:n 4})
   :ago5 (t "log.daysAgo" {:n 5})
   :ago6 (t "log.daysAgo" {:n 6})})

(defcomputed dayList "The days the records keep, the newest first, in words."
  [(t "log.today")
   (t "log.yesterday")
   (t "log.daysAgo" {:n 2})
   (t "log.daysAgo" {:n 3})
   (t "log.daysAgo" {:n 4})
   (t "log.daysAgo" {:n 5})
   (t "log.daysAgo" {:n 6})])

(defcomputed logDays "The days of the logbook's period, the newest first."
  (case state.logPeriod
    "today" ["today"]
    "yesterday" ["yesterday"]
    "week" ["today" "yesterday" "ago2" "ago3" "ago4" "ago5" "ago6"]
    ["today" "yesterday"]))

(defcomputed logPeriodName "The logbook's period, in words." (t (str "log.p." state.logPeriod)))

(defcomputed logPeriods "The logbook's period menu: one item a period, the chosen one checked."
  (map (fn [p]
         {:title (t (str "log.p." p))
          :icon (if (= p state.logPeriod) "check" "")
          :run "pick-log-period"
          :with {:period p}})
       ["today" "yesterday" "twoDays" "week"]))

(defcomputed logShown "The events whose name, or whose automation's, holds the search."
  (filter (fn [it]
            (or (= state.logQuery "")
                (contains (lower-case (event-name it)) (lower-case (trim state.logQuery)))))
          data.logbook))

(defmacro event-name
  ;; A macro, so that what it writes stays an expression of the binding that uses it.
  "What an event is named by: its automation, if one did it, else the thing it happened to."
  [ev]
  `(if (not= (:rule ~ev) "") (t (str "rule." (:rule ~ev))) (:name ~ev)))

(defcomponent logbookPage "The logbook: its bar, the period and the search, and the events by day."
  {}
  `(node :dir auto :color transparent
     ~(page-bar "logBar" "page.logbook" "logRange" "state.logRangeOpen" state.logRangeOpen computed.logPeriods)
     (row#logFilters :anchors {:left 24 :right 24 :top 72} :height 48 :gap 16
       ~(period-field "logPeriod" computed.logPeriodName "state.logPeriodOpen" state.logPeriodOpen computed.logPeriods)
       ~(log-search))
     (node#logCard :style card :anchors {:left 24 :right 24 :top 136 :bottom 24}
       {:style caption :when ~(empty? computed.logShown) :text ~(t "log.empty") :anchors {:left 24 :top 24}}
       (column#logList :anchors {:left 0 :right 0 :top 8 :bottom 8} :scroll vertical :role list
         :label ~(t "page.logbook")
         ~(repeat [day computed.logDays :key day]
           (log-day day))))))

(defview log-day "A day of the logbook: its name, and its events." [day]
  `(node#logDay :let {:events ~(filter #(= (:day %) day) computed.logShown)}
     :when ~(> (count events) 0)
     :anchors {:left 0 :right 0}
     :height ~(+ 40 (* (count events) 56))
     :color transparent
     :role group
     :label ~(get computed.dayNames day)
     {:style caption :text ~(upper-case (get computed.dayNames day)) :anchors {:left 24 :top 16}}
     (column :anchors {:left 0 :right 0 :top 40 :bottom 0}
       ~(repeat [ev events :key ev.id]
         (log-row ev)))))

(defview log-search "The search over the events." []
  `(node#logSearchBox :width 320
     :height 48
     :radius 24
     :color transparent
     :borderWidth 1
     :borderColor @border
     :hover {:color @accentSoft}
     (label :icon ~(if (= state.logQuery "") "plus" "search") :text "" :textColor @accent
       :anchors {:left 14 :top 0 :bottom 0} :width 24 :align center)
     (textarea#logSearch :bind state.logQuery
       :placeholder ~(t "hist.choose")
       :placeholderColor @accent
       :textColor @text
       :color transparent
       :anchors {:left 44 :right 16 :top 0 :bottom 0}
       :label ~(t "hist.choose"))))

(defview log-row "An event: its icon, its name and what happened, and when." [ev]
  `(node#logRow :anchors {:left 0 :right 0}
     :height 56
     :color transparent
     :hover {:color @cardHover}
     :role listitem
     :label ~(str (event-name ev) ": " (t (str "log." ev.what)) ", " ev.time)
     (label :icon ~ev.icon
       :text ""
       :anchors {:left 20 :top 8}
       :width 40
       :height 40
       :radius 20
       :align center
       :color ~(if (not= ev.rule "") "@accentSoft" "@raised")
       :textColor ~(if (not= ev.rule "") "@accent" "@muted"))
     (label :text ~(str (event-name ev) " " (t (str "log." ev.what)))
       :textColor @text
       :anchors {:left 76 :right 24 :top 9}
       :height 20)
     (label :font small :text ~ev.time :textColor @muted :anchors {:left 76 :top 31} :fitText true)))

;;; History

;;; The days before today are invented from today's: each thing changes state a little earlier or later, and the
;;; temperature is a day of its own, in pastTemps.

(defstate histDay 0 "The day history shows, in days before today: 0 is today, 6 the oldest kept." :type (number))

(defstate histPeriodOpen false "Whether the menu of history's days under its field is open.")

(defstate histRangeOpen false "Whether the menu of history's days under its bar's calendar is open.")

(defstate pastTemps
  [[19.7 19.4 19.4 19 19 19 19 19.9 20.5 21.1 21.1 21 21.4 21.6 21.1 21.1 21.4 21.8 21.8 21.7 21.7 21.1 21.2 21.1]
   [20.2 19.8 19.7 19.7 19.4 19.4 19.7 20.3 21.2 21.4 21.6 21.6 22 22 21.7 21.7 21.8 22 22.4 22.3 21.9 21.8 21.6 21.9]
   [19.7 19.2 19.3 19 19 19 19 19.7 20.3 21 21.2 21 21.4 21.2 21.2 21 21.1 21.4 21.7 21.7 21.3 21.2 20.7 21.1]
   [20.3 20.1 19.8 19.4 19.4 19.4 19.3 20.3 21 21.3 21.5 21.7 21.7 21.8 21.7 21.7 21.5 22 22.2 22.3 22 21.8 21.4 21.6]
   [19.8 19.8 19.6 19.1 19 19 19.1 20 20.8 21.1 21.2 21.3 21.4 21.6 21.6 21.4 21.4 21.7 22 21.6 21.8 21.5 21.3 21.4]
   [20.4 20.1 19.7 19.8 19.4 19.3 19.6 20.4 21.2 21.5 21.7 21.7 21.8 22 21.7 21.9 21.9 22.1 22.3 22.2 22 21.7 21.8 22]]
  "The living room's temperature on the six days before today, from yesterday back, hour by hour from midnight."
  :type (array :items (array :items (number))))

(defcomputed histTemps "The living room's temperature on the day history shows."
  (if (= state.histDay 0) state.tempHistory (get state.pastTemps (dec state.histDay))))

(defn day-segs "A thing's spells of on and off, days days before today: each change of state a little earlier or later."
  [segs days]
  (let [last (dec (count segs))
        shift (fn [i]
                (if (or (= days 0) (< i 0) (>= i last))
                  0
                  (let [phase (dec (mod (+ days i i) 3))
                        shorter (min (get-in segs [i :h]) (get-in segs [(inc i) :h]))]
                    (* phase 0.45 shorter))))
        move (fn [i]
               (- (shift i) (shift (dec i))))]
    (map-indexed (fn [i seg]
                   (update seg :h + (move i)))
                 segs)))

(defcomputed histTimeline "Each thing's spells of on and off on the day history shows."
  (map #(assoc % :segs (day-segs (:segs %) state.histDay)) data.timeline))

(defcomputed histTitle "The timeline's title: today's activity, or a day before's."
  (if (= state.histDay 0) (t "hist.timeline") (t "hist.timelineOn" {:day (get computed.dayList state.histDay)})))

(defcomputed histPeriodName "History's day, in words: today so far, or the whole of a day before."
  (if (= state.histDay 0) (t "hist.today") (t "hist.day" {:day (get computed.dayList state.histDay)})))

(defcomputed histDays "History's day menu: today and the six days before it, the shown one checked."
  (map (fn [d]
         {:title (get computed.dayList d)
          :icon (if (= d state.histDay) "check" "")
          :run "pick-hist-day"
          :with {:day d}})
       (range 7)))

(defcomponent historyPage "History: its bar, the period, each thing's day of on and off, and the temperature."
  {}
  `(node :dir auto :color transparent
     ~(page-bar "histBar" "page.history" "histRange" "state.histRangeOpen" state.histRangeOpen computed.histDays)
     (row#histFilters :anchors {:left 24 :right 24 :top 72} :height 48 :gap 16
       ~(period-field "histPeriod" computed.histPeriodName "state.histPeriodOpen" state.histPeriodOpen
                      computed.histDays)
       ~(entity-button))
     (node#histTimeline :style card
       :anchors {:left 24 :right 24 :top 136}
       :height ~(+ 76 (* (count computed.histTimeline) 40) 28)
       :role group
       :label ~computed.histTitle
       {:style cardTitle :text ~computed.histTitle}
       (column :anchors {:left 24 :right 24 :top 60} :height ~(* (count computed.histTimeline) 40) :role list
         ~(repeat [ent computed.histTimeline :key ent.id]
           (entity-day ent)))
       (row :anchors {:left 172 :right 24 :bottom 12} :height 20 :justify space-between
         {:style caption :text "00:00"}
         {:style caption :text "06:00"}
         {:style caption :text "12:00"}
         {:style caption :text "18:00"}
         {:style caption :text ~(if (= state.histDay 0) (t "hist.now") "24:00")}))
     ~(temperature-card)))

(defview entity-button "Choose an entity: it opens the search over the controls." []
  `(label#histTarget :icon plus
     :text ~(t "hist.choose")
     :textColor @accent
     :width 320
     :height 48
     :padding 12
     :color transparent
     :borderWidth 1
     :borderColor @border
     :radius 24
     :align {:horizontal start :vertical center}
     :hover {:color @accentSoft}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "hist.choose")
     :onClick {:set state.quickPick.open :to true}))

(defview entity-day "A thing's day: its name, and a bar for each time it was on or off." [ent]
  `(node#histEntity :anchors {:left 0 :right 0} :height 40 :color transparent :role listitem :label ~ent.name
     (label :text ~ent.name
       :textColor @text
       :anchors {:left 0 :top 0 :bottom 0}
       :width 140
       :align {:horizontal start :vertical center})
     (row :anchors {:left 148 :right 0 :top 6} :height 28 :gap 0
       ~(repeat [seg ent.segs :index si]
         `(label :grow ~seg.h
            :basis 0
            :anchors {:top 0 :bottom 0}
            :radius {:topLeft ~(if (= si 0) 4 0)
                     :bottomLeft ~(if (= si 0) 4 0)
                     :topRight ~(if (= si (dec (count ent.segs))) 4 0)
                     :bottomRight ~(if (= si (dec (count ent.segs))) 4 0)}
            :color ~(if seg.on "@accent" "@raised")
            :textColor ~(if seg.on "@onAccent" "@muted")
            :font small
            :align center
            :text ~(if (>= seg.h 2) (t (+ ent.k (if seg.on ".on" ".off"))) "")
            :tooltip ~(t (+ ent.k (if seg.on ".on" ".off"))))))))

(defview temperature-card "The living room's temperature over the day, on a scale of degrees." []
  `(node#histTemp :style card
     :anchors {:left 24 :right 24 :top ~(+ 136 104 (* (count data.timeline) 40) 16) :bottom 24}
     :role group
     :label ~(t "hist.temp" {:name state.rooms.0.name})
     {:style cardTitle :text ~(t "hist.temp" {:name state.rooms.0.name})}
     {:style caption
      :text ~(t "hist.range" {:min (min computed.histTemps) :max (max computed.histTemps)})
      :anchors {:right 24 :top 24}}
     (column#histScale :anchors {:left 24 :right 24 :top 52 :bottom 32} :justify space-between
       ~(repeat [deg [23 22 21 20 19]]
         `(node :anchors {:left 0 :right 0} :height 16 :color transparent
            (label :font small
              :text ~(str deg "°")
              :textColor @muted
              :anchors {:left 0 :top 0}
              :width 28
              :height 16
              :align {:horizontal end :vertical center})
            {:anchors {:left 40 :right 0 :top 8} :height 1 :color @divider})))
     (node :use lineChart
       :with {:values ~(map #(- % 19) computed.histTemps) :top 4 :color @warm :lineWidth 2}
       :anchors {:left 64 :right 24 :top 60 :bottom 40})
     (row :anchors {:left 64 :right 24 :bottom 12} :height 16 :justify space-between
       ~(repeat [h ["00:00" "06:00" "12:00" "18:00" (if (= state.histDay 0) (t "hist.now") "24:00")]]
         `(label :font small :text ~h :textColor @muted :fitText true)))))

;;; Parts both pages use

(defview page-bar "A page's bar: its title, and a calendar button that opens a menu of the days kept."
  [id key rangeId openPath isOpen items]
  `(node :id ~id :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
     (label :text ~(t key)
       :font heading
       :textColor @onHeader
       :anchors {:left 24 :top 0 :bottom 0}
       :fitText true
       :align {:horizontal start :vertical center})
     (label :id ~rangeId
       :font iconsMedium
       :icon calendar
       :text ""
       :textColor @onHeader
       :anchors {:right 16 :top 8}
       :width 40
       :height 40
       :radius 20
       :align center
       :hover {:color @headerHover}
       :cursor hand
       :focusable true
       :role button
       :label ~(t "energy.pick")
       :tooltip ~(t "energy.pick")
       :isOpen ~isOpen
       :onClick {:toggle ~openPath}
       (node :use popMenu :with {:open ~openPath :width 220 :items ~items}))))

(defview period-field "The period shown, in a field that opens a menu of the periods kept."
  [id text openPath isOpen items]
  `(label :id ~id
     :icon calendar
     :text ~text
     :textColor @text
     :width 280
     :height 48
     :padding 12
     :color @field
     :borderWidth 1
     :borderColor ~(if isOpen "@accent" "@border")
     :radius 4
     :align {:horizontal start :vertical center}
     :hover {:color @cardHover}
     :cursor hand
     :focusable true
     :role button
     :label ~text
     :isOpen ~isOpen
     :onClick {:toggle ~openPath}
     (label :icon chevron-down :text "" :textColor @muted :anchors {:right 12 :top 12} :width 24 :height 24
       :align center)
     (node :use popMenu :with {:open ~openPath :width 280 :items ~items})))
