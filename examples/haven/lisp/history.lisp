;;; history.lisp --- Haven's logbook and history pages
;;;
;;; The logbook lists today's and yesterday's events, with a search; history draws a day of on and off states per
;;; device, and the living room's temperature. The events are invented, in seed/.

;;; The logbook

(defstate logQuery "" "The text the logbook is filtered by." :type (string :maxLength 64))

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
     ~(page-bar "logBar" "page.logbook" "logRange" "log.twoDays")
     (row#logFilters :anchors {:left 24 :right 24 :top 72} :height 48 :gap 16
       ~(period-field "logPeriod" "log.period" "log.twoDays")
       ~(log-search))
     (node#logCard :style card :anchors {:left 24 :right 24 :top 136 :bottom 24}
       {:style caption :when ~(= (count computed.logShown) 0) :text ~(t "log.empty") :anchors {:left 24 :top 24}}
       (column#logList :anchors {:left 0 :right 0 :top 8 :bottom 8} :scroll vertical :role list
         :label ~(t "page.logbook")
         ~(repeat [day ["today" "yesterday"] :key day]
           (log-day day))))))

(defview log-day "A day of the logbook: its name, and its events." [day]
  `(node#logDay :let {:events ~(filter (fn [it] (= it.day day)) computed.logShown)}
     :when ~(> (count events) 0)
     :anchors {:left 0 :right 0}
     :height ~(+ 40 (* (count events) 56))
     :color transparent
     :role group
     :label ~(t (str "log." day))
     {:style caption :text ~(upper-case (t (str "log." day))) :anchors {:left 24 :top 16}}
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

(defcomponent historyPage "History: its bar, the period, each thing's day of on and off, and the temperature."
  {}
  `(node :dir auto :color transparent
     ~(page-bar "histBar" "page.history" "histRange" "energy.onlyToday")
     (row#histFilters :anchors {:left 24 :right 24 :top 72} :height 48 :gap 16
       ~(period-field "histPeriod" "hist.today" "energy.onlyToday")
       ~(entity-button))
     (node#histTimeline :style card
       :anchors {:left 24 :right 24 :top 136}
       :height ~(+ 76 (* (count data.timeline) 40) 28)
       :role group
       :label ~(t "hist.timeline")
       {:style cardTitle :text ~(t "hist.timeline")}
       (column :anchors {:left 24 :right 24 :top 60} :height ~(* (count data.timeline) 40) :role list
         ~(repeat [ent data.timeline :key ent.id]
           (entity-day ent)))
       (row :anchors {:left 172 :right 24 :bottom 12} :height 20 :justify space-between
         {:style caption :text "00:00"}
         {:style caption :text "06:00"}
         {:style caption :text "12:00"}
         {:style caption :text "18:00"}
         {:style caption :text ~(t "hist.now")}))
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
                     :topRight ~(if (= si (- (count ent.segs) 1)) 4 0)
                     :bottomRight ~(if (= si (- (count ent.segs) 1)) 4 0)}
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
      :text ~(t "hist.range" {:min (min state.tempHistory) :max (max state.tempHistory)})
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
       :with {:values ~(map (fn [it] (- it 19)) state.tempHistory) :top 4 :color @warm :lineWidth 2}
       :anchors {:left 64 :right 24 :top 60 :bottom 40})
     (row :anchors {:left 64 :right 24 :bottom 12} :height 16 :justify space-between
       ~(repeat [h ["00:00" "06:00" "12:00" "18:00" (t "hist.now")]]
         `(label :font small :text ~h :textColor @muted :fitText true)))))

;;; Parts both pages use

(defview page-bar "A page's bar: its title, and the button of its dates, off as only these days are kept."
  [id key rangeId tip]
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
       :textColor @off
       :anchors {:right 16 :top 8}
       :width 40
       :height 40
       :radius 20
       :align center
       :role button
       :label ~(t "energy.pick")
       :tooltip ~(t tip)
       :cursor arrow
       :disabled true)))

(defview period-field "The period shown, in a field that doesn't open, as only these days are kept." [id key tip]
  `(label :id ~id
     :icon calendar
     :text ~(t key)
     :textColor @text
     :width 280
     :height 48
     :padding 12
     :color @field
     :borderWidth 1
     :borderColor @border
     :radius 4
     :align {:horizontal start :vertical center}
     :role button
     :label ~(t key)
     :tooltip ~(t tip)
     :cursor arrow
     :disabled true))
