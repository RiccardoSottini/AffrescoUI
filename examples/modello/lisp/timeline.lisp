;;; timeline.lisp --- TODO

(defcomputed keyTracks
  (flat (map (fn [it]
                 [{:node it.id
                   :path "translation"
                   :times (map (fn [it] (/ (- it.frame 1) 24)) it.keyframes)
                   :values (flat (map (fn [it] it.translation) it.keyframes))}
                  {:node it.id
                   :path "rotation"
                   :times (map (fn [it] (/ (- it.frame 1) 24)) it.keyframes)
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

(defcomputed gltfClips (if (= (count computed.keyTracks) 0) [] [{:name "Action" :tracks computed.keyTracks}]))

(defcomputed keyFrames
  (reduce (fn [acc it] (if (contains acc it) acc (concat acc [it])))
            []
            (sort (flat (map (fn [it] (map (fn [it] it.frame) (or-else it.keyframes [])))
                             (filter (fn [it] (contains state.view.selection it.id)) state.scene.nodes))))))

(defcomputed keyframes (map (fn [it] {:time (/ (- it 1) 24)}) computed.keyFrames))

(defcomputed range
  (if (some? state.previewRange) [state.previewRange.start state.previewRange.end] [state.start state.end]))

(defcomputed rangeKeys (if (some? state.previewRange) ["previewRange.start" "previewRange.end"] ["start" "end"]))

(defcomponent timelineArea "The timelineArea component."
  {}
  `(node#timeline-area :color @region :radius 8
     ~(timeline-header)
     ~(timeline-view)))

(defview timeline-header "#timeline-header." []
  `(row#timeline-header :anchors {:left 0 :right 0 :top 0}
     :height 26
     :color @header
     :radius {:topLeft 8 :topRight 8}
     :alignItems center
     :gap 2
     (node :use gap)
     (node#editor-type-timeline :use hButton
       :with {:icon clock-3
              :chevron true
              :width 38
              :tooltip "Editor Type"
              :open state.open.editorTimeline
              :placement above
              :items ~(map (fn [it]
                             (merge it
                                    {:checked (= it.id "timeline")
                                     :run "swapEditor"
                                     :with {:args {:from "timeline" :to it.id}}}))
                           computed.editorTypes)})
     (node :use gap :with {:width 6})
     ~(repeat [m ["Playback" "Keying" "View" "Marker"]]
       `(node :id ~(str "timeline-menu-" (lower-case m))
          :use hButton
          :with {:text ~m
                 :width ~(+ (* (count m) 7) 26)
                 :chevron true
                 :back transparent
                 :open ~(str "state.open.timeline" m)
                 :items ~(get computed.timelineMenus (lower-case m))
                 :menuWidth 230
                 :placement above}))
     {:grow 1 :height 1 :color transparent}
     (node#auto-key :use hButton
       :with {:icon circle
              :width 24
              :tooltip "Auto Keying: moving the selection keys it"
              :on ~(= state.autoKey true)
              :run toggleOption
              :with {:args {:key autoKey}}})
     (node :use gap :with {:width 6})
     ~(repeat [b [["skip-back" "Jump to Endpoint" "jumpStart"] ["chevrons-left" "Jump to Keyframe" "prevKey"] ["play" "Play Animation in Reverse" "playReverse"] ["play" "Play Animation" "play"] ["chevrons-right" "Jump to Keyframe" "nextKey"] ["skip-forward" "Jump to Endpoint" "jumpEnd"]] :index i]
       `(node :id ~(str "timeline-" b.2)
          :use hButton
          :with {:icon ~(if (and state.playing (= b.2 "play")) "pause" b.0)
                 :width 24
                 :tooltip ~b.1
                 :run ~b.2
                 :radius ~(cond (= i 0) {:topLeft 4 :bottomLeft 4} (= i 5) {:topRight 4 :bottomRight 4} :else 0)}))
     (node :use gap :with {:width 6})
     (node#frame-current :use frameField :with {:label "Current Frame" :value ~state.frame :width 56 :key frame})
     {:grow 1 :height 1 :color transparent}
     (node#preview-range :use hButton
       :with {:icon clock-3
              :width 24
              :back transparent
              :on ~(some? state.previewRange)
              :tooltip "Use Preview Range: Start and End then set a range of their own for playback"
              :run togglePreviewRange})
     (node#frame-start :use frameField
       :with {:label "Start"
              :value ~computed.range.0
              :key start
              :radius {:topLeft 4 :bottomLeft 4}
              :ranged ~(some? state.previewRange)})
     (node#frame-end :use frameField
       :with {:label "End"
              :value ~computed.range.1
              :key end
              :radius {:topRight 4 :bottomRight 4}
              :ranged ~(some? state.previewRange)})
     (node :use gap)))

(defview timeline-view "#timeline." []
  `(timeline#timeline :anchors {:left 0 :right 0 :top 26 :bottom 0}
     :tracks ~[{:id "summary" :label " " :keyframes computed.keyframes}]
     :time ~(/ (- state.frame 1) 24)
     :duration ~(/ (+ (max state.end computed.range.1) 20) 24)
     :fps 24
     :timeFormat ~(if (= state.timeSeconds true) "seconds" "frames")
     :markers ~(if (= state.hideMarkers true)
                   []
                   (map (fn [it] {:time (/ (- it.frame 1) 24) :label it.label}) state.markers))
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
     :label "Timeline"
     :onScrub {:run setFrame :with {:args {:value ~(+ (round (* event.time 24)) 1)}}}
     :onKeyframeMove {:run moveKeyframe}))

