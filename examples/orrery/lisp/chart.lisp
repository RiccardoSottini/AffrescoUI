;;; chart.lisp --- the side: the sky chart (from above, or the sky from the ground) and what orbits what
;;;
;;; From above, the chart is a canvas of the orbits, the planets' names and your notes; from the ground it is
;;; sky.lisp's view. Under it, the orbit graph: drag a link to make a body orbit another.

;;; The chart

(defcomputed chartItems
  (concat state.stars
            (map (fn [it]
                   {:id (str "ring-" it.id)
                    :x (- (* it.cx 22) (* it.radius 22))
                    :y (- (* it.cz 22) (* it.radius 22))
                    :width (* it.radius 44)
                    :height (* it.radius 44)
                    :radius (* it.radius 22)
                    :color "transparent"
                    :borderColor "#2f3a55"
                    :borderWidth 1})
                 (filter (fn [it] (not= it.parent "")) computed.placed))
            (map (fn [it]
                   {:id (str "body-" it.id)
                    :x (- (* it.x 22) (if (= it.parent "") 9 (+ 4 (* it.size 10))))
                    :y (- (* it.z 22) (if (= it.parent "") 9 (+ 4 (* it.size 10))))
                    :width (if (= it.parent "") 18 (+ 8 (* it.size 20)))
                    :height (if (= it.parent "") 18 (+ 8 (* it.size 20)))
                    :radius (if (= it.parent "") 9 (+ 4 (* it.size 10)))
                    :color it.hex
                    :borderColor (if (= it.id state.selected) "#fbbf24" "transparent")
                    :borderWidth 2})
                 computed.placed)
            (map (fn [it]
                   {:id (str "label-" it.id)
                    :x (+ (* it.x 22) 10)
                    :y (- (* it.z 22) 10)
                    :width 110
                    :height 20
                    :color "transparent"
                    :borderWidth 0
                    :text it.name
                    :textSize 16
                    :textColor "#aab2c5"})
                 (filter (fn [it] (= it.parent "sun")) computed.placed))
            state.notes))

(defview chart-bar "#chart-bar." []
  `(row#chart-bar :width "100%" :height 40 :gap 6 :alignItems center :color @panel
     ~(sky-mode-ground)
     ~(sky-mode-above)
     (textarea#note-text :visible ~(= state.skyMode "above")
       :bind state.draft
       :grow 1
       :height 28
       :radius 4
       :color @background
       :textColor @text
       :label "Annotation text"
       :placeholder "Note"
       :onSubmit addNote)
     ~(add-note)
     {:id mark
      :visible ~(= state.skyMode "above")
      :style button
      :icon crosshair
      :role button
      :label "Mark the selected body"
      :tooltip "Mark the selected body"
      :onClick markSelected
      :width 30}
     {:id clear-notes
      :visible ~(= state.skyMode "above")
      :style button
      :icon trash-2
      :role button
      :label "Clear the annotations"
      :tooltip "Clear the annotations"
      :onClick clearNotes
      :width 30}))

(defview sky-mode-ground "#sky-mode-ground, in chart-bar." []
  `{:id sky-mode-ground
    :style button
    :role button
    :text "From the ground"
    :label "The sky from the ground"
    :selected ~(= state.skyMode "ground")
    :color ~(if (= state.skyMode "ground") "@raised" "@panel")
    :fitText true
    :onClick skyGround})

(defview sky-mode-above "#sky-mode-above, in chart-bar." []
  `{:id sky-mode-above
    :style button
    :role button
    :text "From above"
    :label "The chart from above"
    :selected ~(= state.skyMode "above")
    :color ~(if (= state.skyMode "above") "@raised" "@panel")
    :fitText true
    :onClick skyAbove})

(defview add-note "#add-note, in chart-bar." []
  `{:id add-note
    :visible ~(= state.skyMode "above")
    :style button
    :icon sticky-note
    :role button
    :label "Add a note in the middle of the chart"
    :tooltip "Add a note in the middle of the chart"
    :onClick addNote
    :width 30})

(defview chart-frame "#chart-frame." []
  `(panel#chart-frame :width "100%" :height ~(clamp (- window.height 524) 120 300)
     (canvas#chart :visible ~(= state.skyMode "above")
       :bind state.chart
       :color @sky
       :rulers false
       :snap false
       :accentColor @accent
       :items ~computed.chartItems
       :label "Sky chart"
       :onTransform moveNotes
       :keys {:delete deleteNotes :backspace deleteNotes}
       :anchors {:left 0 :right 0 :top 0 :bottom 0})
     (node#sky-view :use
                    skyFromGround
                    :visible
                    ~(= state.skyMode "ground")
                    :anchors
                    {:left 0 :right 0 :top 0 :bottom 0})
     ~(chart-info)))

(defview chart-info "#chart-info." []
  `(column#chart-info :visible ~(some? computed.selectedPosition)
     :height ~(if (= state.skyMode "ground") 62 42)
     :anchors {:left 8 :top 6}
     :width 260
     :gap 2
     (label#chart-info-name :height 22 :fitText true :font title :text ~computed.selectedBody.name :textColor @onSky)
     (label#chart-info-line :height 18
       :fitText true
       :font small
       :textColor ~(if (= state.skyMode "ground") "#dfe7f2" "@onSkyMuted")
       :text ~(cond (= state.skyMode "ground") (or-else computed.skyInfo.0 "Where you stand")
                    (= computed.selectedBody.parent "") "The centre of the chart"
                    :else (str (fixed (/ (hypot computed.selectedPosition.x computed.selectedPosition.z) 4.3) 2)
                         " AU from the Sun, orbits "
                         (if (= computed.selectedBody.parent "sun")
                             "the Sun"
                             (:name (find (fn [it] (= it.id computed.selectedBody.parent)) state.bodies)))
                         " every "
                         computed.selectedBody.period
                         " days")))
     (label#chart-info-sky :visible ~(= state.skyMode "ground")
       :height 18
       :fitText true
       :font small
       :textColor "#dfe7f2"
       :text ~(or-else computed.skyInfo.1 ""))))

;;; What orbits what

(defview orbits-view "#orbits." []
  `(graph#orbits :width "100%"
     :grow 1
     :color @background
     :minimap false
     :nodes ~computed.graphNodes
     :links ~computed.graphLinks
     :selectedIds ~[state.selected]
     :kindColors {:orbit "#2dd4bf"}
     :nodeColor @raised
     :headerColor @border
     :textColor @text
     :accentColor @accent
     :zoom 0.8
     :onMove moveGraphNodes
     :onConnect reparent
     :onDisconnect orbitAround
     :onSelect [{:if ~(> (count event.ids) 0) :then [{:set state.selected :to ~event.ids.0}]}]))
