;;; canvas.lisp --- the canvas's guides and gridlines
;;;
;;; With View > Guides on, the canvas draws the deck's guides, or the slide's centre lines when it has none; objects
;;; snap to them as they move, as they do to the grid. A guide dragged out of a ruler turns the guides on.

(ns sinopia.parts.canvas)

(defcomputed centreGuides "The guides through the slide's centre." {:vertical [480] :horizontal [270]})

(defcomputed guidesShown "The guides the canvas draws: none while Guides is off, else the deck's or the centre lines."
  (let [own state.board.guides
        many (+ (count (or-else own.vertical [])) (count (or-else own.horizontal [])))]
    (cond (not state.guides) {:vertical [] :horizontal []}
          (pos? many) own
          :else computed.centreGuides)))

(defn guides-dragged "Turns the guides on when the canvas's view brings one dragged out of a ruler." [view]
  (let [many (+ (count (or-else view.guides.vertical [])) (count (or-else view.guides.horizontal [])))]
    (when (and (not state.guides) (pos? many))
      (set "state.guides" true))))

;;; The gridlines

(defcomputed gridPath "The gridlines over the 960 by 540 slide, every 12 points, as one path."
  (str (join "" (map #(str "M" (* 12 %) " 0V540") (range 1 80)))
       (join "" (map #(str "M0 " (* 12 %) "H960") (range 1 45)))))

(defview grid-lines "The gridlines over the slide, under its objects, while View > Gridlines is on." []
  `(path :when ~state.gridlines
     :x 0
     :y 0
     :width 960
     :height 540
     :viewBox "0 0 960 540"
     :fit fill
     :d ~computed.gridPath
     :stroke "#8090a040"
     :strokeWidth 0.5))
