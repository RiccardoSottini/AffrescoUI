;;; animation.lisp --- keyframes, playback and markers
;;;
;;; A node keeps its keyframes, `{frame, translation, rotationDegrees, scale}` sorted by frame; between two of them it
;;; is posed by easing from one to the other. The timeline plays at 24 frames a second (`tick`, from #playback).

;;; Keyframes

(defn key-of "A node's keyframe at the current frame." [n]
  {:frame state.frame :translation (:translation n) :rotationDegrees (:rotationDegrees n) :scale (:scale n)})

(defn keySelected "Keys the selected objects where they are, at the current frame." []
  (change-nodes selected?
                (fn [n] (assoc n "keyframes"
                               (sort-by :frame (concat (remove (fn [k] (= (:frame k) state.frame)) (or (:keyframes n) []))
                                                       [(key-of n)]))))))

(defn autoKey "Keys the selected objects after a change, while Auto Keying is on." []
  (when (= state.autoKey true) (keySelected)))

(defcommand insertKeyframe "Keys the selected objects at the current frame."
  :title ~(t "command.insertKeyframe") :category ~(t "category.animation") :icon "diamond"
  :enablement ~(> (count state.view.selection) 0)
  []
  (dismiss)
  (snapshot)
  (keySelected)
  (set "state.last" (t "command.insertKeyframe")))

(defn change-keys "Changes the keyframes of the selected objects that have some." [change]
  (change-nodes (fn [n] (and (selected? n) (some? (:keyframes n))))
                (fn [n] (merge n {:keyframes (change (:keyframes n))}))))

(defcommand deleteKeyframe "Takes the selected objects' keyframes at the current frame away."
  :title ~(t "command.deleteKeyframe") :category ~(t "category.animation") :icon "diamond-minus"
  :enablement ~(contains computed.keyFrames state.frame)
  []
  (dismiss)
  (when (contains computed.keyFrames state.frame)
    (snapshot)
    (change-keys (fn [keyframes] (remove (fn [k] (= (:frame k) state.frame)) keyframes)))
    (set "state.last" (t "command.deleteKeyframe"))))

(defcommand clearKeyframes "Takes every keyframe of the selected objects away."
  :title ~(t "command.clearKeyframes") :category ~(t "category.animation")
  :enablement ~(> (count computed.keyFrames) 0)
  []
  (dismiss)
  (when (not (empty? computed.keyFrames))
    (snapshot)
    (change-keys (fn [keyframes] []))
    (set "state.last" (t "command.clearKeyframes"))))

(defn frame-at "The frame at a time in seconds." [time]
  (+ (round (* time 24)) 1))

(defn moveKeyframe "A keyframe dragged on the timeline: the selected objects' keys at its frame move to the new one." []
  (let [from (frame-at event.from)
        to (frame-at event.time)]
    (when (and (not= from to) (not (contains computed.keyFrames to)) (contains computed.keyFrames from))
      (when (not= state.keyDragAt from) (snapshot))
      (set "state.keyDragAt" to)
      (change-keys (fn [keyframes] (sort-by :frame (map (fn [k] (if (= (:frame k) from) (merge k {:frame to}) k)) keyframes))))
      (poseFrame))))

;;; Posing between keyframes

(defn eased "How far between two keyframes a frame is, eased in and out." [a b]
  (let [u (if (= (:frame b) (:frame a)) 0 (/ (- state.frame (:frame a)) (- (:frame b) (:frame a))))]
    (* u u (- 3 (* 2 u)))))

(defn between "A list of a keyframe's, item by item, a part of the way to the next one's, rounded." [from to u scale]
  (map-indexed (fn [i v] (round-to (+ v (* (- (get to i) v) u)) scale)) from))

(defn posed "A node with keyframes, posed at the current frame." [n]
  (let [keyframes (:keyframes n)
        a (or (find (fn [k] (<= (:frame k) state.frame)) (reverse keyframes)) (first keyframes))
        b (or (find (fn [k] (>= (:frame k) state.frame)) keyframes) (last keyframes))
        u (eased a b)]
    (merge n {:translation (between (:translation a) (:translation b) u 10000)
              :rotationDegrees (between (:rotationDegrees a) (:rotationDegrees b) u 1000)
              :scale (between (:scale a) (:scale b) u 10000)})))

(defn poseFrame "Poses every node that has keyframes at the current frame." []
  (change-nodes (fn [n] (not (empty? (or (:keyframes n) [])))) posed))

;;; The current frame and the range

(defn setFrame "Goes to a frame." [value]
  (set "state.frame" (round (clamp value 0 1048574)))
  (poseFrame))

(defn setRange "The start or the end of the range: of the preview range while it is used." [key value]
  (let [frame (round (clamp value 0 1048574))]
    (if (= key "start")
      (set (str "state." (first computed.rangeKeys)) (min frame (second computed.range)))
      (set (str "state." (second computed.rangeKeys)) (max frame (first computed.range))))))

(defn setFrameField "A field of the timeline's header: the current frame, the start or the end." [key value]
  (if (= key "frame") (setFrame value) (setRange key value)))

(defcommand togglePreviewRange "Plays a range of its own, the whole range to start with, or the scene's again."
  :title ~(t "command.togglePreviewRange") :category ~(t "category.playback") :icon "clock-3"
  []
  (dismiss)
  (set "state.previewRange" (when (nil? state.previewRange) {:start state.start :end state.end})))

;;; Playing

(defcommand play "Plays the animation, or stops it."
  :title ~(t "command.play") :category ~(t "category.playback") :icon "play" []
  (dismiss)
  (set "state.playing" (if (not= state.playing "forward") "forward" false)))

(defcommand playReverse "Plays the animation backwards, or stops it."
  :title ~(t "command.playReverse") :category ~(t "category.playback") []
  (dismiss)
  (set "state.playing" (if (not= state.playing "reverse") "reverse" false)))

(defn tick "One frame on while playing, round the range." []
  (let [[start end] computed.range]
    (set "state.frame" (cond (= state.playing "reverse") (if (<= state.frame start) end (dec state.frame))
                             (>= state.frame end) start
                             :else (inc state.frame))))
  (poseFrame))

(defcommand jumpStart "Goes to the first frame." :title ~(t "command.jumpStart") :category ~(t "category.playback") []
  (dismiss)
  (setFrame (first computed.range)))

(defcommand jumpEnd "Goes to the last frame." :title ~(t "command.jumpEnd") :category ~(t "category.playback") []
  (dismiss)
  (setFrame (second computed.range)))

(defcommand prevKey "Goes to the keyframe before, or to the start."
  :title ~(t "command.prevKey") :category ~(t "category.playback") []
  (dismiss)
  (setFrame (apply max (concat [(first computed.range)] (filter (fn [f] (< f state.frame)) computed.keyFrames)))))

(defcommand nextKey "Goes to the next keyframe, or to the end."
  :title ~(t "command.nextKey") :category ~(t "category.playback") []
  (dismiss)
  (setFrame (apply min (concat [(second computed.range)] (filter (fn [f] (> f state.frame)) computed.keyFrames)))))

(defcommand frameCurrent "Scrolls the timeline to the current frame."
  :title ~(t "command.frameCurrent") :category ~(t "category.view") []
  (dismiss)
  (set "state.timelineOffset" (round-to (- (/ (dec state.frame) 24) 4) 1000)))

;;; Markers

(defn markers-where "The frames of the markers a test picks." [pick]
  (map :frame (filter (fn [m] (pick (:frame m))) state.markers)))

(defcommand addMarker "Adds a marker at the current frame."
  :title ~(t "command.addMarker") :category ~(t "category.marker") :icon "bookmark" []
  (dismiss)
  (when (empty? (markers-where (fn [f] (= f state.frame))))
    (set "state.markers"
         (sort-by :frame (concat state.markers [{:frame state.frame :label (str "F_" (pad state.frame 2 "0"))}])))))

(defcommand deleteMarker "Takes the marker at the current frame away."
  :title ~(t "command.deleteMarker") :category ~(t "category.marker")
  :enablement ~(some (fn [m] (= m.frame state.frame)) state.markers)
  []
  (dismiss)
  (set "state.markers" (remove (fn [m] (= (:frame m) state.frame)) state.markers)))

(defcommand prevMarker "Goes to the marker before."
  :title ~(t "command.prevMarker") :category ~(t "category.marker")
  :enablement ~(some (fn [m] (< m.frame state.frame)) state.markers)
  []
  (dismiss)
  (let [before (markers-where (fn [f] (< f state.frame)))]
    (when (not (empty? before)) (setFrame (apply max before)))))

(defcommand nextMarker "Goes to the next marker."
  :title ~(t "command.nextMarker") :category ~(t "category.marker")
  :enablement ~(some (fn [m] (> m.frame state.frame)) state.markers)
  []
  (dismiss)
  (let [later (markers-where (fn [f] (> f state.frame)))]
    (when (not (empty? later)) (setFrame (apply min later)))))

(defn scrubTo "The timeline scrubbed: the frame at that time." [time]
  (setFrame (frame-at time)))
