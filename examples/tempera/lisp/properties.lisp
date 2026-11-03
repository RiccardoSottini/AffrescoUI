;;; properties.lisp --- the Properties and Adjust panels: the active layer's settings, and the adjustments to add
;;;
;;; Properties shows what the active layer is. An adjustment layer shows its own values (levels, a curve, hue and
;;; saturation, colour balance, brightness and contrast, a threshold); a pixel layer, its transform, the align icons
;;; and quick actions, in sections that fold. Adjust adds an adjustment layer of each kind.

;;; Sliders and sections

(defcomponent tAdjustSlider "One of the active adjustment's values, from low to high: its title, a slider, its value."
  {:title "" :key "" :value 0 :low 0 :high 100 :scale 100}
  `(row :height 26 :alignItems center :gap 6 :width "100%"
     (node :style dim :fitText false :text ~title :width 74 :height 20)
     (node :use tRange
       :with {:title ~title :key ~key :value ~value :low ~low :high ~high :scale ~scale :run setAdjustment :width 112})
     (label :text ~(round (* value scale))
       :textColor @text
       :width 34
       :height 20
       :align {:horizontal end :vertical center})))

(defcomponent tBalanceSlider "One channel of the active Colour Balance layer, from -100 to 100." {:title "" :channel 0}
  `(row :height 26 :alignItems center :gap 6 :width "100%"
     (node :style dim :fitText false :text ~title :width 74 :height 20)
     (node :use tRange
       :with {:title ~title
              :key value
              :value ~(get computed.adjustment.balance channel)
              :low -100
              :high 100
              :scale 100
              :run setBalance
              :args {:channel ~channel}
              :width 112})
     (label :text ~(round (* (get computed.adjustment.balance channel) 100))
       :textColor @text
       :width 34
       :height 20
       :align {:horizontal end :vertical center})))

(defcomponent tSection "A section's title in the Properties panel: a click folds it, or opens it." {:title "" :key ""}
  `(row :role button
     :label ~title
     :selected ~(get state.props key)
     :width "100%"
     :height 24
     :alignItems center
     :gap 4
     :cursor hand
     :onClick ~(handler toggleOpen (str "state.props." key))
     (label :icon ~(if (get state.props key) "chevron-down" "chevron-right")
       :textColor @muted
       :width 14
       :height 20
       :align center)
     (label :text ~title :textColor @text :fitText true)))

;;; The Properties panel

(defview properties-panel "The Properties panel: what the active layer is, then its values or its sections." []
  `(column#properties :when ~(= state.panels.properties "properties")
     :role region
     :label ~(t "panel.properties")
     :anchors {:left 10 :right 10 :top 6 :bottom 6}
     :gap 4
     :scroll vertical
     (label#properties-kind :textColor @text
       :fitText true
       :icon ~(cond (not computed.activeLayer) "circle-off"
                    (= computed.activeLayer.kind "group") "folder"
                    computed.adjustment "contrast"
                    :else "image")
       :text ~(cond (not computed.activeLayer) (t "properties.noLayer")
                    (= computed.activeLayer.kind "group") (t "properties.group")
                    computed.adjustment (t (str "adjustment." computed.adjustmentKind))
                    :else (t "properties.pixelLayer")))
     (node#properties-name :style dim
       :when ~(and computed.activeLayer (not computed.adjustment))
       :text ~(t "properties.summary"
                 {:name computed.activeLayer.name
                  :blend computed.blendTitle
                  :opacity (text computed.activeLayer.opacity)}))
     (node#properties-line :width "100%" :height 1 :color @border)
     ~@(levels-values)
     ~@(curve-values)
     ~@(hue-values)
     ~@(balance-values)
     ~@(brightness-values)
     ~@(threshold-values)
     ~(layer-sections)))

(defview levels-values "A Levels layer's tones, and its input, gamma and output sliders." []
  [`(node#adjust-tones :use areaChart
      :when ~(= computed.adjustmentKind "levels")
      :width 200
      :height 60
      :with {:color @muted :values ~(tones)})
   (adjust-slider "adjust-input-black" "levels" "inputBlack" 0 255 255)
   (adjust-slider "adjust-input-white" "levels" "inputWhite" 0 255 255)
   (adjust-slider "adjust-gamma" "levels" "gamma" 10 400 100)
   (adjust-slider "adjust-output-black" "levels" "outputBlack" 0 255 255)
   (adjust-slider "adjust-output-white" "levels" "outputWhite" 0 255 255)])

(defmacro tones "The image's tones for a chart: each bin at most four times the average, so a peak flattens nothing." []
  '(map (fn [it] (min it (/ (* 4 (sum state.doc.histogram)) (count state.doc.histogram))))
        (or-else state.doc.histogram [])))

(defview adjust-slider "An adjustment's slider, id its node's, shown for one kind of adjustment: key names its value."
  [id kind key low high scale]
  `(node :id ~id
     :use tAdjustSlider
     :when ~(= computed.adjustmentKind kind)
     :with {:title ~(t (str "adjust." key))
            :key ~key
            :value ~(get computed.adjustment key)
            :low ~low
            :high ~high
            :scale ~scale}))

(defview curve-values "A Curves layer's curve, over the image's tones." []
  [`(curves#adjust-curve :label ~(t "adjust.curve")
      :when ~(= computed.adjustmentKind "curves")
      :width 200
      :height 140
      :value ~(merge {:points []} {:points computed.adjustment.curve})
      :accentColor @accent
      :histogram ~(tones)
      :onChange ~(handler setCurve event.value.points))])

(defview hue-values "A Hue/Saturation layer's hue, saturation and lightness." []
  [(adjust-slider "adjust-hue" "hue-saturation" "hue" -180 180 1)
   (adjust-slider "adjust-saturation" "hue-saturation" "saturation" -100 100 100)
   (adjust-slider "adjust-lightness" "hue-saturation" "lightness" -100 100 100)])

(defview balance-values "A Colour Balance layer's three channels." []
  [(balance-slider "adjust-cyan-red" "adjust.cyanRed" 0)
   (balance-slider "adjust-magenta-green" "adjust.magentaGreen" 1)
   (balance-slider "adjust-yellow-blue" "adjust.yellowBlue" 2)])

(defview balance-slider "A Colour Balance channel's slider, id its node's." [id title channel]
  `(node :id ~id
     :use tBalanceSlider
     :when ~(= computed.adjustmentKind "color-balance")
     :with {:title ~(t title) :channel ~channel}))

(defview brightness-values "A Brightness/Contrast layer's two sliders." []
  [(adjust-slider "adjust-brightness" "brightness-contrast" "brightness" -100 100 100)
   (adjust-slider "adjust-contrast" "brightness-contrast" "contrast" -100 100 100)])

(defview threshold-values "A Threshold layer's level, and what Invert and Gradient Map do, which have no values." []
  [(adjust-slider "adjust-level" "threshold" "level" 0 255 255)
   `(node#adjust-plain :style dim
      :fitText false
      :width "100%"
      :height 34
      :wrap true
      :when ~(or (= computed.adjustmentKind "invert") (= computed.adjustmentKind "gradient-map"))
      :text ~(if (= computed.adjustmentKind "invert") (t "adjust.invert.about") (t "adjust.gradientMap.about")))])

;;; A pixel layer's sections

(defview layer-sections "A pixel layer's sections, each folding: Transform, Align and Distribute, Quick Actions." []
  `(column#properties-sections :when ~(not computed.adjustment) :width "100%" :height 300 :gap 4
     (node#section-transform :use tSection :with {:title ~(t "section.transform") :key transform})
     ~@(transform-fields)
     (node :width "100%" :height 1 :color @border)
     (node#section-align :use tSection :with {:title ~(t "section.align") :key align})
     (row#align-icons :when ~state.props.align :width "100%" :height 24 :gap 2
       ~(align-icon "align-start-vertical" "left")
       ~(align-icon "align-center-vertical" "center")
       ~(align-icon "align-end-vertical" "right")
       ~(align-icon "align-start-horizontal" "top")
       ~(align-icon "align-center-horizontal" "middle")
       ~(align-icon "align-end-horizontal" "bottom"))
     (node :width "100%" :height 1 :color @border)
     (node#section-actions :use tSection :with {:title ~(t "section.actions") :key actions})
     (column#quick-actions :when ~state.props.actions :width "100%" :height 76 :gap 4
       (node#action-duplicate :use tButton
         :with {:title ~(t "command.duplicateActive") :tip ~(t "action.duplicate.tip") :run duplicateActive})
       (node#action-mask :use tButton
         :with {:title ~(t "command.addLayerMask") :tip ~(t "action.mask.tip") :run addLayerMask})
       (node#action-select-all :use tButton
         :with {:title ~(t "action.selectAll") :tip ~(t "action.selectAll.tip") :run selectAll}))))

(defview transform-fields "The Transform section: the box's width, height, x and y, the angle, and the flips." []
  [`(row :when ~state.props.transform :width "100%" :height 24 :gap 10 :alignItems center
      (node#transform-w :use tField
        :with {:title ~(t "transform.width") :key w :value ~(get computed.box 2) :unit " px" :pop false :width 66 :low 1
               :high 100000 :run transformBox :off ~(not= computed.activeLayer.kind "pixel")
               :tip ~(if state.doc.selection (t "transform.width.selection") (t "transform.width.image"))})
      ~(box-field "transform-h" "h" 3 1))
   `(row :when ~state.props.transform :width "100%" :height 24 :gap 10 :alignItems center
      ~(box-field "transform-x" "x" 0 -100000)
      ~(box-field "transform-y" "y" 1 -100000))
   `(row :when ~state.props.transform :width "100%" :height 24 :gap 6 :alignItems center
      (label :icon rotate-cw :textColor @muted :width 14 :height 20 :align center :tooltip ~(t "transform.angle"))
      (node#transform-angle :use tField
        :with {:title "" :key angle :value 0 :unit "°" :pop false :width 50 :low -360 :high 360 :run transformActive
               :off ~(not= computed.activeLayer.kind "pixel") :tip ~(t "transform.angle.tip")})
      (node :grow 1 :height 1 :style clear)
      (node#flip-horizontal :use tIcon
        :with {:icon flip-horizontal :tip ~(t "transform.flipHorizontal") :run flipLayerHorizontal
               :off ~(not= computed.activeLayer.kind "pixel")})
      (node#flip-vertical :use tIcon
        :with {:icon flip-vertical :tip ~(t "transform.flipVertical") :run flipLayerVertical
               :off ~(not= computed.activeLayer.kind "pixel")}))])

(defview box-field "A field of the box, its number at place in computed.box: a number typed moves or scales the layer."
  [id key place low]
  `(node :id ~id
     :use tField
     :with {:title ~(t (str "transform." key))
            :key ~key
            :value ~(get computed.box place)
            :unit " px"
            :pop false
            :width 66
            :low ~low
            :high 100000
            :run transformBox
            :off ~(not= computed.activeLayer.kind "pixel")
            :tip ~(t (str "transform." key ".tip"))}))

(defview align-icon "An align icon: the active layer's edge to the selection's, or the image's." [icon edge]
  `(node :id ~(str "align-" edge)
     :use tIcon
     :with {:icon ~icon
            :tip ~(if state.doc.selection
                    (t "align.toSelection" {:edges (t (str "align.edge." edge))})
                    (t "align.toImage" {:edges (t (str "align.edge." edge))}))
            :run alignActive
            :args {:edge ~edge}
            :off ~(not= computed.activeLayer.kind "pixel")}))

;;; The Adjust panel

(defview adjustments-panel "The Adjust panel: an icon for each kind of adjustment layer, adding one." []
  `(column#adjustments :when ~(= state.panels.properties "adjustments")
     :role region
     :label ~(t "panel.adjustments")
     :anchors {:left 10 :right 10 :top 8 :bottom 8}
     :gap 8
     (node :style dim :text ~(t "adjust.add"))
     (row#adjustment-icons :width "100%" :height 24 :gap 3
       ~(repeat [a state.adjustments]
         `(node :id ~(str "adjustment-" a.id)
            :use tIcon
            :with {:icon ~a.icon
                   :tip ~(t (str "adjustment." a.id))
                   :run addLayer
                   :args ~{:kind a.id :name (t (str "adjustment." a.id))}})))))
