;;; animation.lisp --- keyframes, playback and markers
;;;
;;; A node keeps its keyframes, `{frame, translation, rotationDegrees, scale}` sorted by frame; between two of them it
;;; is posed by easing from one to the other. The timeline plays at 24 frames a second (`tick`, from #playback).

(ns modello.animation)

;;; Keyframes

(defn key-of "A node's keyframe at the current frame." [n]
  {:frame state.frame :translation (:translation n) :rotationDegrees (:rotationDegrees n) :scale (:scale n)})

(defn key-selected "Keys the selected objects where they are, at the current frame." []
  (change-nodes selected?
                (fn [n]
                  (let [others (remove #(= (:frame %) state.frame) (or (:keyframes n) []))]
                    (assoc n "keyframes" (sort-by :frame (concat others [(key-of n)])))))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn autoKey "Keys the selected objects after a change, while Auto Keying is on." []
  (when (true? state.autoKey) (key-selected)))

(defcommand insertKeyframe "Keys the selected objects at the current frame."
  :title ~(t "command.insertKeyframe") :description ~(t "command.insertKeyframe.about")
  :category ~(t "category.animation") :icon "diamond" :enablement ~(pos? (count state.view.selection)) []
  (dismiss)
  (snapshot)
  (key-selected)
  (set "state.last" (t "command.insertKeyframe")))

(defn change-keys "Changes the keyframes of the selected objects that have some." [change]
  (change-nodes #(and (selected? %) (some? (:keyframes %))) #(update % :keyframes change)))

(defcommand deleteKeyframe "Takes the selected objects' keyframes at the current frame away."
  :title ~(t "command.deleteKeyframe") :description ~(t "command.deleteKeyframe.about")
  :category ~(t "category.animation") :icon "diamond-minus" :enablement ~(contains computed.keyFrames state.frame) []
  (dismiss)
  (when (contains computed.keyFrames state.frame)
    (snapshot)
    (change-keys (partial remove #(= (:frame %) state.frame)))
    (set "state.last" (t "command.deleteKeyframe"))))

(defcommand clearKeyframes "Takes every keyframe of the selected objects away."
  :title ~(t "command.clearKeyframes") :description ~(t "command.clearKeyframes.about")
  :category ~(t "category.animation") :enablement ~(pos? (count computed.keyFrames)) []
  (dismiss)
  (when (seq computed.keyFrames)
    (snapshot)
    (change-keys (constantly []))
    (set "state.last" (t "command.clearKeyframes"))))

(defn frame-at "The frame at a time in seconds." [time]
  (inc (round (* time 24))))

(defn move-keyframe "A keyframe dragged on the timeline: the selected objects' keys at its frame move to the new one." []
  (let [from (frame-at event.from)
        to (frame-at event.time)]
    (when (and (not= from to) (not (contains computed.keyFrames to)) (contains computed.keyFrames from))
      (when (not= state.keyDragAt from) (snapshot))
      (set "state.keyDragAt" to)
      (change-keys (fn [keyframes]
                     (->> keyframes (map #(if (= (:frame %) from) (assoc % :frame to) %)) (sort-by :frame))))
      (pose-frame))))

;;; Posing between keyframes

(defn eased "How far between two keyframes a frame is, eased in and out." [a b]
  (let [start (:frame a)
        end (:frame b)
        u (if (= end start) 0 (/ (- state.frame start) (- end start)))]
    (* u u (- 3 (* 2 u)))))

(defn between "A list of a keyframe's, item by item, a part of the way to the next one's, rounded." [from to u scale]
  (map (fn [v w] (round-to (+ v (* (- w v) u)) scale)) from to))

(defn posed "A node with keyframes, posed at the current frame." [n]
  (let [keyframes (:keyframes n)
        a (or (find #(<= (:frame %) state.frame) (reverse keyframes)) (first keyframes))
        b (or (find #(>= (:frame %) state.frame) keyframes) (last keyframes))
        u (eased a b)]
    (assoc n
           :translation (between (:translation a) (:translation b) u 10000)
           :rotationDegrees (between (:rotationDegrees a) (:rotationDegrees b) u 1000)
           :scale (between (:scale a) (:scale b) u 10000))))

(defn pose-frame "Poses every node that has keyframes at the current frame." []
  (change-nodes #(seq (:keyframes %)) posed))

;;; The current frame and the range

(defn set-frame "Goes to a frame." [value]
  (set "state.frame" (round (clamp value 0 1048574)))
  (pose-frame))

(defn set-range "The start or the end of the range: of the preview range while it is used." [key value]
  (let [frame (round (clamp value 0 1048574))
        [start-key end-key] computed.rangeKeys
        [start end] computed.range]
    (if (= key :start)
      (set (str "state." start-key) (min frame end))
      (set (str "state." end-key) (max frame start)))))

(defn set-frame-field "A field of the timeline's header: the current frame, the start or the end." [key value]
  (if (= key :frame) (set-frame value) (set-range key value)))

(defcommand togglePreviewRange "Plays a range of its own, the whole range to start with, or the scene's again."
  :title ~(t "command.togglePreviewRange") :description ~(t "command.togglePreviewRange.about")
  :category ~(t "category.playback") :icon "clock-3" []
  (dismiss)
  (set "state.previewRange" (when (nil? state.previewRange) {:start state.start :end state.end})))

;;; Playing

(defcommand play "Plays the animation, or stops it."
  :title ~(t "command.play") :description ~(t "command.play.about")
  :category ~(t "category.playback") :icon "play" []
  (dismiss)
  (set "state.playing" (if (not= state.playing :forward) :forward false)))

(defcommand playReverse "Plays the animation backwards, or stops it."
  :title ~(t "command.playReverse") :description ~(t "command.playReverse.about")
  :category ~(t "category.playback") []
  (dismiss)
  (set "state.playing" (if (not= state.playing :reverse) :reverse false)))

(defn tick "One frame on while playing, round the range." []
  (let [[start end] computed.range]
    (set "state.frame" (cond (= state.playing :reverse) (if (<= state.frame start) end (dec state.frame))
                             (>= state.frame end) start
                             :else (inc state.frame))))
  (pose-frame))

(defcommand jumpStart "Goes to the first frame."
  :title ~(t "command.jumpStart") :description ~(t "command.jumpStart.about")
  :category ~(t "category.playback") []
  (dismiss)
  (set-frame (first computed.range)))

(defcommand jumpEnd "Goes to the last frame."
  :title ~(t "command.jumpEnd") :description ~(t "command.jumpEnd.about")
  :category ~(t "category.playback") []
  (dismiss)
  (set-frame (second computed.range)))

(defcommand prevKey "Goes to the keyframe before, or to the start."
  :title ~(t "command.prevKey") :description ~(t "command.prevKey.about")
  :category ~(t "category.playback") []
  (dismiss)
  (set-frame (apply max (first computed.range) (filter #(< % state.frame) computed.keyFrames))))

(defcommand nextKey "Goes to the next keyframe, or to the end."
  :title ~(t "command.nextKey") :description ~(t "command.nextKey.about")
  :category ~(t "category.playback") []
  (dismiss)
  (set-frame (apply min (second computed.range) (filter #(> % state.frame) computed.keyFrames))))

(defcommand frameCurrent "Scrolls the timeline to the current frame."
  :title ~(t "command.frameCurrent") :description ~(t "command.frameCurrent.about")
  :category ~(t "category.view") []
  (dismiss)
  (set "state.timelineOffset" (round-to (- (/ (dec state.frame) 24) 4) 1000)))

;;; Markers

(defn markers-where "The frames of the markers a test picks." [pick]
  (filter pick (map :frame state.markers)))

(defcommand addMarker "Adds a marker at the current frame."
  :title ~(t "command.addMarker") :description ~(t "command.addMarker.about")
  :category ~(t "category.marker") :icon "bookmark" []
  (dismiss)
  (when (empty? (markers-where #(= % state.frame)))
    (set "state.markers"
         (sort-by :frame (concat state.markers [{:frame state.frame :label (str "F_" (pad state.frame 2 "0"))}])))))

(defcommand deleteMarker "Takes the marker at the current frame away."
  :title ~(t "command.deleteMarker") :description ~(t "command.deleteMarker.about")
  :category ~(t "category.marker") :enablement ~(some #(= (:frame %) state.frame) state.markers) []
  (dismiss)
  (set "state.markers" (remove #(= (:frame %) state.frame) state.markers)))

(defcommand prevMarker "Goes to the marker before."
  :title ~(t "command.prevMarker") :description ~(t "command.prevMarker.about")
  :category ~(t "category.marker") :enablement ~(some #(< (:frame %) state.frame) state.markers) []
  (dismiss)
  (let [before (markers-where #(< % state.frame))]
    (when (seq before) (set-frame (apply max before)))))

(defcommand nextMarker "Goes to the next marker."
  :title ~(t "command.nextMarker") :description ~(t "command.nextMarker.about")
  :category ~(t "category.marker") :enablement ~(some #(> (:frame %) state.frame) state.markers) []
  (dismiss)
  (let [later (markers-where #(> % state.frame))]
    (when (seq later) (set-frame (apply min later)))))

(defn scrub-to "The timeline scrubbed: the frame at that time." [time]
  (set-frame (frame-at time)))
