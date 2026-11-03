;;; energy.lisp --- Haven's energy page: today's use, where it came from and went, what it cost, and the week
;;;
;;; Today's use hour by hour, split between the grid and the solar panels; where the energy went, as a flow between
;;; the sun, the grid and the home; what it cost; how much of the home the sun ran, against the daily budget; and the
;;; week by day, by device or by room.

;;; What the page keeps

(defstate solar
  [0 0 0 0 0 0 0.1 0.4 0.9 1.4 1.8 2.1 2.2 2.0 1.7 1.2 0.7 0.3 0.1 0 0 0 0 0]
  "What the solar panels made today, hour by hour from midnight, in kWh."
  :type (array :items (number)))

(defstate prices {:buy 0.3 :sell 0.08} "What a kWh costs from the grid, and what one sold back earns."
  :type (object :properties {:buy (number) :sell (number)}))

;;; What it works out

(defcomputed gridHourly "What came from the grid, hour by hour: the use the sun didn't cover."
  (map-indexed (fn [index it] (max (- it (get state.solar index)) 0)) state.hourly))

(defcomputed gridIn "What came from the grid today." (sum computed.gridHourly))

(defcomputed solarMade "What the sun made today." (sum state.solar))

(defcomputed solarUsed "What the home used of it." (- (sum state.hourly) computed.gridIn))

(defcomputed gridOut "What went back to the grid." (- computed.solarMade computed.solarUsed))

(defcomputed selfUse "The share of the sun's energy the home used."
  (if (> computed.solarMade 0) (/ computed.solarUsed computed.solarMade) 0))

(defcomputed selfSufficiency "The share of the home's use the sun covered."
  (/ computed.solarUsed (max (sum state.hourly) 0.000001)))

(defcomputed sources "Each source of the day: its energy and its cost."
  [{:id "grid"
    :color "@grid"
    :kwh (fixed computed.gridIn 1)
    :cost (t "money" {:n (fixed (* computed.gridIn state.prices.buy) 2)})}
   {:id "return"
    :color "@gridOut"
    :kwh (str "−" (fixed computed.gridOut 1))
    :cost (str "−" (t "money" {:n (fixed (* computed.gridOut state.prices.sell) 2)}))}
   {:id "solar" :color "@solar" :kwh (fixed computed.solarMade 1) :cost ""}])

(defcomputed netCost "What the day cost, less what it earned."
  (t "money" {:n (fixed (- (* computed.gridIn state.prices.buy) (* computed.gridOut state.prices.sell)) 2)}))

(defcomputed flowHeight "The flow's height." (if computed.compact 172 220))

(defcomputed flowY "Where the flow's lower circles are." (- computed.flowHeight 56))

(defcomputed roomRows "The rooms table's rows: each room's energy this week and temperature."
  (map (fn [it] {:id it.name :cells [it.name (t "kwh" {:n (fixed it.kwh 1)}) (str it.temp "°")]}) state.rooms))

(defcomputed roomColumns "The rooms table's columns."
  [{:id "room" :title (t "table.room") :width 110}
   {:id "kwh" :title (t "table.energy") :width 80}
   {:id "temp" :title (t "table.now") :width 92}])

;;; The page

(defcomponent energyPage "The energy page: its bar, the day's use and flow, and the sources, the balance and the week."
  {}
  `(node :dir auto :color transparent
     ~(energy-bar)
     (row#energyTop :anchors {:left 24 :right 24 :top 72} :height ~(if computed.compact 236 300) :gap 16
       ~(usage-card)
       (node#flowCard :style card :anchors {:top 0 :bottom 0} :grow 2 :basis 0
         {:id label-flow :style cardTitle :text ~(t "flow.title")}
         (row#flowCentre :anchors {:left 0 :right 0 :top 52 :bottom 8} :justify center :alignItems center
           ~(energy-flow))))
     (row#energyBottom :anchors {:left 24 :right 24 :top "#energyTop.bottom + 16" :bottom 16} :gap 16
       (node#sourcesCard :style card :anchors {:top 0 :bottom 0} :grow 1 :basis 0
         {:id label-sources :style cardTitle :text ~(t "sources.title")}
         ~(sources-table))
       ~(balance-card)
       ~(week-card))))

(defview energy-bar "The bar: the page's title, and the day shown, with the days before and after (only today here)." []
  `(node#energyBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
     (label#energyTitle :text ~(t "page.energy")
       :font heading
       :textColor @onHeader
       :anchors {:left 24 :top 0 :bottom 0}
       :fitText true
       :align {:horizontal start :vertical center})
     ~(day-button "energyNext" "chevron-right" "energy.next" 16)
     (label#energyDay :text ~(t "energy.today") :textColor @onHeader :anchors {:right 60 :top 8} :width 72 :height 40
       :align center)
     ~(day-button "energyPrev" "chevron-left" "energy.prev" 136)
     (label#energyPick :font iconsMedium
       :icon calendar
       :text ""
       :textColor @off
       :anchors {:right 184 :top 8}
       :width 40
       :height 40
       :radius 20
       :align center
       :role button
       :label ~(t "energy.pick")
       :tooltip ~(t "energy.onlyToday")
       :cursor arrow
       :disabled true)))

(defview day-button "A button to the day before or after, off as only today is kept." [id icon key right]
  `(label :id ~id
     :font iconsMedium
     :icon ~icon
     :mirror true
     :text ""
     :textColor @off
     :anchors {:right ~right :top 8}
     :width 40
     :height 40
     :radius 20
     :align center
     :role button
     :label ~(t key)
     :tooltip ~(t "energy.onlyToday")
     :cursor arrow
     :disabled true))

;;; Today's use

(defview usage-card "Today's use hour by hour: the grid's bars over the sun's, the peak hour, and the scale." []
  `(node#usageCard :style card :anchors {:top 0 :bottom 0} :grow 3 :basis 0
     {:id label-usage :style cardTitle :text ~(t "usage.title")}
     (row#usageLegend :anchors {:right 24 :top 24} :width 220 :height 18 :gap 6 :justify end :alignItems center
       {:id legendGridSwatch :width 10 :height 10 :radius 2 :color @grid}
       {:id legendGrid :style caption :text ~(t "flow.grid")}
       {:id legendGap :width 6 :height 1 :color transparent}
       {:id legendSolarSwatch :width 10 :height 10 :radius 2 :color @solar}
       {:id legendSolar :style caption :text ~(t "flow.solar")})
     (row#usagePeak :anchors {:left 24 :top 52} :width 220 :height 18 :gap 6 :alignItems center
       {:id label-peak-hour :style caption :text ~(t "stat.peak")}
       (label#peakFigure :font small :textColor @text :fitText true :text ~(str (pad computed.peakHour 2) ":00")))
     ~(usage-scale)
     (node#usageAll :use barChart
       :with {:values ~state.hourly :top 2.6 :color @solar :gap 0.25}
       :anchors {:left 64 :right 24 :top 80 :bottom 37})
     (node#usageGrid :use barChart
       :with {:values ~computed.gridHourly :top 2.6 :color @grid :gap 0.25}
       :anchors {:left 64 :right 24 :top 80 :bottom 37}
       :role img
       :label ~(t "usage.label" {:hour (str (pad computed.peakHour 2) ":00")}))
     (row#hours :anchors {:left 64 :right 24 :bottom 10} :height 18
       ~(repeat [h ["00:00" "06:00" "12:00" "18:00" "23:00"]]
         `(label#hour :text ~h
            :font small
            :textColor @muted
            :anchors {:top 0 :bottom 0}
            :grow 1
            :align {:horizontal ~(cond (= index 0) "start" (= index 4) "end" :else "center") :vertical center})))))

(defview usage-scale "The chart's scale: a line at 1 and 2 kWh, and the axis." []
  `(node#usageAxis :anchors {:left 24 :right 24 :top 80 :bottom 36} :color transparent
     ~(repeat [v [1 2]]
       `(node#usageTick :anchors {:left 0 :right 0 :bottom 0} :height ~(str (* (/ v 2.6) 100) "%") :color transparent
          {:anchors {:left 40 :right 0 :top 0} :height 1 :color @divider}
          {:id usageTickLabel :style caption :text ~(t "kwh" {:n v}) :anchors {:left 0 :top 2}}))
     {:anchors {:left 40 :right 0 :bottom 0} :height 1 :color @divider}))

;;; Where it went

(defmacro flow-line
  ;; A macro, so that the path it writes stays one expression of the running app.
  "A line of the flow, from the sun at x to the lower circle at end."
  [x end]
  `(str "M " ~x " 96 C " ~x " " (- computed.flowY 20)
        " " ~(+ x (if (> end x) 26 -26)) " " computed.flowY
        " " ~end " " computed.flowY))

(defview energy-flow "The flow: the sun above, the grid and the home below, a line where energy went." []
  `(node#flow :dir ltr
     :color transparent
     :width 300
     :height ~computed.flowHeight
     :role img
     :label ~(t "flow.label" {:solar (fixed computed.solarMade 1) :grid (fixed computed.gridIn 1)
                              :back (fixed computed.gridOut 1)})
     (path#flowSolarHome :fill 0 :viewBox ~(str "0 0 300 " computed.flowHeight) :fit fill
       :d ~(flow-line 164 214) :stroke @solar :strokeWidth 3 :strokeCap round)
     (path#flowSolarGrid :fill 0 :viewBox ~(str "0 0 300 " computed.flowHeight) :fit fill
       :d ~(flow-line 136 86) :stroke @gridOut :strokeWidth 3 :strokeCap round)
     (path#flowGridHome :fill 0 :viewBox ~(str "0 0 300 " computed.flowHeight) :fit fill
       :d ~(str "M 86 " computed.flowY " H 214") :stroke @grid :strokeWidth 3 :strokeCap round)
     (node#flowSolar :use energyNode
       :with {:icon sun :color @solar :figure ~(t "kwh" {:n (fixed computed.solarMade 1)}) :name ~(t "flow.solar")
              :figureId solarFigure :x 102 :y 0})
     (node#flowGrid :use energyNode
       :with {:icon utility-pole :color @grid :figure ~(t "kwh" {:n (fixed computed.gridIn 1)}) :name ~(t "flow.grid")
              :figureId gridFigure :nameTop 74 :x 0 :y ~(- computed.flowHeight 92)})
     (node#flowHome :use energyNode
       :with {:icon home :color @accent :figure ~(t "kwh" {:n computed.today}) :name ~(t "flow.home")
              :figureId todayFigure :nameTop 74 :x 204 :y ~(- computed.flowHeight 92)})))

(defcomponent energyNode "A circle of the flow: an icon and a figure in a ring of colour, its name above or below."
  {:icon zap :color @accent :figure "" :name "" :figureId figure :nameTop 0 :x 0 :y 0}
  `(node :color transparent :x ~x :y ~y :width 96 :height 104
     (label :font small :textColor @muted :text ~name :anchors {:left 0 :right 0 :top ~nameTop} :height 18
       :align center)
     ~(energy-ring icon color figure figureId nameTop)))

(defview energy-ring "The circle: its icon and its figure, inside a ring of its colour."
  [icon color figure figureId nameTop]
  `(node :anchors {:top ~(if (= nameTop 0) 20 0)}
     :x 12
     :width 72
     :height 72
     :radius 36
     :color @card
     :borderWidth 3
     :borderColor ~color
     (label :icon ~icon :text "" :textColor ~color :anchors {:left 0 :right 0 :top 12} :height 22 :align center)
     (label :id ~figureId
       :font small
       :textColor @text
       :text ~figure
       :anchors {:left 0 :right 0 :top 36}
       :height 20
       :align center)))

;;; What it cost

(defview sources-table "Each source: its energy and its cost, then the day's total." []
  `(column#sources :anchors {:left 24 :right 24 :top 60}
     :alignItems stretch
     :height 192
     :role table
     :label ~(t "sources.title")
     (row#sourcesHead :height 28 :gap 8 :alignItems center
       (label#label-source :font small :textColor @muted :text ~(t "sources.source") :height 18 :grow 1 :align start)
       (label#label-source-energy :font small :textColor @muted :text ~(t "sources.energy") :width 72 :height 18
         :align end)
       (label#label-source-cost :font small :textColor @muted :text ~(t "sources.cost") :width 60 :height 18
         :align end))
     ~(repeat [s computed.sources]
       `(column#source :height 41 :alignItems stretch
          {:id sourceLine :height 1 :color @divider}
          (row#sourceRow :height 40 :gap 8 :alignItems center
            {:id sourceSwatch :width 12 :height 12 :radius 3 :color ~s.color}
            (label#sourceName :textColor @text :text ~(t (str "sources." s.id)) :height 20 :grow 1 :align start)
            (label#sourceKwh :textColor @text :text ~s.kwh :width 72 :height 20 :align end)
            (label#sourceCost :textColor @text :text ~s.cost :width 60 :height 20 :align end))))
     {:id sourcesLine :height 1 :color @divider}
     (row#sourcesTotal :height 40 :gap 8 :alignItems center
       (label#label-total-cost :font heading :textColor @text :text ~(t "sources.total") :height 22 :grow 1
         :align start)
       (label#netCost :font heading :textColor @text :text ~computed.netCost :width 100 :height 22 :align end))))

;;; The balance and the week

(defview balance-card "How much of the home the sun ran, and today's use against the daily budget." []
  `(node#gaugesCard :style card :anchors {:top 0 :bottom 0} :grow 1 :basis 0
     {:id label-balance :style cardTitle :text ~(t "gauge.title")}
     (row#gauges :anchors {:left 8 :right 8 :top 64} :height 150 :justify space-evenly
       (node#selfUseGauge :use energyGauge
         :with {:share ~computed.selfUse :color @solar :caption ~(t "gauge.selfUse")}
         :width 140)
       (node#selfSufficiencyGauge :use energyGauge
         :with {:share ~computed.selfSufficiency :color @accent :caption ~(t "gauge.selfSufficiency")}
         :width 140))
     (label#label-budget-today :font small
       :textColor @muted
       :text ~(t "budget.today" {:n computed.today})
       :anchors {:left 24 :right 24 :bottom 96}
       :height 18
       :align start)
     (row#budgetTrack :anchors {:left 24 :right 24 :bottom 76} :height 8 :radius 4 :color @raised
       {:id budgetFill
        :radius 4
        :height 8
        :basis 0
        :grow ~(clamp (/ (number computed.today) settings.energy.dailyBudget) 0.001 1)
        :color ~(if (> (number computed.today) settings.energy.dailyBudget) "@warm" "@accent")}
       {:id budgetRest :basis 0 :grow ~(- 1.001 (clamp (/ (number computed.today) settings.energy.dailyBudget) 0 1))})
     {:id budgetLine :anchors {:left 24 :right 24 :bottom 56} :height 1 :color @divider}
     (row#budget :anchors {:left 24 :right 24 :bottom 16} :height 40 :gap 8 :alignItems center
       (label#label-budget :textColor @muted :text ~(t "stat.budget") :height 20 :grow 1 :align start)
       (label#budgetFigure :font heading
         :text ~(t "kwh" {:n settings.energy.dailyBudget})
         :width 100
         :height 22
         :align end
         :textColor ~(if (> (number computed.today) settings.energy.dailyBudget) "@warm" "@text")
         :tooltip ~(t "budget.today" {:n computed.today})))))

(defcomponent energyGauge "A half-circle gauge: share (0 to 1) of the arc in colour, the percentage, and the caption."
  {:share 0 :color @accent :caption ""}
  `(node :color transparent :width 180 :height 150 :role meter :label ~(str caption ": " (round (* share 100)) "%")
     (path :anchors {:left 0 :right 0 :top 0}
       :height 100
       :viewBox "0 0 180 100"
       :d "M 14 92 A 76 76 0 0 1 166 92"
       :stroke @raised
       :strokeWidth 14
       :strokeCap round)
     (path :anchors {:left 0 :right 0 :top 0}
       :height 100
       :viewBox "0 0 180 100"
       :d "M 14 92 A 76 76 0 0 1 166 92"
       :stroke ~color
       :strokeWidth 14
       :strokeCap round
       :strokeDash ~[(max (* share 239) 0.01) 400])
     (label :font title
       :textColor @text
       :text ~(str (round (* share 100)) "%")
       :anchors {:left 0 :right 0 :top 52}
       :height 40
       :align center)
     (label :font small
       :textColor @muted
       :text ~caption
       :anchors {:left 0 :right 0 :top 104}
       :height 40
       :wrap true
       :align {:horizontal center :vertical start})))

(defview week-card "The week by day or by device as bars, or by room as a table." []
  `(node#weekCard :style card :anchors {:top 0 :bottom 0} :grow 1 :basis 0
     {:id label-week-caption
      :style cardTitle
      :text ~(cond (= state.energyView "day") (t "week.byDay")
                   (= state.energyView "device") (t "week.byDevice")
                   :else (t "byRoom.title"))}
     (label#label-this-week :font heading
       :textColor @text
       :text ~(t "kwh" {:n computed.energyTotal})
       :anchors {:right 24 :top 16}
       :width 120
       :height 22
       :align end)
     ~(week-choices)
     (node#weekBars :when ~(not= state.energyView "room")
       :use barChart
       :with {:values ~(if (= state.energyView "day") computed.weekValues computed.deviceValues)
              :color ~(if (= state.energyView "day") "@accent" "@warm")
              :gap 0.4}
       :anchors {:left 24 :right 24 :top 92 :bottom 36}
       :role img
       :label ~(if (= state.energyView "day") (t "week.byDay") (t "week.byDevice")))
     ~(week-labels)
     (table#roomTable :when ~(= state.energyView "room")
       :anchors {:left 12 :right 12 :top 92 :bottom 12}
       :rowHeight 30
       :color @card
       :items ~computed.roomRows
       :columns ~computed.roomColumns
       :value ~(get-in state.rooms [state.room :name])
       :label ~(t "byRoom.title")
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @selected
       :accentColor @accent
       :onChange ~(handler pickRoomNamed event.value)
       :onOpen {:set state.page :to home})))

(defview week-choices "What the week is by: day, device or room." []
  `(row#energyViews :anchors {:left 24 :right 24 :top 52} :height 28 :gap 4 :role toolbar :arrows horizontal
     ~(week-choice "energyByDay" "day" "week.dayButton")
     ~(week-choice "energyByDevice" "device" "week.deviceButton")
     ~(week-choice "energyByRoom" "room" "week.roomButton")))

(defview week-choice "A choice of what the week is by, filled while it is the one shown." [id view key]
  `{:id ~id
    :style ~(if (= state.energyView view) "choiceOn" "choice")
    :font small
    :text ~(t key)
    :grow 1
    :basis 0
    :anchors {:top 0 :bottom 0}
    :selected ~(= state.energyView view)
    :onClick ~(handler showEnergyView view)})

(defview week-labels "The bars' labels: the days, the peak in its colour, or the devices." []
  `(row#weekDays :when ~(not= state.energyView "room") :anchors {:left 24 :right 24 :bottom 10} :height 18
     ~(repeat [d data.energy]
       `(label#weekDay :when ~(= state.energyView "day")
          :text ~(t (str "day." d.day))
          :font small
          :textColor ~(if (= d.day computed.peakDay) "@warm" "@muted")
          :anchors {:top 0 :bottom 0}
          :grow 1
          :basis 0
          :align center))
     ~(repeat [d state.deviceEnergy]
       `(label#weekDevice :when ~(= state.energyView "device")
          :text ~(t (str "use." d.id))
          :font small
          :tooltip ~(str (t (str "use." d.id)) ": " (t "kwh" {:n d.kwh}))
          :textColor @muted
          :anchors {:top 0 :bottom 0}
          :grow 1
          :basis 0
          :align center
          :fitText false))))
