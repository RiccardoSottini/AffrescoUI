;;; timeline.lisp --- the timeline: its header (editor type, menus, auto keying, the transport, the current frame and the
;;; range) and the timeline itself, with the selected objects' keys and the markers

;;; Keys

(defcomputed keyTracks "The animation as glTF tracks: each keyed object's translation, rotation (as quaternions) and scale over time."
  (flat (map (fn [it]
                 [{:node it.id
                   :path "translation"
                   :times (map (fn [it] (/ (- it.frame 1) 24)) it.keyframes)
                   :values (flat (map (fn [it] it.translation) it.keyframes))}
                  {:node it.id
                   :path "rotation"
                   :times (map (fn [it] (/ (- it.frame 1) 24)) it.keyframes)
                   ;; A rotation in degrees as a quaternion: (get (map f [x]) 0) gives f's value for x, as an expression has no let.
                   :values (flat (map (fn [it]
                                        (get (map (fn [it]
                                                    [(- (* (sin it.0) (cos it.1) (cos it.2))
                                                        (* (cos it.0) (sin it.1) (sin it.2)))
                                                     (+ (* (cos it.0) (sin it.1) (cos it.2))
                                                        (* (sin it.0) (cos it.1) (sin it.2)))
                                                     (- (* (cos it.0) (cos it.1) (sin it.2))
                                                        (* (sin it.0) (sin it.1) (cos it.2)))
                                                     (+ (* (cos it.0) (cos it.1) (cos it.2))
                                                        (* (sin it.0) (sin it.1) (sin it.2)))])
                                                  [(map (fn [it] (/ (radians it) 2)) it.rotationDegrees)])
                                             0))
                                      it.keyframes))}
                  {:node it.id
                   :path "scale"
                   :times (map (fn [it] (/ (- it.frame 1) 24)) it.keyframes)
                   :values (flat (map (fn [it] it.scale) it.keyframes))}])
               (filter (fn [it] (> (count (or-else it.keyframes [])) 0)) state.scene.nodes))))

(defcomputed gltfClips "The clips an export holds: one, Action, if anything is keyed."
  (if (= (count computed.keyTracks) 0) [] [{:name "Action" :tracks computed.keyTracks}]))

(defcomputed keyFrames "The frames the selected objects have keys at, each once, in order."
  (reduce (fn [acc it] (if (contains acc it) acc (concat acc [it])))
            []
            (sort (flat (map (fn [it] (map (fn [it] it.frame) (or-else it.keyframes [])))
                             (filter (fn [it] (contains state.view.selection it.id)) state.scene.nodes))))))

(defcomputed keyframes "Those frames as the timeline's keyframes, at their times."
  (map (fn [it] {:time (/ (- it 1) 24)}) computed.keyFrames))

;;; The range

(defcomputed range "The frames played: the preview range's, or the scene's."
  (if (some? state.previewRange) [state.previewRange.start state.previewRange.end] [state.start state.end]))

;;; The range

(defcomputed rangeKeys "Where the range's start and end are kept in the state."
  (if (some? state.previewRange) ["previewRange.start" "previewRange.end"] ["start" "end"]))

;;; The area

(defcomponent timelineArea "The timeline: its header and the track of the selected objects' keys." {}
  `(node#timeline-area :color @region :radius 8
     ~(timeline-header)
     ~(timeline-track)))

(defcomputed transport "The transport's buttons: to the start, the key before, play back and forth, the next key, the end."
  [{:id "jumpStart" :icon "skip-back" :title (t "timeline.toEndpoint")}
   {:id "prevKey" :icon "chevrons-left" :title (t "timeline.toKeyframe")}
   {:id "playReverse" :icon "play" :title (t "command.playReverse")}
   {:id "play" :icon "play" :title (t "command.play")}
   {:id "nextKey" :icon "chevrons-right" :title (t "timeline.toKeyframe")}
   {:id "jumpEnd" :icon "skip-forward" :title (t "timeline.toEndpoint")}])

(defview timeline-header "The header: editor type, the menus, auto keying, the transport, the frame, the range." []
  `(row#timeline-header :anchors {:left 0 :right 0 :top 0} :height 26 :color @header :radius {:topLeft 8 :topRight 8} :alignItems center :gap 2
     (node :use gap)
     (node#editor-type-timeline :use hButton
       :with {:icon clock-3
              :chevron true
              :width 38
              :tooltip ~(t "header.editorType")
              :open state.open.editorTimeline
              :placement above
              :items ~(editor-choices "timeline")})
     (node :use gap :with {:width 6})
     ~(repeat [m ["Playback" "Keying" "View" "Marker"]] (timeline-menu m))
     {:grow 1 :height 1 :color transparent}
     (node#auto-key :use hButton
       :with {:icon circle :width 24 :tooltip ~(t "timeline.autoKeyTip") :on ~(= state.autoKey true) :run toggleOption :with {:key autoKey}})
     (node :use gap :with {:width 6})
     ~(repeat [b computed.transport :index i] (transport-button b i))
     (node :use gap :with {:width 6})
     (node#frame-current :use frameField :with {:label ~(t "timeline.currentFrame") :value ~state.frame :width 56 :key frame})
     {:grow 1 :height 1 :color transparent}
     ~@(range-fields)
     (node :use gap)))

(defview timeline-menu "One of the header's menus: Playback, Keying, View or Marker; m is its name in its state's key." [m]
  `(node :id ~(str "timeline-menu-" (lower-case m))
     :use hButton
     :with {:text ~(t (str "timeline.menu." m))
            :width ~(+ (* (count (t (str "timeline.menu." m))) 7) 26)
            :chevron true
            :back transparent
            :open ~(str "state.open.timeline" m)
            :items ~(get computed.timelineMenus (lower-case m))
            :menuWidth 230
            :placement above}))

(defview transport-button "A transport button; Play shows Pause while playing, and the ends round the group's corners." [b i]
  `(node :id ~(str "timeline-" b.id)
     :use hButton
     :with {:icon ~(if (and state.playing (= b.id "play")) "pause" b.icon)
            :width 24
            :tooltip ~b.title
            :run ~b.id
            :radius ~(cond (= i 0) {:topLeft 4 :bottomLeft 4} (= i 5) {:topRight 4 :bottomRight 4} :else 0)}))

(defview range-fields "The preview range's switch, then the range's start and end: three nodes." []
  [`(node#preview-range :use hButton
      :with {:icon clock-3 :width 24 :back transparent :on ~(some? state.previewRange) :tooltip ~(t "timeline.previewRangeTip") :run togglePreviewRange})
   `(node#frame-start :use frameField
      :with {:label ~(t "timeline.start") :value ~computed.range.0 :key start :radius {:topLeft 4 :bottomLeft 4} :ranged ~(some? state.previewRange)})
   `(node#frame-end :use frameField
      :with {:label ~(t "properties.frameEnd") :value ~computed.range.1 :key end :radius {:topRight 4 :bottomRight 4} :ranged ~(some? state.previewRange)})])

(defview timeline-track "The timeline: the keys, the markers, the range played; scrubbing goes to a frame, a key dragged moves." []
  `(timeline#timeline :anchors {:left 0 :right 0 :top 26 :bottom 0}
     :tracks ~[{:id "summary" :label " " :keyframes computed.keyframes}]
     :time ~(/ (- state.frame 1) 24)
     :duration ~(/ (+ (max state.end computed.range.1) 20) 24)
     :fps 24
     :timeFormat ~(if (= state.timeSeconds true) "seconds" "frames")
     :markers ~(if (= state.hideMarkers true) [] (map (fn [it] {:time (/ (- it.frame 1) 24) :label it.label}) state.markers))
     :playRange ~[(/ (- computed.range.0 1) 24) (/ (- computed.range.1 1) 24)]
     :headerWidth 0
     :trackHeight 20
     :pixelsPerSecond 110
     :offset ~state.timelineOffset
     :color @region
     :headerColor @header
     :textColor @dim
     :accentColor @sel
     :clipColor @active
     :label ~(t "editor.timeline")
     :onScrub ~(handler scrubTo event.time)
     :onKeyframeMove moveKeyframe))
