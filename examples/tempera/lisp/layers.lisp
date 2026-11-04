;;; layers.lisp --- the Layers and Channels panels
;;;
;;; The Layers panel: a filter by kind, the active layer's blend mode, opacity and locks, then one row per layer, top
;;; first (computed.layerRows: a folded group's layers hidden, the filter applied), and the buttons that add and delete
;;; layers. A row is dragged onto another to move it there. The Channels panel lists the image's channels.

;;; The Layers panel

(defview layers-panel "The Layers panel: its filter, blend and lock rows, the layers, and its buttons." []
  `(column#layers :when ~(= state.panels.layers "layers") :role region :label ~(t "panel.layers") :fill true
     ~(layers-filter-row)
     ~(layers-blend-row)
     ~(layers-lock-row)
     (node :width "100%" :height 1 :color @border)
     (column#layer-rows :role list :label ~(t "panel.layers") :width "100%" :grow 1 :scroll vertical
       ~(repeat [layer computed.layerRows :key layer.id] (layer-row layer)))
     ~(layer-buttons)))

(defview layers-filter-row "The filter: show only some kinds of layer, or all of them." []
  `(row#layers-filter-row :role toolbar :label ~(t "layers.filter") :height 30 :width "100%" :gap 1 :alignItems center
     (node :width 6 :height 1 :style clear)
     (node#layer-filter-kind :use tPicker
       :with {:title ~(t "layers.kind") :tip ~(t "layers.kind.tip") :width 70 :open state.layerKindMenu
              :items ~[{:title (t "layers.kind") :checked true}]})
     (node :width 6 :height 1 :style clear)
     ~(kind-filter "pixel" "image")
     ~(kind-filter "adjustment" "contrast")
     ~(kind-filter "group" "folder")
     (node#filter-kind-type :use tIcon :with {:icon type :tip ~(t "layers.typeLayers") :off true})
     (node#filter-kind-shape :use tIcon :with {:icon shapes :tip ~(t "layers.shapeLayers") :off true})
     (node :grow 1 :height 1 :style clear)
     (node#layer-filter-toggle :use tIcon
       :with {:icon ~(if state.layerFilter.on "toggle-right" "toggle-left")
              :tip ~(if state.layerFilter.on (t "layers.filterOff") (t "layers.filterOn"))
              :run toggleLayerFilter})
     (node :width 4 :height 1 :style clear)))

(defview kind-filter "The filter's icon for one kind of layer: on while the filter shows that kind." [kind icon]
  `(node :id ~(str "filter-kind-" kind)
     :use tIcon
     :with {:icon ~icon
            :tip ~(t (str "layers.only." kind))
            :on ~(and state.layerFilter.on (contains state.layerFilter.kinds kind))
            :run toggleLayerKind
            :args {:kind ~kind}}))

(defview layers-blend-row "The active layer's blend mode and opacity." []
  `(row#layers-blend-row :height 30 :width "100%" :gap 6 :alignItems center
     (node :width 2 :height 1 :style clear)
     (node#layer-blend :use tPicker
       :with {:title ~computed.blendTitle
              :tip ~(t "layers.blend")
              :width 104
              :open state.blendMenu
              :items ~(map (fn [it]
                             (if (= it "-")
                               {:separator true}
                               {:title (t (str "blend." it))
                                :checked (= it computed.activeLayer.blend)
                                :run "setLayer"
                                :with {:args {:blend it}}}))
                           state.blends)})
     (node :grow 1 :height 1 :style clear)
     (node#layer-opacity :use tField
       :with {:title ~(t "option.opacity") :key opacity :value ~(or-else computed.activeLayer.opacity 100) :run setLayer
              :width 40})))

(defview layers-lock-row "The active layer's locks, and its fill." []
  `(row#layers-lock-row :height 26 :width "100%" :gap 2 :alignItems center
     (node :width 6 :height 1 :style clear)
     (node :style dim :text ~(t "layers.lock"))
     (node#lock-transparency :use tIcon :with {:icon grid-2-x-2 :tip ~(t "layers.lockTransparency") :off true})
     ~(lock-icon "lock-pixels" "brush" "lockPixels" "layers.lockPixels")
     ~(lock-icon "lock-position" "move" "lockPosition" "layers.lockPosition")
     (node#lock-artboard :use tIcon :with {:icon frame :tip ~(t "layers.lockArtboard") :off true})
     ~(lock-icon "lock-all" "lock" "lockAll" "layers.lockAll")
     (node :grow 1 :height 1 :style clear)
     (node#layer-fill :use tField
       :with {:title ~(t "layers.fill") :key fill :value 100 :run setLayer :width 40 :off true
              :tip ~(t "layers.fill.off")})))

(defview lock-icon "A lock of the active layer, key its name: on while it is locked; a click turns it."
  [id icon key tip]
  `(node :id ~id
     :use tIcon
     :with {:icon ~icon
            :tip ~(t tip)
            :on ~(get computed.activeLayer key)
            :run setLayer
            :args ~(fromEntries [{:key key :value (not (get computed.activeLayer key))}])}))

;;; A layer's row

(defview layer-row "A layer's row: chosen by a click, moved by a drag onto another." [layer]
  `(row :id ~(str "layer-" layer.id)
     :role listitem
     :label ~layer.name
     :selected ~(= layer.id state.doc.active)
     :anchors {:left 4 :right 4}
     :height 36
     :alignItems center
     :gap 5
     :cursor hand
     :radius @kit.radius
     :color ~(if (= layer.id state.doc.active) "@selected" "transparent")
     :hover {:color @hover}
     :drag {:data {:id ~layer.id}}
     :onDrop ~(handler moveLayer {:id event.data.id :onto layer.id})
     :onClick ~(handler selectLayer {:id layer.id})
     (node :width 3 :height 22 :radius 2 :color ~(if (= layer.id state.doc.active) "@accent" "transparent"))
     (label :id ~(str "layer-eye-" layer.id)
       :role button
       :label ~(t "layers.visibility" {:name layer.name})
       :icon ~(if layer.visible "eye" "")
       :width 28
       :height 36
       :align center
       :cursor hand
       :textColor @text
       :onClick ~(handler setLayer {:id layer.id :visible (not layer.visible)}))
     (node :style clear :width ~(* layer.depth 14) :height 1)
     ~@(layer-badges layer)
     ~@(layer-picture layer)
     ~@(layer-name layer)
     (node :width 6 :height 1 :style clear)))

(defview layer-badges "A group's fold, and the mark of a layer clipped to the one below." [layer]
  [`(label :id ~(str "layer-fold-" layer.id)
      :when ~(= layer.kind "group")
      :role button
      :focusable true
      :label ~(if (contains state.foldedGroups layer.id)
                (t "layers.openGroup" {:name layer.name})
                (t "layers.closeGroup" {:name layer.name}))
      :isOpen ~(not (contains state.foldedGroups layer.id))
      :icon ~(if (contains state.foldedGroups layer.id) "chevron-right" "chevron-down")
      :textColor @muted
      :width 12
      :height 36
      :cursor hand
      :onClick ~(handler foldGroup {:id layer.id}))
   `(label :id ~(str "layer-clip-" layer.id)
      :when ~layer.clipped
      :icon corner-left-down
      :textColor @muted
      :width 14
      :tooltip ~(t "layers.clipped"))])

(defview layer-picture "A layer's thumbnail, or its kind's icon, then its mask, linked." [layer]
  [`(image :id ~(str "layer-thumb-" layer.id)
      :when ~(not (empty? layer.thumbnail))
      :source ~layer.thumbnail
      :fit contain
      :width 30
      :height 28
      :color @canvasBack
      :borderWidth 1
      :borderColor @border)
   `(label :id ~(str "layer-icon-" layer.id)
      :when ~(empty? layer.thumbnail)
      :icon ~(if (= layer.kind "group") "folder" "contrast")
      :font iconsLarge
      :width 30
      :height 28
      :align center
      :textColor @text)
   `(label :id ~(str "layer-mask-" layer.id)
      :when ~layer.mask
      :icon link
      :font small
      :textColor @muted
      :width 10
      :tooltip ~(t "layers.maskLinked"))
   `(node :when ~layer.mask
      :width 30
      :height 28
      :color "#ffffff"
      :borderWidth 1
      :borderColor @border
      :tooltip ~(t "layers.mask"))])

(defview layer-name "A layer's name, and a lock if any of its locks is on." [layer]
  [`(label :id ~(str "layer-name-" layer.id)
      :text ~layer.name
      :textColor @text
      :grow 1
      :height 36
      :align {:horizontal start :vertical center})
   `(label :id ~(str "layer-lock-" layer.id)
      :when ~(or layer.lockAll layer.lockPixels layer.lockPosition)
      :icon lock
      :textColor @muted
      :width 16
      :tooltip ~(t "layers.locked"))])

;;; The buttons

(defview layer-buttons "The panel's buttons: masks, adjustments, a group, a layer, and delete." []
  `(row#layer-buttons :role toolbar :label ~(t "layers.actions") :width "100%" :height 28 :alignItems center
     :justify end :gap 2
     (node#layer-link :use tIcon :with {:icon link :tip ~(t "layers.link") :off true})
     (node#layer-fx :use tIcon :with {:icon sparkles :tip ~(t "layers.style") :off true})
     (node#layer-add-mask :use tIcon :with {:icon circle-dot :tip ~(t "layers.addMask") :run addLayerMask})
     (node#layer-adjust :use tIcon
       :with {:icon contrast :tip ~(t "layers.newAdjustment") :on ~state.adjustMenu}
       :onClick [{:toggle state.adjustMenu}]
       (node#adjust-menu :use menu
         :with {:open state.adjustMenu
                :placement above
                :width 200
                :icons true
                :items ~(map (fn [it]
                               {:title (str (t (str "adjustment." it.id)) "...")
                                :icon it.icon
                                :run "addLayer"
                                :with {:args {:kind it.id :name (t (str "adjustment." it.id))}}})
                             state.adjustments)}))
     (node#layer-new-group :use tIcon :with {:icon folder :tip ~(t "layers.newGroup") :run newGroup})
     (node#layer-new :use tIcon :with {:icon plus-square :tip ~(t "layers.newLayer") :run newLayer})
     (node#layer-delete :use tIcon :with {:icon trash-2 :tip ~(t "layers.delete") :run deleteActive})
     (node :width 6 :height 1 :style clear)))

;;; The Channels panel

(defview channels-panel "The Channels panel: the composite and the red, green and blue channels." []
  `(column#channels :when ~(= state.panels.layers "channels") :role list :label ~(t "panel.channels") :fill true
     ~@(map-indexed channel-row ["rgb" "red" "green" "blue"])
     (node :style dim :anchors {:left 10} :text ~(t "channel.notYet"))))

(defview channel-row "A channel's row, i its place: the composite first, chosen, with the image." [i channel]
  `(row :role listitem
     :label ~(t (str "channel." channel))
     :width "100%"
     :height 34
     :alignItems center
     :gap 8
     :color ~(if (= i 0) "@selected" "transparent")
     (label :icon eye :textColor @text :width 26 :height 20 :align center)
     (image :source "doc/navigator" :fit contain :width 30 :height 26 :color @canvasBack :visible ~(= i 0))
     (label :text ~(t (str "channel." channel)) :textColor @text :grow 1 :height 20)
     (node :style dim :text ~(t "channel.key" {:n (str (+ i 2))}))
     (node :width 6 :height 1 :style clear)))
