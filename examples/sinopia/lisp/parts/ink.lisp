;;; ink.lisp --- Ink Replay and Ink to Shape
;;;
;;; Ink Replay draws the slide's ink strokes again, one after another, each from its first point to its last. Ink to
;;; Shape, while it is on, turns each closed stroke drawn into a shape: a rectangle when it reaches three corners of the
;;; box around it or more, else an ellipse; an open stroke stays ink.

(ns sinopia.parts.ink)

;;; Definitions

(defstate inkReplay {:slide nil :clock 0 :n 0} "The ink replay: its slide, the strokes drawn so far, and of how many.")

(defstate inkToShape false "Whether a closed stroke drawn becomes a shape.")

(defcomputed inkIds "The ids of the slide's ink strokes, in the order they were drawn."
  (map :id (filter #(= (:kind %) :ink) computed.objects)))

(defcomputed replaying "Whether the ink replay is drawing this slide's strokes."
  (and (= state.inkReplay.slide computed.slide.id) (< state.inkReplay.clock state.inkReplay.n)))

;;; Replaying

(defn replayed-path "Ink object `o`'s path as the replay has drawn it so far: its first points, or all of them."
  [o]
  (let [path (or-else (:d o) "")
        at (indexOf computed.inkIds (:id o))
        done (clamp (- state.inkReplay.clock at) 0 1)
        points (str/split path " L ")
        part (ceil (* done (count points)))]
    (cond (not computed.replaying) path
          (neg? at) path
          (zero? done) ""
          :else (join " L " (take (max 2 part) points)))))

(defn replay-ink "Draws the slide's ink strokes again, one after another, 0.7 seconds each." []
  (let [n (count computed.inkIds)]
    (when (pos? n)
      (set "state.inkReplay" {:slide computed.slide.id :clock 0 :n n})
      (animate "state.inkReplay.clock" :to n :ms (* n 700) :easing "linear"))))

;;; Ink to Shape

(defn distance "How far apart two points are." [[x0 y0] [x1 y1]]
  (hypot (- x1 x0) (- y1 y0)))

(defn reaches? "Whether a stroke passes within a distance of a point." [stroke point reach]
  (some #(< (distance % point) reach) stroke))

(defn stroke-shape
  "The shape a stroke in a box looks like: nil when it is open or small, rect when it reaches three corners or more,
  else ellipse."
  [stroke box]
  (let [{:keys [left top right bottom]} box
        size (max (- right left) (- bottom top))
        reach (* 0.12 (min (- right left) (- bottom top)))
        gap (distance (first stroke) (last stroke))
        corners (count (filter #(reaches? stroke % reach) [[left top] [right top] [right bottom] [left bottom]]))]
    (cond (< size 16) nil
          (> gap (* 0.3 size)) nil
          (>= corners 3) "rect"
          :else "ellipse")))

(defn add-stroke "Adds the stroke drawn: a shape, when Ink to Shape is on and it looks like one, else ink."
  [left top right bottom]
  (if-some [shape (when state.inkToShape
                    (stroke-shape state.stroke {:left left :top top :right right :bottom bottom}))]
    (do (insertObject {:kind "shape" :shape shape :x left :y top :w (- right left) :h (- bottom top) :fill "none"
                       :line computed.strokeColor :lineWidth computed.strokeWidth :text ""})
        (set "state.board.selection" []))
    (add-ink left top right bottom)))

;;; Views

(defview g-ink-convert "Ink to Shape, and Ink Replay." []
  `(node#g-ink-convert :use rbGroup :with {:title ~(t "pane.convert")}
     (node#b-ink-shape :use rbBig
       :with {:icon shapes :title ~(t "pane.inkToShape") :on ~state.inkToShape :width 46}
       :tooltip ~(t "ink.turnClosedStrokesIntoShapes")
       :onClick [{:toggle state.inkToShape}])
     (node#b-ink-replay :use rbBig
       :with {:icon history
              :title ~(t "pane.inkReplay")
              :on ~computed.replaying
              :enabled ~(pos? computed.slideInk)
              :width 46}
       :tooltip ~(t "ink.drawTheStrokesAgain")
       :onClick replay-ink)))
