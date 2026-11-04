;;; panels.lisp --- the dock on the window's right: three groups of panels, and the icons of two that pop out
;;;
;;; Each group is a card with a row of pill tabs (Colour | Palette | Gradients | Patterns, Properties | Adjust,
;;; Layers | Channels | Paths) over the panel chosen; the strip at the window's edge opens History and the Navigator
;;; beside it. A feature docks under the groups, at the panels.dock point (offers.lisp). The Properties and Adjust
;;; panels are in properties.lisp, the Layers and Channels panels in layers.lisp.

;;; The groups

(defcomponent tGroup "A group of the dock: pill tabs for its panels, and the panel chosen under them (the children)."
  {:slot "" :tabs []}
  `(column :id ~(str "group-" slot) :width "100%" :color @panel :radius @kit.cardRadius
     (row :id ~(str "panel-tabs-" slot)
       :role tablist
       :label ~(t "panels.label")
       :width "100%"
       :height 32
       :gap 2
       :alignItems center
       :arrows horizontal
       (node :width 4 :height 1 :style clear)
       ~(repeat [tab tabs] (panel-tab tab)))
     (node :id ~(str "panel-" slot) :style clear :width "100%" :grow 1
       (node :slot true))))

(defview panel-tab "A group's tab: a pill, filled while its panel is shown." [tab]
  `(label :id ~(str "panel-tab-" tab.id)
     :role tab
     :label ~tab.label
     :focusable true
     :selected ~(= (get state.panels slot) tab.id)
     :text ~tab.label
     :width ~(+ (* (count tab.label) 6) 14)
     :height 22
     :align center
     :radius 999
     :cursor hand
     :color ~(if (= (get state.panels slot) tab.id) "@accentSoft" "transparent")
     :textColor ~(if (= (get state.panels slot) tab.id) "@text" "@muted")
     :hover {:textColor @text}
     :focused {:borderWidth 1 :borderColor @accent}
     :onClick ~(handler showPanel {:slot slot :id tab.id})))

(defcomponent tPopout "An icon of the strip, and the panel (the children) it pops out to its left."
  {:name "" :icon "" :title "" :height 300}
  `(node :id ~(str name "-popout-anchor") :style clear :width 30 :height 28
     (node :id ~(str name "-button")
       :use tIcon
       :width 30
       :height 28
       :with {:icon ~icon :tip ~title :on ~(get state.popout name) :run showPopout :args {:id ~name}})
     (column :id ~(str name "-popout")
       :role region
       :label ~title
       :width 250
       :height ~height
       :color @panel
       :radius @kit.cardRadius
       :borderWidth 1
       :borderColor @border
       :shadow {:y 4 :blur 14 :color "#00000060"}
       :overlay {:placement left :open ~(str "state.popout." name) :dismiss outside}
       (label :text ~title :textColor @text :width "100%" :height 28 :padding 10)
       (node :style clear :width "100%" :grow 1
         (node :slot true)))))

;;; The dock

(defview dock "The dock, in the standard screen mode: the groups, and the strip of panels that pop out." []
  `(row#dock :when ~(= state.screenMode "standard") :width 304 :height "100%" :gap 6
     (column#panels :point panels.dock
       :role complementary
       :label ~(t "panels.label")
       :width 262
       :height "100%"
       :color transparent
       :gap 6
       ~(colour-group)
       ~(properties-group)
       ~(layers-group))
     (column#panel-icons :role toolbar
       :label ~(t "panels.collapsed")
       :width 36
       :height "100%"
       :color @panel
       :radius @kit.cardRadius
       :alignItems center
       :gap 2
       (node :height 6 :width 1 :style clear)
       (node :use tPopout :with {:name history :icon history :title ~(t "panel.history")}
         ~(history-panel))
       (node :use tPopout :with {:name navigator :icon compass :title ~(t "panel.navigator") :height 250}
         ~(navigator-panel)))))

(defview colour-group "The first group: the colours, the palette, the gradients and the patterns." []
  `(node :use tGroup
     :height 158
     :with {:slot colour
            :tabs ~[{:id "colour" :label (t "panel.colour")}
                    {:id "swatches" :label (t "panel.swatches")}
                    {:id "gradients" :label (t "panel.gradients")}
                    {:id "patterns" :label (t "panel.patterns")}]}
     ~(colour-panel)
     ~(swatches-panel)
     ~(gradients-panel)
     (node#patterns-empty :style dim
       :when ~(= state.panels.colour "patterns")
       :anchors {:left 10 :top 10}
       :text ~(t "panel.patterns.none"))))

(defview properties-group "The second group: the active layer's properties, and the adjustments to add." []
  `(node :use tGroup
     :height 236
     :shrink 1
     :minHeight 96
     :with {:slot properties
            :tabs ~[{:id "properties" :label (t "panel.properties")} {:id "adjustments" :label (t "panel.adjust")}]}
     ~(properties-panel)
     ~(adjustments-panel)))

(defview layers-group "The third group: the layers, the channels and the paths." []
  `(node :use tGroup
     :grow 1
     :minHeight 200
     :with {:slot layers
            :tabs ~[{:id "layers" :label (t "panel.layers")}
                    {:id "channels" :label (t "panel.channels")}
                    {:id "paths" :label (t "panel.paths")}]}
     ~(layers-panel)
     ~(channels-panel)
     (node#paths-empty :style dim
       :when ~(= state.panels.layers "paths")
       :anchors {:left 10 :top 10}
       :text ~(t "panel.paths.none"))))

;;; Colours

(defview colour-panel "The Colour panel: which colour is edited, and its picker." []
  `(node#colour :when ~(= state.panels.colour "colour") :style clear :fill true
     (node#colour-chips :role group :label ~(t "colour.edited") :style clear :x 8 :y 8 :width 36 :height 36
       ~(edit-chip "colour-chip-background" "background" 'state.doc.background 13)
       (node :x 0 :y 0 :width 24 :height 24 :radius 999 :color @panel)
       ~(edit-chip "colour-chip-foreground" "foreground" 'state.doc.foreground 1))
     (colorPicker#colour-picker
       :label ~(if (= state.colourTarget "background") (t "colour.background") (t "colour.foreground"))
       :anchors {:left 52 :right 8 :top 6 :bottom 6}
       :value ~(if (= state.colourTarget "background") state.doc.background state.doc.foreground)
       :fields false
       :alpha false
       :textColor @text
       :accentColor @accent
       :onChange ~(handler setOption state.colourTarget event.value))))

(defview edit-chip "A colour's chip, at an offset: a click makes it the colour the picker edits, outlined."
  [id target value at]
  `(node :id ~id
     :role button
     :label ~(t (str "colour.edit." target))
     :tooltip ~(t (str "colour." target))
     :x ~at
     :y ~at
     :width 22
     :height 22
     :radius 999
     :color ~value
     :cursor hand
     :borderWidth ~(if (= state.colourTarget target) 2 1)
     :borderColor ~(if (= state.colourTarget target) "@text" "@muted")
     :onClick {:set state.colourTarget :to ~target}))

(defview swatches-panel "The Palette panel: a grid of colours; a click makes one the foreground." []
  `(grid#swatches :when ~(= state.panels.colour "swatches")
     :role group
     :label ~(t "panel.swatches")
     :columns 12
     :gap 3
     :anchors {:left 8 :right 8 :top 8}
     :height 40
     ~(repeat [swatch state.swatches :index index]
       `(node :id ~(str "swatch-" index)
          :role button
          :label ~(t "swatch.label" {:colour swatch})
          :tooltip ~swatch
          :width 17
          :height 17
          :color ~swatch
          :borderWidth 1
          :borderColor @border
          :cursor hand
          :hover {:borderColor @text}
          :onClick ~(handler setOptions {:foreground swatch})))))

(defview gradients-panel "The Gradients panel: the presets; a click gives one to the gradient tool, and chooses it." []
  `(column#gradients :when ~(= state.panels.colour "gradients")
     :role list
     :label ~(t "option.gradientPresets")
     :anchors {:left 0 :right 0 :top 4 :bottom 4}
     :scroll vertical
     ~(repeat [g state.gradients]
       `(label :role listitem
          :label ~(t (str "gradient." g.name))
          :text ~(t (str "gradient." g.name))
          :icon blend
          :width "100%"
          :height 22
          :padding 4
          :textColor @text
          :cursor hand
          :hover {:color @hover}
          :onClick ~(handler chooseGradient g.preset)))))

;;; The panels that pop out

(defview history-panel "The History panel: the steps, the current one chosen; a click goes back or forward to one." []
  `(column#history :fill true
     (list#history-list :label ~(t "panel.history")
       :width "100%"
       :grow 1
       :color @panel
       :rowHeight 24
       :items ~state.doc.history
       :value ~(text state.doc.position)
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @selected
       :accentColor @accent
       :onChange ~(handler goToHistory {:position (number event.value)}))
     (node#history-dropped :style dim
       :fitText false
       :when ~(> state.doc.dropped 0)
       :font small
       :text ~(t "history.dropped" {:n (text state.doc.dropped)})
       :width "100%"
       :height 18
       :padding 4)))

(defview navigator-panel "The Navigator panel: the whole image small, and the zoom." []
  `(column#navigator :fill true :gap 6 :alignItems center
     (node :height 4 :width 1 :style clear)
     (image#navigator-picture :label ~(t "panel.navigator")
       :source "doc/navigator"
       :fit contain
       :width 234
       :height 170
       :color @canvasBack)
     (row#navigator-zoom :height 26 :gap 4 :alignItems center
       (label#navigator-percent :text ~(str computed.zoomPercent "%") :textColor @text :width 56 :height 22)
       (node#navigator-out :use tIcon :with {:icon zoom-out :run zoomOut :tip ~(t "command.zoomOut")})
       (node#navigator-fit :use tIcon :with {:icon maximize :run fitOnScreen :tip ~(t "command.fitOnScreen")})
       (node#navigator-in :use tIcon :with {:icon zoom-in :run zoomIn :tip ~(t "command.zoomIn")}))))
