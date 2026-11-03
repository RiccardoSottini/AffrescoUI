;;; options.lisp --- the options bar: the current tool, then what it offers, then the search and the workspace
;;;
;;; Under the menu bar, one rounded bar: the tool's name, then the options of the kind of tool chosen (painting,
;;; selecting, filling, gradients, cropping, moving, the view), each a control of controls.lisp. What isn't available
;;; yet is shown disabled, its tooltip saying so.

;;; The bar

(defview options-bar "The options bar: the current tool, its options, the search and the workspace switcher." []
  `(row#options-bar :role toolbar
     :label ~(t "options.label")
     :style card
     :anchors {:left 6 :right 6}
     :height 36
     :gap 8
     :alignItems center
     (node :width 4 :height 1 :style clear)
     ~(current-tool)
     ~(paint-options)
     ~(select-options)
     ~(fill-options)
     ~(gradient-options)
     ~(crop-options)
     (node#option-sample-all :use tCheck
       :when ~(= state.doc.tool "eyedropper")
       :with {:title ~(t "option.sampleAllLayers") :key allLayers :checked ~state.doc.options.allLayers})
     ~@(move-options)
     ~(view-options)
     (node :grow 1 :height 1 :style clear)
     ~(options-end)))

(defview current-tool "The current tool's icon and short name, in a pill at the bar's start." []
  `(row#options-tool :label ~(t "options.currentTool")
     :tooltip ~computed.toolTitle
     :height 26
     :gap 4
     :alignItems center
     :width ~(+ (* (count computed.toolName) 6.5) 46)
     :radius 999
     :color @accentSoft
     (label :icon ~computed.tool.icon :textColor @accent :width 26 :height 26 :align center)
     (label#options-tool-name :text ~computed.toolName :textColor @text :fitText true)))

;;; Painting

(defview paint-options "A painting tool's options: the brush, the mode, opacity and flow, and what isn't available yet." []
  `(row#options-paint :when ~computed.paints :alignItems center :gap 8 :height 30 :grow 100
     ~(brush-picker)
     (node#brush-settings :use tIcon :with {:icon sliders-horizontal :tip ~(t "option.brushSettings") :off true})
     (node :style sep)
     (node#paint-mode :use tPicker
       :with {:title ~(t "blend.normal")
              :tip ~(t "option.paintMode")
              :width 84
              :open state.paintModeMenu
              :items ~[{:title (t "blend.normal") :checked true}]})
     (node#option-opacity :use tField :with {:title ~(t "option.opacity") :key opacity :value ~state.doc.options.opacity})
     (node#opacity-pressure :use tIcon :with {:icon pencil-line :tip ~(t "option.opacityPressure") :off true})
     (node#option-flow :use tField :with {:title ~(t "option.flow") :key flow :value ~state.doc.options.flow})
     (node#airbrush :use tIcon :with {:icon spray-can :tip ~(t "option.airbrush") :off true})
     (node#option-smoothing :use tField
       :with {:title ~(t "option.smoothing") :key smoothing :value 0 :off true :width 40 :tip ~(t "option.smoothing.off")})
     (label :icon rotate-cw :textColor @muted :width 14 :height 20 :align center :tooltip ~(t "option.brushAngle"))
     (node#option-angle :use tField
       :with {:title "" :key angle :value 0 :unit "°" :pop false :off true :width 36 :tip ~(t "option.brushAngle.off")})
     (node#size-pressure :use tIcon :with {:icon pencil-line :tip ~(t "option.sizePressure") :off true})))

(defview brush-picker "The brush's button: its tip at its size and its size; it opens the brush's sizes and presets." []
  `(row#brush-picker :role button
     :label ~(t "option.brushPicker")
     :tooltip ~(t "option.brushPicker")
     :width 58
     :height 26
     :alignItems center
     :gap 3
     :radius @kit.radius
     :cursor hand
     :color ~(if state.brushMenu "@selected" "transparent")
     :hover {:color @hover}
     :onClick {:toggle state.brushMenu}
     (node :style clear :width 24 :height 24
       (node :let {:d ~(clamp (+ 4 (/ state.doc.options.size 6)) 4 20)}
         :x ~(- 12 (/ d 2))
         :y ~(- 12 (/ d 2))
         :width ~d
         :height ~d
         :radius 999
         :color @text
         :borderWidth ~(if (< state.doc.options.hardness 60) 2 0)
         :borderColor @muted))
     (label :font small :text ~state.doc.options.size :textColor @text :fitText true)
     (label :icon chevron-down :font small :textColor @muted :fitText true)
     ~(brush-popover)))

(defview brush-popover "Under the brush's button: its size, hardness and spacing, then the brush presets." []
  `(column#brush-popover :overlay {:placement below :open state.brushMenu :dismiss outside}
     :width 250
     :height 236
     :color @panel
     :radius @kit.cardRadius
     :borderWidth 1
     :borderColor @border
     :shadow {:y 3 :blur 10 :color "#00000060"}
     :gap 4
     ~(brush-option "size" 500 " px")
     ~(brush-option "hardness" 100 "%")
     ~(brush-option "spacing" 200 "%")
     (node :width "100%" :height 1 :color @border)
     (column :role list :label ~(t "option.brushPresets") :width "100%" :grow 1
       ~(repeat [b state.brushes]
         `(label :role listitem
            :label ~(t (str "brush." b.name))
            :text ~(t (str "brush." b.name))
            :width "100%"
            :height 21
            :padding 4
            :textColor @text
            :cursor hand
            :hover {:color @menuHover}
            :onClick ~(handler chooseBrush b.preset))))))

(defview brush-option "One of the brush's numbers, from 1 to high: its title, its slider and its value." [key high unit]
  `(row :id ~(str "option-" key) :width "100%" :height 26 :alignItems center :gap 8
     (node :width 2 :height 1 :style clear)
     (node :style dim :fitText false :text ~(str (t (str "option." key)) ":") :width 62 :height 16)
     (node :use tRange
       :with {:title ~(t (str "option." key)) :key ~key :value ~(get state.doc.options key) :low 1 :high ~high :width 112})
     (label :text ~(str (get state.doc.options key) unit) :textColor @text :fitText true)))

;;; Selecting and filling

(defview select-options "A selection tool's options: the mode, the wand's, Feather and Quick Mask." []
  `(row#options-select :when ~computed.selects :alignItems center :gap 8 :height 30 :grow 100
     (node#option-mode :use tChoice
       :with {:key mode
              :value ~state.doc.options.mode
              :tip ~(t "option.selectionMode")
              :tabs ~[{:id "replace" :icon "square" :title (t "selectionMode.replace")}
                      {:id "add" :icon "copy-plus" :title (t "selectionMode.add")}
                      {:id "subtract" :icon "copy-minus" :title (t "selectionMode.subtract")}
                      {:id "intersect" :icon "copy-x" :title (t "selectionMode.intersect")}]})
     (node :style sep)
     (node#option-wand-tolerance :use tField
       :when ~(= state.doc.tool "wand")
       :with {:title ~(t "option.tolerance") :key tolerance :value ~state.doc.options.tolerance :high 255 :unit ""
              :pop false :width 36})
     (node#option-anti-alias :use tCheck :with {:title ~(t "option.antiAlias") :checked true :off true})
     (node#option-wand-contiguous :use tCheck
       :when ~(= state.doc.tool "wand")
       :with {:title ~(t "option.contiguous") :key contiguous :checked ~state.doc.options.contiguous})
     (node#option-wand-all :use tCheck
       :when ~(= state.doc.tool "wand")
       :with {:title ~(t "option.sampleAllLayers") :key allLayers :checked ~state.doc.options.allLayers})
     (node#option-feather :use tButton
       :with {:title ~(t "option.feather") :tip ~(t "option.feather.tip") :run featherSelection
              :off ~(not state.doc.selection)})
     (node#option-quick-mask :use tButton
       :with {:title ~(t "option.quickMask") :tip ~(t "option.quickMask.tip") :run toggleQuickMask})))

(defview fill-options "The paint bucket's options: the source, opacity, tolerance and what it fills." []
  `(row#options-fill :when ~(= state.doc.tool "fill") :alignItems center :gap 8 :height 30 :grow 100
     (node#fill-source :use tPicker
       :with {:title ~(t "option.foreground")
              :tip ~(t "option.fillSource")
              :width 90
              :open state.fillSourceMenu
              :items ~[{:title (t "option.foreground") :checked true}]})
     (node#option-fill-opacity :use tField
       :with {:title ~(t "option.opacity") :key fillOpacity :value ~state.doc.options.fillOpacity})
     (node#option-fill-tolerance :use tField
       :with {:title ~(t "option.tolerance") :key tolerance :value ~state.doc.options.tolerance :high 255 :unit ""
              :pop false :width 36})
     (node#option-fill-anti-alias :use tCheck :with {:title ~(t "option.antiAlias") :checked true :off true})
     (node#option-fill-contiguous :use tCheck
       :with {:title ~(t "option.contiguous") :key contiguous :checked ~state.doc.options.contiguous})
     (node#option-fill-all :use tCheck
       :with {:title ~(t "option.allLayers") :key allLayers :checked ~state.doc.options.allLayers})))

;;; Gradients, cropping, moving and the view

(defview gradient-options "The gradient tool's options: the gradient, its presets and shape, mode and opacity." []
  `(row#options-gradient :when ~(= state.doc.tool "gradient") :alignItems center :gap 8 :height 30 :grow 100
     (gradient#option-gradient :label ~(t "option.gradient")
       :width 120
       :height 22
       :focusable true
       :value ~state.gradientEdit
       :accentColor @accent
       :onChange ~(handler setGradient event.value))
     (node#gradient-preset :use tPicker
       :with {:title ""
              :width 20
              :tip ~(t "option.gradientPresets")
              :open state.gradientMenu
              :items ~(map (fn [it] {:title (t (str "gradient." it.name)) :run "setOptions"
                                     :with {:args {:gradientPreset it.preset}}})
                           state.gradients)})
     (node#option-shape :use tChoice
       :with {:key shape
              :value ~state.doc.options.shape
              :tip ~(t "option.gradientShape")
              :tabs ~[{:id "linear" :icon "align-vertical-space-around" :title (t "shape.linear")}
                      {:id "radial" :icon "circle-dot" :title (t "shape.radial")}
                      {:id "angle" :icon "rotate-cw" :title (t "shape.angle")}
                      {:id "reflected" :icon "flip-vertical" :title (t "shape.reflected")}
                      {:id "diamond" :icon "diamond" :title (t "shape.diamond")}]})
     (node :style sep)
     (node#gradient-mode :use tPicker
       :with {:title ~(t "blend.normal") :tip ~(t "option.gradientMode") :width 84 :open state.paintModeMenu
              :items ~[{:title (t "blend.normal") :checked true}]})
     (node#option-gradient-opacity :use tField
       :with {:title ~(t "option.opacity") :key fillOpacity :value ~state.doc.options.fillOpacity})
     (node#gradient-reverse :use tCheck :with {:title ~(t "option.reverse") :checked false :off true})
     (node#gradient-dither :use tCheck :with {:title ~(t "option.dither") :checked false :off true})
     (node#gradient-transparency :use tCheck :with {:title ~(t "option.transparency") :checked true :off true})))

(defview crop-options "The crop tool's ratios: the selection's box, or a centred box of a ratio." []
  `(node#option-crop :use tPicker
     :when ~(= state.doc.tool "crop")
     :with {:title ~(t "crop.ratio")
            :tip ~(t "crop.tip")
            :open state.cropMenu
            :items ~[{:title (t "crop.toSelection") :run "cropToSelection"}
                     {:title (t "crop.square") :run "cropToRatio" :with {:args {:width 1 :height 1}}}
                     {:title "4 : 3" :run "cropToRatio" :with {:args {:width 4 :height 3}}}
                     {:title "3 : 2" :run "cropToRatio" :with {:args {:width 3 :height 2}}}
                     {:title "16 : 9" :run "cropToRatio" :with {:args {:width 16 :height 9}}}
                     {:title (t "crop.portrait") :run "cropToRatio" :with {:args {:width 4 :height 5}}}]}))

(defview move-options "The move tool's Auto-Select, and what dragging moves." []
  [`(node#option-auto-select :use tCheck
      :when ~(= state.doc.tool "move")
      :with {:title ~(t "option.autoSelect") :key autoSelect :checked ~state.doc.options.autoSelect})
   `(node#option-move :style dim
      :when ~(= state.doc.tool "move")
      :text ~(if state.doc.options.autoSelect (t "move.underPointer") (t "move.active")))])

(defview view-options "The hand's and the zoom's options: zoom in, zoom out, and 100% or fit." []
  `(row#options-view :when ~(or (= state.doc.tool "hand") (= state.doc.tool "zoom")) :alignItems center :gap 4 :height 30
     :grow 100
     (node#view-zoom-in :use tIcon :with {:icon zoom-in :run zoomIn :tip ~(t "command.zoomIn")})
     (node#view-zoom-out :use tIcon :with {:icon zoom-out :run zoomOut :tip ~(t "command.zoomOut")})
     (node#view-actual :use tPicker
       :with {:title "100%"
              :width 50
              :tip ~(t "option.zoom")
              :open state.zoomMenu
              :items ~[{:title "100%" :run "actualPixels"} {:title (t "option.fitScreen") :run "fitOnScreen"}]})))

;;; The bar's end

(defview options-end "The bar's end: the command search, and the workspace switcher." []
  `(row#options-end :width 112 :height 30 :alignItems center :justify end :gap 4
     (node#search :use tIcon :with {:icon search :tip ~(t "option.search") :run showSearch})
     (row#workspace-switcher :role button
       :label ~(t "workspace.label")
       :tooltip ~(t "workspace.choose")
       :width 40
       :height 22
       :alignItems center
       :gap 2
       :radius @kit.radius
       :cursor hand
       :hover {:color @hover}
       :onClick {:toggle state.workspaceMenu}
       (label :icon layout-panel-left :textColor @text :width 22 :height 20 :align center)
       (label :icon chevron-down :font small :textColor @muted :fitText true)
       (node :use tMenu
         :with {:open state.workspaceMenu
                :placement below-end
                :width 200
                :items ~[{:title (t "workspace.standard") :checked true :run "resetWorkspace"}
                         {:separator true}
                         {:title (t "command.resetWorkspace") :run "resetWorkspace"}]}))
     (node :width 6 :height 1 :style clear)))
