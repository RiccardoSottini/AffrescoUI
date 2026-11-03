;;; chart.lisp --- the side: the sky chart (from above, or the sky from the ground) and what orbits what
;;;
;;; From above, the chart is a canvas of the orbits, the planets' names and your notes; from the ground it is
;;; sky.lisp's view. Under it, the orbit graph: drag a link to make a body orbit another.

;;; The chart

(defcomputed chartItems "The chart's items: the stars, each planet's orbit and name, and the notes."
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

(defview chart-bar "The chart's bar: from the ground or from above, and, from above, the note's text and its tools."
  []
  `(row#chart-bar :width "100%" :height 40 :gap 6 :alignItems center :color @panel
     ~(ground-button)
     ~(above-button)
     (textarea#note-text :visible ~(= state.skyMode "above")
       :bind state.draft
       :grow 1
       :height 28
       :radius 4
       :color @background
       :textColor @text
       :label ~(t "chart.noteText")
       :placeholder ~(t "chart.note")
       :onSubmit addNote)
     ~(add-note-button)
     (node#mark :visible ~(= state.skyMode "above")
       :style button
       :icon crosshair
       :role button
       :label ~(t "command.toggleMarker")
       :tooltip ~(t "command.toggleMarker")
       :onClick markSelected
       :width 30)
     (node#clear-notes :visible ~(= state.skyMode "above")
       :style button
       :icon trash-2
       :role button
       :label ~(t "chart.clearNotes")
       :tooltip ~(t "chart.clearNotes")
       :onClick clearNotes
       :width 30)))

(defview ground-button "From the ground: the sky as you see it, standing on the Earth." []
  `(node#sky-mode-ground :style button
     :role button
     :text ~(t "chart.fromGround")
     :label ~(t "command.skyGround")
     :selected ~(= state.skyMode "ground")
     :color ~(if (= state.skyMode "ground") "@raised" "@panel")
     :fitText true
     :onClick skyGround))

(defview above-button "From above: the chart of the system, with its notes." []
  `(node#sky-mode-above :style button
     :role button
     :text ~(t "chart.fromAbove")
     :label ~(t "chart.fromAboveLabel")
     :selected ~(= state.skyMode "above")
     :color ~(if (= state.skyMode "above") "@raised" "@panel")
     :fitText true
     :onClick skyAbove))

(defview add-note-button "Add a note in the middle of the chart, from above." []
  `(node#add-note :visible ~(= state.skyMode "above")
     :style button
     :icon sticky-note
     :role button
     :label ~(t "chart.addNote")
     :tooltip ~(t "chart.addNote")
     :onClick addNote
     :width 30))

(defview chart-frame
  "The chart: the canvas from above, or the sky from the ground, with what is known of the selected body."
  []
  `(panel#chart-frame :width "100%" :height ~(clamp (- window.height 524) 120 300)
     (canvas#chart :visible ~(= state.skyMode "above")
       :bind state.chart
       :color @sky
       :rulers false
       :snap false
       :accentColor @accent
       :items ~computed.chartItems
       :label ~(t "chart.label")
       :onTransform moveNotes
       :keys {:delete deleteNotes :backspace deleteNotes}
       :anchors {:left 0 :right 0 :top 0 :bottom 0})
     (node#sky-view :use skyFromGround
       :visible ~(= state.skyMode "ground")
       :anchors {:left 0 :right 0 :top 0 :bottom 0})
     ~(chart-info)))

(defview chart-info "The selected body's name, and where it is or what it orbits." []
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
       :text ~(cond (= state.skyMode "ground") (or-else computed.skyInfo.0 (t "chart.whereYouStand"))
                    (= computed.selectedBody.parent "") (t "chart.centre")
                    :else (t "chart.orbit"
                       {:au (fixed (/ (hypot computed.selectedPosition.x computed.selectedPosition.z) 4.3) 2)
                        :parent (if (= computed.selectedBody.parent "sun")
                                  (t "chart.theSun")
                                  (:name (find (fn [it] (= it.id computed.selectedBody.parent)) state.bodies)))
                        :days (text computed.selectedBody.period)})))
     (label#chart-info-sky :visible ~(= state.skyMode "ground")
       :height 18
       :fitText true
       :font small
       :textColor "#dfe7f2"
       :text ~(or-else computed.skyInfo.1 ""))))

;;; What orbits what

(defview orbit-graph "What orbits what, as a graph: drag a link to make a body orbit another." []
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
     :onSelect ~(handler pickInGraph event.ids)))
