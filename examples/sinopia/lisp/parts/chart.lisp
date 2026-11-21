;;; chart.lisp --- Insert > Chart: a chart object drawn from a small table of data
;;;
;;; A chart is an object of kind `chart` whose `cells` are rows of texts, like a table's: the first row names the
;;; series, the first column the categories, and the other cells are numbers. Its `chart` is how it is drawn: columns,
;;; bars, a line or a pie (the first series only). Insert > Chart offers the four; a double click shows the data as a
;;; table to edit, with Tab and Shift+Tab, and the chart is drawn again when the edit ends.

(ns sinopia.parts.chart)

;;; Definitions

(def chart-kinds "The charts Insert > Chart offers, with their icons." [["column" "chart-column-decreasing"] ["bar" "chart-bar-decreasing"]
                                                                         ["line" "chart-line"] ["pie" "chart-pie"]])

(def steps "The steps the value axis may take, from small to large."
  [0.1 0.2 0.5 1 2 2.5 5 10 20 25 50 100 200 250 500 1000 2000 2500 5000 10000 20000 50000 100000])

(defn accent "The theme colour of series `n` (from 0): the six accents, in turn." [n]
  (let [k (inc (mod n 6))]
    (get computed.palette (str "accent" k))))

(defn chart-data
  "What a chart's cells hold: the series' names, the categories, the values (one row of numbers a category) and the
  axis: its step and its top, a whole number of steps over the largest value."
  [cells]
  (let [head (or-else (first cells) [])
        rows (rest (or-else cells []))
        amount #(max (or-else (number %) 0) 0)
        values (mapv #(mapv amount (rest %)) rows)
        most (reduce max 0 (mapcat identity values))
        step (or-else (first (filter #(>= (* % 5) most) steps)) 100000)
        whole (max (ceil (/ most step)) 1)]
    {:series (vec (rest head))
     :cats (mapv #(or-else (first %) "") rows)
     :n (max (count rows) 1)
     :values values
     :step step
     :top (* step whole)}))

(defn chart-bars
  "The columns (or, `across`, the bars) of chart data `d` in plot area `a`: a band a category, the series side by side."
  [d a across]
  (let [k (max (count d.series) 1)
        band (/ (if across a.h a.w) d.n)
        thick (/ (* band 0.7) k)]
    (vec (for [[i row] (map-indexed vector d.values)
               [j v] (map-indexed vector row)
               :when (< j k)]
           (let [along (+ (* i band) (* band 0.15) (* j thick))
                 long (* (if across a.w a.h) (/ v d.top))]
             (if across
               {:id (str i "-" j) :x a.x :y (+ a.y along) :w long :h (max (- thick 2) 1) :color (accent j)}
               {:id (str i "-" j)
                :x (+ a.x along)
                :y (- (+ a.y a.h) long)
                :w (max (- thick 2) 1)
                :h long
                :color (accent j)}))))))

(defn chart-lines "The lines of chart data `d` in plot area `a`: one path a series, through each category's middle."
  [d a]
  (let [band (/ a.w d.n)
        point (fn [i v]
                (let [x (+ (* i band) (/ band 2))
                      y (- a.h (/ (* a.h v) d.top))]
                  (str (fixed x 2) " " (fixed y 2))))]
    (vec (for [j (range (count d.series))]
           (let [points (map-indexed #(point %1 (or-else (get %2 j) 0)) d.values)]
             {:id (str j) :color (accent j) :d (str "M" (str/join "L" points))})))))

(defn arc-point "The point at `turn` (0 to 1, clockwise from the top) on a circle round `cx` `cy` of radius `r`."
  [cx cy r turn]
  (let [angle (* turn 2 3.14159265)
        x (+ cx (* r (sin angle)))
        y (- cy (* r (cos angle)))]
    (str (fixed x 2) " " (fixed y 2))))

(defn chart-slices "The slices of a pie of the first series of chart data `d`, in plot area `a`." [d a]
  (let [values (mapv #(or-else (first %) 0) d.values)
        total (reduce + 0 values)
        r (/ (min a.w a.h) 2)
        cx (/ a.w 2)
        cy (/ a.h 2)
        starts (vec (reductions + 0 values))]
    (vec (for [[i v] (map-indexed vector values)
               :when (pos? v)]
           (let [from (/ (get starts i) (max total 1))
                 part (/ v (max total 1))]
             {:id (str i)
              :color (accent i)
              :d (if (>= part 0.9999)
                   (str "M" (arc-point cx cy r 0) "A" r " " r " 0 1 1 " (arc-point cx cy r 0.5) "A" r " " r " 0 1 1 "
                        (arc-point cx cy r 0) "Z")
                   (str "M" cx " " cy "L" (arc-point cx cy r from) "A" r " " r " 0 " (if (> part 0.5) 1 0) " 1 "
                        (arc-point cx cy r (+ from part)) "Z"))})))))

(defn chart-ticks "The value axis's gridlines of chart data `d` in plot area `a` (across: the bars' axis runs along x)."
  [d a across]
  (let [many (inc (round (/ d.top d.step)))
        bottom (+ a.y a.h)]
    (vec (for [s (range many)]
           (let [part (/ (* s d.step) d.top)
                 tenths (round (* s d.step 10))]
             {:id (str s)
              :text (text (/ tenths 10))
              :x (if across (+ a.x (* a.w part)) a.x)
              :y (if across bottom (- bottom (* a.h part)))})))))

(defn chart-cats "Where the category names go: under the columns, or left of the bars (`across`)." [d a across]
  (let [band (/ (if across a.h a.w) d.n)]
    (vec (map-indexed (fn [i name]
                        (if across
                          {:id (str i) :text (text name) :x 0 :y (+ a.y (* i band)) :w 88 :h band}
                          {:id (str i) :text (text name) :x (+ a.x (* i band)) :y (+ a.y a.h 4) :w band :h 20}))
                      d.cats))))

(defn chart-series-names "The series' names of a chart's cells, in a list for its label." [cells]
  (->> (or-else (first cells) [])
       rest
       (str/join ", ")))

(defn chart-legend "The legend's entries: the series, or a pie's categories." [d pie]
  (vec (map-indexed (fn [i name] {:id (str i) :text (or-else name "") :color (accent i)})
                    (if pie d.cats d.series))))

;;; Views

(defcomputed chartSelected "The kind of the first selected chart, if a chart is selected, so the Chart menu changes it."
  (->> computed.selected
       (filter #(= (:kind %) "chart"))
       first
       :chart))

(defcomputed chartId "The first selected chart's id, whose data Edit Data opens."
  (->> computed.selected
       (filter #(= (:kind %) "chart"))
       first
       :id))

(defcomputed menu-b-ins-chart
  "The Chart menu: a chart of each kind, or, with a chart selected, its kind changed and its data to edit."
  (concat (map (fn [it]
                 {:title (t (str "chart." it.0))
                  :icon it.1
                  :checked (= computed.chartSelected it.0)
                  :run (if computed.chartSelected "set-chart-kind" "insert-chart")
                  :with {:kind it.0}})
               chart-kinds)
          (if computed.chartSelected
            [{:separator true} {:title (t "chart.editData") :icon "table" :run "edit-chart-data" :with {:id computed.chartId}}]
            [])))

(defcomponent slideChart
  "A chart object, in a box `w` by `h`: its legend, axis and gridlines, then its columns, bars, line or pie. While it
  is edited, the slide shows its data as a table instead."
  {:o nil :prefix "" :w 100 :h 100}
  `(node :id ~(str prefix o.id "-chart")
     :role img
     :label ~(t (str "chart.described." (or-else o.chart "column")) {:series (chart-series-names o.cells)})
     :x 0
     :y 0
     :width ~w
     :height ~h
     :let {:d ~(chart-data o.cells)
           :kind ~(or-else o.chart "column")
           :across ~(= (or-else o.chart "column") "bar")
           :area ~{:x (if across 96 44) :y 36 :w (max (- w (if across 112 60)) 10) :h (max (- h 68) 10)}
           :pie ~(= (or-else o.chart "column") "pie")
           :ink ~(or-else computed.palette.dark2 computed.palette.dark1)}
     :color ~(get computed.palette "light1")
     :borderWidth 1
     :borderColor ~(get computed.palette "light2")
     (row :id ~(str prefix o.id "-legend")
       :x 12
       :y 8
       :width ~(- w 24)
       :height 18
       :gap 14
       :justify center
       :color transparent
       ~(repeat [e (chart-legend d pie)]
         `(row :id ~(str prefix o.id "-legend-" e.id) :fitContent width :height 18 :gap 5 :color transparent
            (node :id ~(str prefix o.id "-legend-" e.id "-key") :width 10 :height 10 :radius 2 :color ~e.color)
            (label :id ~(str prefix o.id "-legend-" e.id "-t")
              :fitText true
              :height 18
              :textColor ~ink
              :runs [{:text ~e.text :size 14}]))))
     ~(repeat [g (if pie [] (chart-ticks d area across))]
       `(node :id ~(str prefix o.id "-grid-" g.id)
          :x ~(if across g.x area.x)
          :y ~(if across area.y g.y)
          :width ~(if across 1 area.w)
          :height ~(if across area.h 1)
          :color ~(get computed.palette "light2")))
     ~(repeat [g (if pie [] (chart-ticks d area across))]
       `(label :id ~(str prefix o.id "-tick-" g.id)
          :x ~(if across (- g.x 20) 0)
          :y ~(if across (+ g.y 4) (- g.y 9))
          :width ~(if across 40 38)
          :height 18
          :align {:horizontal ~(if across "center" "end") :vertical center}
          :textColor ~ink
          :runs [{:text ~g.text :size 12}]))
     ~(repeat [b (if (contains? #{"column" "bar"} kind) (chart-bars d area across) [])]
       `(node :id ~(str prefix o.id "-bar-" b.id) :x ~b.x :y ~b.y :width ~b.w :height ~b.h :color ~b.color))
     ~(repeat [c (if pie [] (chart-cats d area across))]
       `(label :id ~(str prefix o.id "-cat-" c.id)
          :x ~c.x
          :y ~c.y
          :width ~c.w
          :height ~c.h
          :align {:horizontal ~(if across "end" "center") :vertical center}
          :textColor ~ink
          :runs [{:text ~c.text :size 14}]))
     (node :id ~(str prefix o.id "-plot")
       :when ~(contains? #{"line" "pie"} kind)
       :x ~area.x
       :y ~area.y
       :width ~area.w
       :height ~area.h
       :color transparent
       ~(repeat [l (if (= kind "line") (chart-lines d area) [])]
         `(path :id ~(str prefix o.id "-line-" l.id)
            :x 0
            :y 0
            :width ~area.w
            :height ~area.h
            :viewBox ~(str "0 0 " area.w " " area.h)
            :d ~l.d
            :fillColor transparent
            :stroke ~l.color
            :strokeWidth 3
            :strokeCap round
            :strokeJoin round))
       ~(repeat [s (if pie (chart-slices d area) [])]
         `(path :id ~(str prefix o.id "-slice-" s.id)
            :x 0
            :y 0
            :width ~area.w
            :height ~area.h
            :viewBox ~(str "0 0 " area.w " " area.h)
            :d ~s.d
            :fillColor ~s.color
            :stroke ~(get computed.palette "light1")
            :strokeWidth 2)))))

;;; What its commands do

(defn sample-cells "The data a new chart starts with: three series over four categories, to be written over." []
  [["" (t "chart.series" {:n 1}) (t "chart.series" {:n 2}) (t "chart.series" {:n 3})]
   [(t "chart.category" {:n 1}) "4.3" "2.4" "2"]
   [(t "chart.category" {:n 2}) "2.5" "4.4" "2"]
   [(t "chart.category" {:n 3}) "3.5" "1.8" "3"]
   [(t "chart.category" {:n 4}) "4.5" "2.8" "5"]])

(defn insert-chart "Puts a chart of the kind given in the middle of the slide, with sample data, and selects it." [kind]
  (set "state.drop.open" false)
  (insertObject {:kind "chart" :chart kind :x 180 :y 110 :w 600 :h 340 :cells (sample-cells)}))

(defn set-chart-kind "Draws the selected charts as another kind: columns, bars, a line or a pie." [kind]
  (set "state.drop.open" false)
  (when (some #(= (:kind %) "chart") computed.selected)
    (commit)
    (set-objects (map #(if (and (selected? %) (= (:kind %) "chart")) (assoc % :chart kind) %) computed.objects))))

(defn edit-chart-data "Opens a chart's data as a table, its first value in an editor." [id]
  (set "state.drop.open" false)
  (edit-cell id 1 1))
