;;; picture.lisp --- Cropping a picture
;;;
;;; A picture's `crop` is how much of each side is cut off, as fractions of the whole picture: left, top, right,
;;; bottom. The object's box shows what is left, at the picture's own scale; the whole picture is drawn under a clip.
;;; The object's menu crops the selected pictures to a square, 4:3 or 16:9 (around their middle), or gives back the
;;; whole picture.

(ns sinopia.parts.picture)

;;; Definitions

(def crop-ratios "The proportions the object's menu crops to, with their names." [[1 "crop.square"] [(/ 4 3) "crop.standard"]
                                                                                 [(/ 16 9) "crop.wide"]])

(defn whole-size "The size the whole picture is drawn at, from its box and its crop." [o]
  (let [[left top right bottom] (or-else (:crop o) [0 0 0 0])]
    {:w (/ (:w o) (max (- 1 left right) 0.01))
     :h (/ (:h o) (max (- 1 top bottom) 0.01))}))

(defn cropped
  "Picture `o` cropped to the proportions `ratio` around its middle (nil: the whole picture), its box at the same
  scale and centred where it was."
  [o ratio]
  (let [whole (whole-size o)
        wide (/ (:w whole) (max (:h whole) 0.01))
        cut-x (if (and ratio (> wide ratio)) (/ (- 1 (/ ratio wide)) 2) 0)
        cut-y (if (and ratio (< wide ratio)) (/ (- 1 (/ wide ratio)) 2) 0)
        w (* (:w whole) (- 1 (* 2 cut-x)))
        h (* (:h whole) (- 1 (* 2 cut-y)))
        cx (+ (:x o) (/ (:w o) 2))
        cy (+ (:y o) (/ (:h o) 2))]
    (assoc o
           :crop (if ratio [cut-x cut-y cut-x cut-y] nil)
           :w (round w)
           :h (round h)
           :x (round (- cx (/ w 2)))
           :y (round (- cy (/ h 2))))))

(defcomputed pictureSelected "Whether a picture is among the selected objects, so the object's menu offers its crops."
  (some #(= (:kind %) "picture") computed.selected))

(defcomputed cropItems "The object's menu's crops, with a picture selected: the proportions, the whole picture."
  (if computed.pictureSelected
    (concat (map (fn [it] {:title (t it.1) :icon "crop" :run "crop-pictures" :with {:ratio it.0}}) crop-ratios)
            [{:title (t "crop.reset") :icon "image" :run "crop-pictures" :with {:ratio nil}} {:separator true}])
    []))

;;; What its commands do

(defn crop-pictures "Crops the selected pictures to proportions (nil: gives back the whole picture)." [ratio]
  (when computed.pictureSelected
    (commit)
    (set-objects (map #(if (and (selected? %) (= (:kind %) "picture")) (cropped % ratio) %) computed.objects))))
