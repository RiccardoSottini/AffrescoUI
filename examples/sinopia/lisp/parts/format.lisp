;;; format.lisp --- the format pane's further rows: gradient fills, dashed lines, the proportion lock and the text box
;;;
;;; parts/views.lisp holds the pane; these rows sit in it. A gradient is {:from slot :to slot :angle degrees} on an
;;; object (0 across, 90 down), drawn by parts/geometry.lisp; a dash is a name the outline knows. With :lockRatio the
;;; width and the height change together. A text box keeps its margins (:marginX, :marginY, in points), whether its
;;; text wraps (:wrap) and how it fits (:autofit: shrink the text, or resize the shape to the text), which
;;; parts/slide.lisp honours.

(ns sinopia.parts.format)

;;; What the rows offer

(defcomputed gradientKinds "The gradient fills: none, then the fill colour fading down, across, diagonally and to dark."
  [{:id "none" :title (t "format.solid")}
   {:id "down" :title (t "format.linearDown") :to "light1" :angle 90}
   {:id "across" :title (t "format.linearRight") :to "light1" :angle 0}
   {:id "diagonal" :title (t "format.linearDiagonal") :to "light1" :angle 45}
   {:id "dark" :title (t "format.toDark") :to "dark1" :angle 90}])

(defcomputed dashKinds "The line's dashes, each with its name."
  [{:id "solid" :title (t "format.solidLine")}
   {:id "dot" :title (t "format.roundDot")}
   {:id "dash" :title (t "format.dash")}
   {:id "dashDot" :title (t "format.dashDot")}
   {:id "longDash" :title (t "format.longDash")}])

(defcomputed autofitKinds "How a text box fits its text."
  [{:id "none" :title (t "format.doNotAutofit")}
   {:id "shrink" :title (t "format.shrinkOnOverflow")}
   {:id "resize" :title (t "format.resizeShapeToFit")}])

(defcomputed ratioLocked "Whether the first selected object keeps its proportions."
  (or-else computed.first.lockRatio false))

(defcomputed gradientNow "The first selected object's gradient kind, or none."
  (or-else computed.first.gradient.kind "none"))

;;; Views

(defview format-gradient "The gradient fills, as small previews of the selection's fill: a click takes one." []
  `(row#format-gradient :width 236 :height 24 :gap 4 :color transparent
     ~(repeat [g computed.gradientKinds :key g.id]
       `(node :id ~(str "format-gradient-" g.id)
          :width 40
          :height 22
          :radius 4
          :cursor hand
          :role button
          :label ~g.title
          :tooltip ~g.title
          :let {:from ~(or-else (get computed.palette (or-else o.fill "accent1")) "#2a9d8f")}
          :color ~from
          :gradient ~(if (= g.id "none")
                       [from from]
                       {:kind "linear" :angle (+ g.angle 90) :stops [from (get computed.palette g.to)]})
          :borderWidth ~(if (= computed.gradientNow g.id) 2 1)
          :borderColor ~(if (= computed.gradientNow g.id) "@brandLine" "@rbFieldLine")
          :onClick ~(handler set-gradient g.id)))))

(defview format-dash "The line's dashes, each drawn: a click takes one." []
  `(row#format-dash :width 236 :height 24 :gap 4 :color transparent
     ~(repeat [d computed.dashKinds :key d.id]
       `(node :id ~(str "format-dash-" d.id)
          :width 44
          :height 22
          :radius 4
          :cursor hand
          :role button
          :label ~d.title
          :tooltip ~d.title
          :color @rbFieldBg
          :borderWidth ~(if (= (or-else o.dash "solid") d.id) 2 1)
          :borderColor ~(if (= (or-else o.dash "solid") d.id) "@brandLine" "@rbFieldLine")
          :onClick ~(handler set-property "dash" (if (= d.id "solid") nil d.id))
          (path :id ~(str "format-dash-" d.id "-line")
            :x 6 :y 6 :width 32 :height 10
            :viewBox "0 0 32 10"
            :d "M1 5L31 5"
            :stroke @rbInk
            :strokeWidth 2
            :strokeDash ~(case d.id
                           "dot" [2 2]
                           "dash" [6 4]
                           "dashDot" [6 3 2 3]
                           "longDash" [10 4]
                           []))))))

(defview format-ratio "Lock the proportions: the width and the height then change together." []
  `(label#format-ratio :style ~(if computed.ratioLocked "rbButtonOn" "rbButton")
     :height 26
     :fitText true
     :padding 6
     :runs [{:icon ~(if computed.ratioLocked "link-2" "link-2-off") :size 14}
            {:text ~(str " " (t "format.lockProportions")) :size 12}]
     :tooltip ~(t "format.lockProportionsAbout")
     :onClick toggle-ratio))

(defview format-text-box "The text box: its margins, whether its text wraps, and how it fits." []
  `(column#format-text-box :width 236 :fitContent height :alignItems stretch :gap 6 :color transparent
     (label#format-text-box-title :height 18 :textColor @rbMuted :runs [{:text ~(t "format.textBox") :size 12}])
     ~(stepper "format-margin-x" "format.marginSides" 'o.marginX 7.2 " pt" "marginX" 1.8 0 200)
     ~(stepper "format-margin-y" "format.marginTopBottom" 'o.marginY 3.6 " pt" "marginY" 1.8 0 200)
     (label#format-wrap :style ~(if (or-else o.wrap true) "rbButtonOn" "rbButton")
       :height 26
       :fitText true
       :padding 6
       :runs [{:icon text-wrap :size 14} {:text ~(str " " (t "format.wrapText")) :size 12}]
       :onClick ~(handler set-property "wrap" (if (or-else o.wrap true) false nil)))
     (column#format-autofit :width 236 :fitContent height :alignItems stretch :gap 2 :color transparent
       ~(repeat [a computed.autofitKinds :key a.id]
         `(label :id ~(str "format-autofit-" a.id)
            :style ~(if (= (or-else o.autofit "none") a.id) "rbButtonOn" "rbButton")
            :height 24
            :padding 6
            :role radio
            :selected ~(= (or-else o.autofit "none") a.id)
            :runs [{:icon ~(if (= (or-else o.autofit "none") a.id) "circle-dot" "circle") :size 12}
                   {:text ~(str " " a.title) :size 12}]
            :onClick ~(handler set-property "autofit" (if (= a.id "none") nil a.id)))))))

;;; What they do

(defn set-gradient "Gives the selected objects a gradient fill from their fill colour, or a solid fill." [kind]
  (let [g (find #(= (:id %) kind) computed.gradientKinds)]
    (setObjects {:gradient (if (= kind "none")
                             nil
                             {:kind kind :from (or-else computed.first.fill "accent1") :to (:to g) :angle (:angle g)})})))

(defn toggle-ratio "Keeps the selected objects' proportions when they are resized, or lets them go." []
  (setObjects {:lockRatio (if computed.ratioLocked nil true)}))

(defn sized "The properties a size change sets: with the proportions locked, the other side follows." [key value]
  (let [box (merge computed.firstPlaceholder computed.first)
        w (or-else box.w 100)
        h (or-else box.h 100)
        tall (/ h (max w 1))
        wide (/ w (max h 1))]
    (cond (not computed.ratioLocked) (assoc {} key value)
          (= key "w") {:w value :h (tenth (* value tall))}
          (= key "h") {:h value :w (tenth (* value wide))}
          :else (assoc {} key value))))
