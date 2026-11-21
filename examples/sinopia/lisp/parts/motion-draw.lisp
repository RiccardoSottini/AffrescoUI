;;; motion-draw.lisp --- Motion paths drawn by hand: Motion > Custom Path, then a drag on the slide draws the path the
;;; selected objects follow in the show, from where they are
;;;
;;; The path is kept in the build as up to 48 points, each an offset from the first in slide points; the show moves the
;;; object along them (path-offset in motion.lisp). Escape gives up the drawing.

(ns sinopia.parts.motion-draw)

;;; Definitions

(defstate pathDrawing false "Whether a drag on the slide draws a custom motion path.")

(defstate pathPoints [] "The points of the motion path being drawn, in slide points.")

(defcomputed livePath "The motion path being drawn, as a path on the canvas."
  (str "M " (join " L " (map #(canvas-point %) state.pathPoints))))

(defcomponent motionPathLayer "Over the slide while a custom path is drawn: a press, a drag and a release draw it." {}
  `(node#motion-path-layer :fill 0
     :color "#0e7c6b10"
     :role region
     :label ~(t "motion.drawThePath")
     :focusable true
     :autofocus true
     :keys {:escape stop-drawing-path}
     :onPress ~(handler path-press (+ computed.view.camera.x (/ event.left computed.view.camera.zoom))
                                   (+ computed.view.camera.y (/ event.top computed.view.camera.zoom)))
     :onDrag ~(handler path-drag (/ event.dx computed.view.camera.zoom) (/ event.dy computed.view.camera.zoom))
     :onRelease path-release
     (label#motion-path-hint :anchors {:left 12 :top 12}
       :fitText true
       :padding 8
       :radius 8
       :color @card
       :borderWidth 1
       :borderColor @rbLine
       :textColor @rbInk
       :runs [{:icon spline :size 13} {:text ~(str " " (t "motion.dragToDrawThePath")) :size 12}])
     (path#motion-path-live :when ~(> (count state.pathPoints) 1)
       :fill 0
       :d ~computed.livePath
       :stroke "#0e7c6b"
       :strokeWidth 2
       :strokeDash [6 4]
       :strokeCap round
       :strokeJoin round)))

;;; The drawn path on the slide

(defcomputed drawnPath
  "The selected object's drawn motion path on the Motion tab, as path data from its centre in slide points, or nil."
  (let [a computed.animation
        o computed.first
        ph computed.firstPlaceholder
        w (or-else o.w ph.w 0)
        h (or-else o.h ph.h 0)
        x (+ (or-else o.x ph.x 0) (/ w 2))
        y (+ (or-else o.y ph.y 0) (/ h 2))
        points (or-else (:points a) [])]
    (when (and (= state.tab "animations") (= (:effect a) "pathCustom") (> (count points) 1))
      (str "M " (join " L " (map #(path-point x y %) points))))))

(defn path-point "A point of a drawn path, an offset from the centre at x and y, as path data." [x y [dx dy]]
  (str (+ x dx) " " (+ y dy)))

(defview motion-path-line "The selected object's drawn motion path, under the objects, while the Motion tab is shown." []
  `(path :when ~(some? computed.drawnPath)
     :x 0
     :y 0
     :width 960
     :height 540
     :viewBox "0 0 960 540"
     :fit fill
     :d ~(or-else computed.drawnPath "")
     :stroke "#0e7c6b"
     :strokeWidth 2
     :strokeDash [6 4]
     :strokeJoin round))

;;; What a drag does

(defn canvas-point "A point of the slide where the canvas shows it, as path data." [[x y]]
  (let [cx (* (- x computed.view.camera.x) computed.view.camera.zoom)
        cy (* (- y computed.view.camera.y) computed.view.camera.zoom)]
    (str (fixed cx 1) " " (fixed cy 1))))

(defn along "The point the part `p` of the way along points (offsets from the first), between the two around it."
  [points p]
  (let [n (count (or-else points []))
        at (* (clamp p 0 1) (max (dec n) 0))
        i (min (floor at) (max (- n 2) 0))
        [x0 y0] (get points i [0 0])
        [x1 y1] (get points (inc i) [x0 y0])
        f (- at i)]
    (if (< n 2) [0 0] [(+ x0 (* f (- x1 x0))) (+ y0 (* f (- y1 y0)))])))

(defn start-drawing-path "Waits for a drag on the slide that draws the selected objects' motion path." []
  (when (seq computed.selected)
    (set "state.pathPoints" [])
    (set "state.pathDrawing" true)))

(defn stop-drawing-path "Gives up the drawing." []
  (set "state.pathPoints" [])
  (set "state.pathDrawing" false))

(defn path-press "Starts the path at a point of the slide." [x y]
  (set "state.pathPoints" [[x y]]))

(defn path-drag "Goes on with the path, at the press moved by dx and dy, in slide points." [dx dy]
  (let [[x y] (first state.pathPoints)]
    (when (< (count state.pathPoints) 2000)
      (push "state.pathPoints" [(+ x dx) (+ y dy)]))))

(defn path-offsets "Up to 48 of the drawn points, the last kept, each as an offset from the first, rounded." [points]
  (let [[x0 y0] (first points)
        share (/ (count points) 47)
        step (max 1 (ceil share))
        kept (concat (take-nth step points) [(last points)])]
    (vec (map (fn [[x y]] [(round (- x x0)) (round (- y y0))]) kept))))

(defn path-release "Ends the drawing: a path of two points or more becomes the selected objects' build." []
  (let [points state.pathPoints]
    (stop-drawing-path)
    (when (> (count points) 1)
      (choose-build "pathCustom")
      (change-animation {:points (path-offsets points)}))))
