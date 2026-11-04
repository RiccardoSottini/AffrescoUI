;;; charts.lisp --- Line, bar and area charts drawn with path nodes, to include in a screen ("include":
;;; ["charts.json"])
;;;
;;; Each takes a list of numbers (values) and draws it from 0 at the bottom to top (default: the largest value) at the
;;; top, stretched to the node. Give the use a width and a height, and an id.

;;; Definitions

(defcomponent lineChart "A line through the values, from 0 at the bottom to the top value at the top."
  {:values [] :top 0 :color "#2563eb" :lineWidth 2}
  `(path :width 240
     :height 120
     :viewBox ~(str "0 0 " (max (- (count values) 1) 1) " 100")
     :fit fill
     :d ~(if (empty? values)
             ""
             (str "M "
                  (join " L "
                        (map-indexed (fn [index it]
                                       (str index " " (- 100 (/ (* it 100) (max (or top (max values)) 0.000001)))))
                                     values))))
     :stroke ~color
     :strokeWidth ~lineWidth
     :strokeJoin round
     :strokeCap round))

(defcomponent areaChart "The area under the values, from 0 at the bottom to the top value at the top."
  {:values [] :top 0 :color "#2563eb80"}
  `(path :width 240
     :height 120
     :viewBox ~(str "0 0 " (max (- (count values) 1) 1) " 100")
     :fit fill
     :d ~(if (empty? values)
             ""
             (str "M 0 100 L "
                  (join " L "
                        (map-indexed (fn [index it]
                                       (str index " " (- 100 (/ (* it 100) (max (or top (max values)) 0.000001)))))
                                     values))
                  " L "
                  (- (count values) 1)
                  " 100 Z"))
     :fillColor ~color))

(defcomponent barChart "A bar for each value, a gap between them, from 0 at the bottom to the top value at the top."
  {:values [] :top 0 :color "#2563eb" :gap 0.3}
  `(path :width 240
     :height 120
     :viewBox ~(str "0 0 " (max (count values) 1) " 100")
     :fit fill
     :d ~(join " "
               (map-indexed (fn [index it]
                              (str "M "
                                   (+ index (/ gap 2))
                                   " 100 V "
                                   (- 100 (/ (* it 100) (max (or top (max values)) 0.000001)))
                                   " H "
                                   (- (+ index 1) (/ gap 2))
                                   " V 100 Z"))
                            values))
     :fillColor ~color))
