{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Tempera's panels and dialogs. The dock on the right holds three rounded groups, each with a row of pill tabs (Colour | Palette | Gradients | Patterns, Properties | Adjust, Layers | Channels | Paths), and an icon strip at the window's far edge opens History and the Navigator. Every change goes through a tempera.* native, which returns the new snapshot into state.doc; the filter dialog only edits state.filter, and watches (parts/commands.json) preview, apply or cancel it."
  :computed {:adjustment "{{computed.activeLayer.adjustment}}"
    :box "{{state.doc.selection ? state.doc.bounds : [0, 0, state.doc.width, state.doc.height]}}"
    :layerFolded "{{map(scan(state.doc.layers, acc.cut < it.depth ? {cut: acc.cut, hide: true} : {cut: it.kind == 'group' && contains(state.foldedGroups, it.id) ? it.depth : 999, hide: false}, {cut: 999, hide: false}), it.hide)}}"
    :layerShown "{{filter(state.doc.layers, !computed.layerFolded[index])}}"
    :layerRows "{{state.layerFilter.on && !empty(state.layerFilter.kinds) ? filter(computed.layerShown, contains(state.layerFilter.kinds, it.kind)) : computed.layerShown}}"
    :adjustmentKind "{{computed.activeLayer.adjustment.kind ?? ''}}"
    :filterTitle "{{find(state.filters, it.id == state.filter.kind).title ?? 'Filter'}}"
    :blendTitle "{{join(map(split(computed.activeLayer.blend ?? 'normal', '-'), upper(slice(it, 0, 1)) + slice(it, 1)), ' ')}}"}
  :components {:tGroup {:params {:slot "" :tabs []}
      :node (column #"group-{{slot}}" :width "100%" :color @panel :radius @kit.cardRadius
        (row #"panel-tabs-{{slot}}"
          :role tablist
          :label Panels
          :width "100%"
          :height 32
          :gap 2
          :alignItems center
          :arrows horizontal
          {:width 4 :height 1 :style clear}
          (label :repeat {:for t :in "{{tabs}}"}
            #"panel-tab-{{t.id}}"
            :role tab
            :label "{{t.label}}"
            :focusable true
            :selected "{{state.panels[slot] == t.id}}"
            :text "{{t.label}}"
            :width "{{length(t.label) * 6 + 14}}"
            :height 22
            :align center
            :radius 999
            :cursor hand
            :color "{{state.panels[slot] == t.id ? '@accentSoft' : 'transparent'}}"
            :textColor "{{state.panels[slot] == t.id ? '@text' : '@muted'}}"
            :hover {:textColor @text}
            :focused {:borderWidth 1 :borderColor @accent}
            :onClick {:run showPanel :with {:args (node :slot "{{slot}}" #"{{t.id}}")}}))
        (node #"panel-{{slot}}" :style clear :width "100%" :grow 1
          {:slot true}))}
    :tPopout {:params {:name "" :icon "" :title "" :height 300}
      :node (node #"{{name}}-popout-anchor" :style clear :width 30 :height 28
        (node :use tIcon
          #"{{name}}-button"
          :width 30
          :height 28
          :with {:icon "{{icon}}"
            :tip "{{title}}"
            :on "{{state.popout[name]}}"
            :run showPopout
            :args (node #"{{name}}")})
        (column #"{{name}}-popout"
          :role region
          :label "{{title}}"
          :width 250
          :height "{{height}}"
          :color @panel
          :radius @kit.cardRadius
          :borderWidth 1
          :borderColor @border
          :shadow {:y 4 :blur 14 :color "#00000060"}
          :overlay {:placement left :open "{{'state.popout.' + name}}" :dismiss outside}
          {:type label :text "{{title}}" :textColor @text :width "100%" :height 28 :padding 10}
          {:style clear :width "100%" :grow 1 :children [{:slot true}]}))}
    :tAdjustSlider {:params {:title "" :key "" :value 0 :min 0 :max 100 :scale 100}
      :node {:type row
        :height 26
        :alignItems center
        :gap 6
        :width "100%"
        :children [{:style dim :fitText false :text "{{title}}" :width 74 :height 20}
          {:use tRange
            :with {:title "{{title}}"
              :key "{{key}}"
              :value "{{value}}"
              :min "{{min}}"
              :max "{{max}}"
              :scale "{{scale}}"
              :run setAdjustment
              :width 112}}
          {:type label
            :text "{{round(value * scale)}}"
            :textColor @text
            :width 34
            :height 20
            :align {:horizontal end :vertical center}}]}}
    :tBalanceSlider {:params {:title "" :channel 0}
      :node {:type row
        :height 26
        :alignItems center
        :gap 6
        :width "100%"
        :children [{:style dim :fitText false :text "{{title}}" :width 74 :height 20}
          {:use tRange
            :with {:title "{{title}}"
              :key value
              :value "{{computed.adjustment.balance[channel]}}"
              :min -100
              :max 100
              :scale 100
              :run setBalance
              :args {:channel "{{channel}}"}
              :width 112}}
          {:type label
            :text "{{round(computed.adjustment.balance[channel] * 100)}}"
            :textColor @text
            :width 34
            :height 20
            :align {:horizontal end :vertical center}}]}}
    :tFilterSlider {:params {:title "" :key "" :min 0 :max 100}
      :node {:type row
        :height 28
        :alignItems center
        :gap 8
        :width "100%"
        :children [{:style dim :fitText false :text "{{title}}" :width 80 :height 20}
          (node :use tRange
            #"filter-{{key}}"
            :with {:title "{{title}}"
              :key "{{key}}"
              :value "{{state.filter[key]}}"
              :min "{{min}}"
              :max "{{max}}"
              :run setFilterValue
              :width 196})
          {:type textarea
            :label "{{title}} value"
            :text "{{round(state.filter[key])}}"
            :width 40
            :height 20
            :padding 3
            :textColor @text
            :color @input
            :borderWidth 1
            :borderColor @border
            :radius @kit.radius
            :onSubmit {:run setFilterValue
              :with {:args "{{parseJson('{' + json(key) + ': ' + json(clamp(number(trim(event.text)) ?? state.filter[key], min, max)) + '}')}}"}}}]}}
    :tSection {:params {:title "" :key ""}
      :node {:type row
        :role button
        :label "{{title}}"
        :selected "{{state.props[key]}}"
        :width "100%"
        :height 24
        :alignItems center
        :gap 4
        :cursor hand
        :onClick {:toggle "{{'state.props.' + key}}"}
        :children [{:type label
            :icon "{{state.props[key] ? 'chevron-down' : 'chevron-right'}}"
            :textColor @muted
            :width 14
            :height 20
            :align center}
          {:type label :text "{{title}}" :textColor @text :fitText true}]}}
    :temperaPanels {:node (row #dock :width 304 :height "100%" :gap 6
        (column #panels
          :point panels.dock
          :role complementary
          :label Panels
          :width 262
          :height "100%"
          :color transparent
          :gap 6
          {:use tGroup
            :height 158
            :with {:slot colour
              :tabs [(node #colour :label Colour)
                (node #swatches :label Palette)
                (node #gradients :label Gradients)
                (node #patterns :label Patterns)]}
            :children [{:use temperaColour :when "{{state.panels.colour == 'colour'}}"}
              {:use temperaSwatches :when "{{state.panels.colour == 'swatches'}}"}
              {:use temperaGradients :when "{{state.panels.colour == 'gradients'}}"}
              (node :style dim
                #patterns-empty
                :when "{{state.panels.colour == 'patterns'}}"
                :anchors {:left 10 :top 10}
                :text "No patterns: pattern fills aren't available yet.")]}
          {:use tGroup
            :height 236
            :shrink 1
            :minHeight 96
            :with {:slot properties :tabs [(node #properties :label Properties) (node #adjustments :label Adjust)]}
            :children [{:use temperaProperties :when "{{state.panels.properties == 'properties'}}"}
              {:use temperaAdjustments :when "{{state.panels.properties == 'adjustments'}}"}]}
          {:use tGroup
            :grow 1
            :minHeight 200
            :with {:slot layers
              :tabs [(node #layers :label Layers) (node #channels :label Channels) (node #paths :label Paths)]}
            :children [{:use temperaLayers :when "{{state.panels.layers == 'layers'}}"}
              {:use temperaChannels :when "{{state.panels.layers == 'channels'}}"}
              (node :style dim
                #paths-empty
                :when "{{state.panels.layers == 'paths'}}"
                :anchors {:left 10 :top 10}
                :text "No paths: the pen tool isn't available yet.")]})
        (column #panel-icons
          :role toolbar
          :label "Collapsed panels"
          :width 36
          :height "100%"
          :color @panel
          :radius @kit.cardRadius
          :alignItems center
          :gap 2
          {:height 6 :width 1 :style clear}
          {:use tPopout :with {:name history :icon history :title History} :children [{:use temperaHistory}]}
          {:use tPopout
            :with {:name navigator :icon compass :title Navigator :height 250}
            :children [{:use temperaNavigator}]}))}
    :temperaNavigator {:node (column #navigator :fill true :gap 6 :alignItems center
        {:height 4 :width 1 :style clear}
        (image #navigator-picture
          :label Navigator
          :source "doc/navigator"
          :fit contain
          :width 234
          :height 170
          :color @canvasBack)
        (row #navigator-zoom :height 26 :gap 4 :alignItems center
          (label #navigator-percent :text "{{computed.zoomPercent}}%" :textColor @text :width 56 :height 22)
          (node :use tIcon #navigator-out :with {:icon zoom-out :run zoomOut :tip "Zoom Out"})
          (node :use tIcon #navigator-fit :with {:icon maximize :run fitOnScreen :tip "Fit on Screen"})
          (node :use tIcon #navigator-in :with {:icon zoom-in :run zoomIn :tip "Zoom In"})))}
    :temperaColour {:node (node #colour :style clear :fill true
        (node #colour-chips :role group :label "Colour being edited" :style clear :x 8 :y 8 :width 36 :height 36
          (node #colour-chip-background
            :role button
            :label "Edit the background colour"
            :tooltip "Background colour"
            :x 13
            :y 13
            :width 22
            :height 22
            :radius 999
            :color "{{state.doc.background}}"
            :cursor hand
            :borderWidth "{{state.colourTarget == 'background' ? 2 : 1}}"
            :borderColor "{{state.colourTarget == 'background' ? '@text' : '@muted'}}"
            :onClick {:set state.colourTarget :to background})
          {:x 0 :y 0 :width 24 :height 24 :radius 999 :color @panel}
          (node #colour-chip-foreground
            :role button
            :label "Edit the foreground colour"
            :tooltip "Foreground colour"
            :x 1
            :y 1
            :width 22
            :height 22
            :radius 999
            :color "{{state.doc.foreground}}"
            :cursor hand
            :borderWidth "{{state.colourTarget == 'foreground' ? 2 : 1}}"
            :borderColor "{{state.colourTarget == 'foreground' ? '@text' : '@muted'}}"
            :onClick {:set state.colourTarget :to foreground}))
        (colorPicker #colour-picker
          :label "{{state.colourTarget == 'background' ? 'Background' : 'Foreground'}} colour"
          :anchors {:left 52 :right 8 :top 6 :bottom 6}
          :value "{{state.colourTarget == 'background' ? state.doc.background : state.doc.foreground}}"
          :fields false
          :alpha false
          :textColor @text
          :accentColor @accent
          :onChange {:run setOptions
            :with {:args "{{parseJson('{' + json(state.colourTarget) + ': ' + json(event.value) + '}')}}"}}))}
    :temperaGradients {:node (column #gradients
        :role list
        :label "Gradient presets"
        :anchors {:left 0 :right 0 :top 4 :bottom 4}
        :scroll vertical
        {:type label
          :repeat {:for g :in state.gradients}
          :role listitem
          :label "{{g}}"
          :text "{{g}}"
          :icon blend
          :width "100%"
          :height 22
          :padding 4
          :textColor @text
          :cursor hand
          :hover {:color @hover}
          :onClick [{:run setOptions :with {:args {:gradientPreset "{{g}}"}}}
            {:run setTool :with {:args {:tool gradient}}}]})}
    :temperaSwatches {:node (grid #swatches
        :role group
        :label Palette
        :columns 12
        :gap 3
        :anchors {:left 8 :right 8 :top 8}
        :height 40
        (node #"swatch-{{index}}"
          :repeat {:for swatch :in state.swatches :index index}
          :role button
          :label "Swatch {{swatch}}"
          :tooltip "{{swatch}}"
          :width 17
          :height 17
          :color "{{swatch}}"
          :borderWidth 1
          :borderColor @border
          :cursor hand
          :hover {:borderColor @text}
          :onClick {:run setOptions :with {:args {:foreground "{{swatch}}"}}}))}
    :temperaProperties {:node (column #properties
        :role region
        :label Properties
        :anchors {:left 10 :right 10 :top 6 :bottom 6}
        :gap 4
        :scroll vertical
        (label #properties-kind
          :textColor @text
          :fitText true
          :icon "{{!computed.activeLayer ? 'circle-off' : computed.activeLayer.kind == 'group' ? 'folder' : computed.adjustment ? 'contrast' : 'image'}}"
          :text "{{!computed.activeLayer ? 'No layer' : computed.activeLayer.kind == 'group' ? 'Group' : computed.adjustment ? (find(state.adjustments, it.id == computed.adjustmentKind).title ?? '') : 'Pixel Layer'}}")
        (node :style dim
          #properties-name
          :when "{{computed.activeLayer && !computed.adjustment}}"
          :text "{{computed.activeLayer.name}}: {{computed.blendTitle}}, {{computed.activeLayer.opacity}}% opacity")
        (node #properties-line :width "100%" :height 1 :color @border)
        (node :use areaChart
          #adjust-tones
          :when "{{computed.adjustmentKind == 'levels'}}"
          :width 200
          :height 60
          :with {:color @muted
            :values "{{map(state.doc.histogram ?? [], min(it, 4 * sum(state.doc.histogram) / length(state.doc.histogram)))}}"})
        (node :use tAdjustSlider
          #adjust-input-black
          :when "{{computed.adjustmentKind == 'levels'}}"
          :with {:title "Input black" :key inputBlack :value "{{computed.adjustment.inputBlack}}" :max 255 :scale 255})
        (node :use tAdjustSlider
          #adjust-input-white
          :when "{{computed.adjustmentKind == 'levels'}}"
          :with {:title "Input white" :key inputWhite :value "{{computed.adjustment.inputWhite}}" :max 255 :scale 255})
        (node :use tAdjustSlider
          #adjust-gamma
          :when "{{computed.adjustmentKind == 'levels'}}"
          :with {:title Gamma :key gamma :value "{{computed.adjustment.gamma}}" :min 10 :max 400 :scale 100})
        (node :use tAdjustSlider
          #adjust-output-black
          :when "{{computed.adjustmentKind == 'levels'}}"
          :with {:title "Output black"
            :key outputBlack
            :value "{{computed.adjustment.outputBlack}}"
            :max 255
            :scale 255})
        (node :use tAdjustSlider
          #adjust-output-white
          :when "{{computed.adjustmentKind == 'levels'}}"
          :with {:title "Output white"
            :key outputWhite
            :value "{{computed.adjustment.outputWhite}}"
            :max 255
            :scale 255})
        (curves #adjust-curve
          :label Curve
          :when "{{computed.adjustmentKind == 'curves'}}"
          :width 200
          :height 140
          :value "{{merge({points: []}, {points: computed.adjustment.curve})}}"
          :accentColor @accent
          :histogram "{{map(state.doc.histogram ?? [], min(it, 4 * sum(state.doc.histogram) / length(state.doc.histogram)))}}"
          :onChange {:run setLayer
            :with {:args {:adjustment "{{merge(computed.adjustment, {curve: event.value.points})}}"}}})
        (node :use tAdjustSlider
          #adjust-hue
          :when "{{computed.adjustmentKind == 'hue-saturation'}}"
          :with {:title Hue :key hue :value "{{computed.adjustment.hue}}" :min -180 :max 180 :scale 1})
        (node :use tAdjustSlider
          #adjust-saturation
          :when "{{computed.adjustmentKind == 'hue-saturation'}}"
          :with {:title Saturation :key saturation :value "{{computed.adjustment.saturation}}" :min -100})
        (node :use tAdjustSlider
          #adjust-lightness
          :when "{{computed.adjustmentKind == 'hue-saturation'}}"
          :with {:title Lightness :key lightness :value "{{computed.adjustment.lightness}}" :min -100})
        (node :use tBalanceSlider
          #adjust-cyan-red
          :when "{{computed.adjustmentKind == 'color-balance'}}"
          :with {:title "Cyan / Red" :channel 0})
        (node :use tBalanceSlider
          #adjust-magenta-green
          :when "{{computed.adjustmentKind == 'color-balance'}}"
          :with {:title "Mag. / Green" :channel 1})
        (node :use tBalanceSlider
          #adjust-yellow-blue
          :when "{{computed.adjustmentKind == 'color-balance'}}"
          :with {:title "Yellow / Blue" :channel 2})
        (node :use tAdjustSlider
          #adjust-brightness
          :when "{{computed.adjustmentKind == 'brightness-contrast'}}"
          :with {:title Brightness :key brightness :value "{{computed.adjustment.brightness}}" :min -100})
        (node :use tAdjustSlider
          #adjust-contrast
          :when "{{computed.adjustmentKind == 'brightness-contrast'}}"
          :with {:title Contrast :key contrast :value "{{computed.adjustment.contrast}}" :min -100})
        (node :use tAdjustSlider
          #adjust-level
          :when "{{computed.adjustmentKind == 'threshold'}}"
          :with {:title Level :key level :value "{{computed.adjustment.level}}" :max 255 :scale 255})
        (node :style dim
          :fitText false
          #adjust-plain
          :width "100%"
          :height 34
          :wrap true
          :when "{{computed.adjustmentKind == 'invert' || computed.adjustmentKind == 'gradient-map'}}"
          :text "{{computed.adjustmentKind == 'invert' ? 'Inverts the colours below.' : 'Maps the lightness below to the foreground and background colours.'}}")
        (column #properties-sections :when "{{!computed.adjustment}}" :width "100%" :height 300 :gap 4
          (node :use tSection #section-transform :with {:title Transform :key transform})
          {:type row
            :when "{{state.props.transform}}"
            :width "100%"
            :height 24
            :gap 10
            :alignItems center
            :children [(node :use tField
                #transform-w
                :with {:title W
                  :key w
                  :value "{{computed.box[2]}}"
                  :unit " px"
                  :pop false
                  :width 66
                  :min 1
                  :max 100000
                  :run transformBox
                  :off "{{computed.activeLayer.kind != 'pixel'}}"
                  :tip "{{state.doc.selection ? 'The selection' : 'The image'}}: typing a width scales the active layer"})
              (node :use tField
                #transform-h
                :with {:title H
                  :key h
                  :value "{{computed.box[3]}}"
                  :unit " px"
                  :pop false
                  :width 66
                  :min 1
                  :max 100000
                  :run transformBox
                  :off "{{computed.activeLayer.kind != 'pixel'}}"
                  :tip "Typing a height scales the active layer"})]}
          {:type row
            :when "{{state.props.transform}}"
            :width "100%"
            :height 24
            :gap 10
            :alignItems center
            :children [(node :use tField
                #transform-x
                :with {:title X
                  :key x
                  :value "{{computed.box[0]}}"
                  :unit " px"
                  :pop false
                  :width 66
                  :min -100000
                  :max 100000
                  :run transformBox
                  :off "{{computed.activeLayer.kind != 'pixel'}}"
                  :tip "Typing a position moves the active layer across"})
              (node :use tField
                #transform-y
                :with {:title Y
                  :key y
                  :value "{{computed.box[1]}}"
                  :unit " px"
                  :pop false
                  :width 66
                  :min -100000
                  :max 100000
                  :run transformBox
                  :off "{{computed.activeLayer.kind != 'pixel'}}"
                  :tip "Typing a position moves the active layer down"})]}
          {:type row
            :when "{{state.props.transform}}"
            :width "100%"
            :height 24
            :gap 6
            :alignItems center
            :children [{:type label :icon rotate-cw :textColor @muted :width 14 :height 20 :align center :tooltip Angle}
              (node :use tField
                #transform-angle
                :with {:title ""
                  :key angle
                  :value 0
                  :unit "°"
                  :pop false
                  :width 50
                  :min -360
                  :max 360
                  :run transformActive
                  :off "{{computed.activeLayer.kind != 'pixel'}}"
                  :tip "Turns the active layer by the angle typed"})
              {:grow 1 :height 1 :style clear}
              (node :use tIcon
                #flip-horizontal
                :with {:icon flip-horizontal
                  :tip "Flip the layer horizontally"
                  :run flipLayerHorizontal
                  :off "{{computed.activeLayer.kind != 'pixel'}}"})
              (node :use tIcon
                #flip-vertical
                :with {:icon flip-vertical
                  :tip "Flip the layer vertically"
                  :run flipLayerVertical
                  :off "{{computed.activeLayer.kind != 'pixel'}}"})]}
          {:width "100%" :height 1 :color @border}
          (node :use tSection #section-align :with {:title "Align and Distribute" :key align})
          (row #align-icons :when "{{state.props.align}}" :width "100%" :height 24 :gap 2
            (node :use tIcon
              :repeat {:for a
                :in "{{[{icon: 'align-start-vertical', edge: 'left', title: 'left edges'}, {icon: 'align-center-vertical', edge: 'center', title: 'horizontal centres'}, {icon: 'align-end-vertical', edge: 'right', title: 'right edges'}, {icon: 'align-start-horizontal', edge: 'top', title: 'top edges'}, {icon: 'align-center-horizontal', edge: 'middle', title: 'vertical centres'}, {icon: 'align-end-horizontal', edge: 'bottom', title: 'bottom edges'}]}}"}
              #"align-{{a.edge}}"
              :with {:icon "{{a.icon}}"
                :tip "Align {{a.title}} with the {{state.doc.selection ? 'selection' : 'image'}}"
                :run alignActive
                :args {:edge "{{a.edge}}"}
                :off "{{computed.activeLayer.kind != 'pixel'}}"}))
          {:width "100%" :height 1 :color @border}
          (node :use tSection #section-actions :with {:title "Quick Actions" :key actions})
          (column #quick-actions :when "{{state.props.actions}}" :width "100%" :height 76 :gap 4
            (node :use tButton
              #action-duplicate
              :with {:title "Duplicate Layer" :tip "Duplicate the active layer (Cmd+J)" :run duplicateActive})
            (node :use tButton
              #action-mask
              :with {:title "Add Layer Mask" :tip "Add a mask to the active layer" :run addLayerMask})
            (node :use tButton
              #action-select-all
              :with {:title "Select All" :tip "Select the whole image (Cmd+A)" :run selectAll}))))}
    :temperaAdjustments {:node (column #adjustments
        :role region
        :label Adjustments
        :anchors {:left 10 :right 10 :top 8 :bottom 8}
        :gap 8
        {:style dim :text "Add an adjustment"}
        (row #adjustment-icons :width "100%" :height 24 :gap 3
          (node :use tIcon
            :repeat {:for a :in state.adjustments}
            #"adjustment-{{a.id}}"
            :with {:icon "{{a.icon}}" :tip "{{a.title}}" :run addLayer :args {:kind "{{a.id}}" :name "{{a.title}}"}})))}
    :temperaChannels {:node (column #channels :role list :label Channels :fill true
        {:type row
          :repeat {:for c :in "{{['RGB', 'Red', 'Green', 'Blue']}}" :index i}
          :role listitem
          :label "{{c}}"
          :width "100%"
          :height 34
          :alignItems center
          :gap 8
          :color "{{i == 0 ? '@selected' : 'transparent'}}"
          :children [{:type label :icon eye :textColor @text :width 26 :height 20 :align center}
            {:type image
              :source "doc/navigator"
              :fit contain
              :width 30
              :height 26
              :color @canvasBack
              :visible "{{i == 0}}"}
            {:type label :text "{{c}}" :textColor @text :grow 1 :height 20}
            {:style dim :text "Cmd+{{i + 2}}"}
            {:width 6 :height 1 :style clear}]}
        {:style dim :anchors {:left 10} :text "Editing single channels isn't available yet."})}
    :temperaLayers {:node (column #layers :role region :label Layers :fill true
        (row #layers-filter-row
          :role toolbar
          :label "Filter the layers"
          :height 30
          :width "100%"
          :gap 1
          :alignItems center
          {:width 6 :height 1 :style clear}
          (node :use tPicker
            #layer-filter-kind
            :with {:title Kind
              :tip "Filter type: name, effect and mode aren't available yet"
              :width 70
              :open state.layerKindMenu
              :items [{:title Kind :checked true}]})
          {:width 6 :height 1 :style clear}
          (node :use tIcon
            :repeat {:for k
              :in "{{[{id: 'pixel', icon: 'image', title: 'pixel layers'}, {id: 'adjustment', icon: 'contrast', title: 'adjustment layers'}, {id: 'group', icon: 'folder', title: 'groups'}]}}"}
            #"filter-kind-{{k.id}}"
            :with {:icon "{{k.icon}}"
              :tip "Show only the {{k.title}}"
              :on "{{state.layerFilter.on && contains(state.layerFilter.kinds, k.id)}}"
              :run toggleLayerKind
              :args {:kind "{{k.id}}"}})
          (node :use tIcon #filter-kind-type :with {:icon type :tip "Type layers aren't available yet" :off true})
          (node :use tIcon #filter-kind-shape :with {:icon shapes :tip "Shape layers aren't available yet" :off true})
          {:grow 1 :height 1 :style clear}
          (node :use tIcon
            #layer-filter-toggle
            :with {:icon "{{state.layerFilter.on ? 'toggle-right' : 'toggle-left'}}"
              :tip "Turn layer filtering {{state.layerFilter.on ? 'off' : 'on'}}"
              :run toggleLayerFilter})
          {:width 4 :height 1 :style clear})
        (row #layers-blend-row :height 30 :width "100%" :gap 6 :alignItems center
          {:width 2 :height 1 :style clear}
          (node :use tPicker
            #layer-blend
            :with {:title "{{computed.blendTitle}}"
              :tip "Blend mode"
              :width 104
              :open state.blendMenu
              :items "{{map(state.blends, it == '-' ? {separator: true} : {title: join(map(split(it, '-'), upper(slice(it, 0, 1)) + slice(it, 1)), ' '), checked: it == computed.activeLayer.blend, run: 'setLayer', with: {args: {blend: it}}})}}"})
          {:grow 1 :height 1 :style clear}
          (node :use tField
            #layer-opacity
            :with {:title Opacity :key opacity :value "{{computed.activeLayer.opacity ?? 100}}" :run setLayer :width 40}))
        (row #layers-lock-row :height 26 :width "100%" :gap 2 :alignItems center
          {:width 6 :height 1 :style clear}
          {:style dim :text "Lock:"}
          (node :use tIcon
            #lock-transparency
            :with {:icon grid-2-x-2 :tip "Lock transparent pixels, not available yet" :off true})
          (node :use tIcon
            #lock-pixels
            :with {:icon brush
              :tip "Lock image pixels"
              :on "{{computed.activeLayer.lockPixels}}"
              :run setLayer
              :args {:lockPixels "{{!computed.activeLayer.lockPixels}}"}})
          (node :use tIcon
            #lock-position
            :with {:icon move
              :tip "Lock position"
              :on "{{computed.activeLayer.lockPosition}}"
              :run setLayer
              :args {:lockPosition "{{!computed.activeLayer.lockPosition}}"}})
          (node :use tIcon
            #lock-artboard
            :with {:icon frame :tip "Prevent auto-nesting in artboards, not available yet" :off true})
          (node :use tIcon
            #lock-all
            :with {:icon lock
              :tip "Lock all"
              :on "{{computed.activeLayer.lockAll}}"
              :run setLayer
              :args {:lockAll "{{!computed.activeLayer.lockAll}}"}})
          {:grow 1 :height 1 :style clear}
          (node :use tField
            #layer-fill
            :with {:title Fill
              :key fill
              :value 100
              :run setLayer
              :width 40
              :off true
              :tip "Fill opacity isn't available yet"}))
        {:width "100%" :height 1 :color @border}
        (column #layer-rows :role list :label Layers :width "100%" :grow 1 :scroll vertical
          (row #"layer-{{layer.id}}"
            :repeat {:for layer :in computed.layerRows :key "{{layer.id}}"}
            :role listitem
            :label "{{layer.name}}"
            :selected "{{layer.id == state.doc.active}}"
            :anchors {:left 4 :right 4}
            :height 36
            :alignItems center
            :gap 5
            :cursor hand
            :radius @kit.radius
            :color "{{layer.id == state.doc.active ? '@selected' : 'transparent'}}"
            :hover {:color @hover}
            :drag {:data (node #"{{layer.id}}")}
            :onDrop {:run moveLayer :with {:args (node #"{{event.data.id}}" :onto "{{layer.id}}")}}
            :onClick {:run selectLayer :with {:args (node #"{{layer.id}}")}}
            {:width 3 :height 22 :radius 2 :color "{{layer.id == state.doc.active ? '@accent' : 'transparent'}}"}
            (label #"layer-eye-{{layer.id}}"
              :role button
              :label "Visibility of {{layer.name}}"
              :icon "{{layer.visible ? 'eye' : ''}}"
              :width 28
              :height 36
              :align center
              :cursor hand
              :textColor @text
              :onClick {:run setLayer :with {:args (node #"{{layer.id}}" :visible "{{!layer.visible}}")}})
            {:style clear :width "{{layer.depth * 14}}" :height 1}
            (label #"layer-fold-{{layer.id}}"
              :when "{{layer.kind == 'group'}}"
              :role button
              :focusable true
              :label "{{contains(state.foldedGroups, layer.id) ? 'Open' : 'Close'}} {{layer.name}}"
              :isOpen "{{!contains(state.foldedGroups, layer.id)}}"
              :icon "{{contains(state.foldedGroups, layer.id) ? 'chevron-right' : 'chevron-down'}}"
              :textColor @muted
              :width 12
              :height 36
              :cursor hand
              :onClick {:run foldGroup :with {:args (node #"{{layer.id}}")}})
            (label #"layer-clip-{{layer.id}}"
              :when "{{layer.clipped}}"
              :icon corner-left-down
              :textColor @muted
              :width 14
              :tooltip "Clipped to the layer below")
            (image #"layer-thumb-{{layer.id}}"
              :when "{{!empty(layer.thumbnail)}}"
              :source "{{layer.thumbnail}}"
              :fit contain
              :width 30
              :height 28
              :color @canvasBack
              :borderWidth 1
              :borderColor @border)
            (label #"layer-icon-{{layer.id}}"
              :when "{{empty(layer.thumbnail)}}"
              :icon "{{layer.kind == 'group' ? 'folder' : 'contrast'}}"
              :font iconsLarge
              :width 30
              :height 28
              :align center
              :textColor @text)
            (label #"layer-mask-{{layer.id}}"
              :when "{{layer.mask}}"
              :icon link
              :font small
              :textColor @muted
              :width 10
              :tooltip "Layer mask, linked")
            {:when "{{layer.mask}}"
              :width 30
              :height 28
              :color "#ffffff"
              :borderWidth 1
              :borderColor @border
              :tooltip "Layer mask"}
            (label #"layer-name-{{layer.id}}"
              :text "{{layer.name}}"
              :textColor @text
              :grow 1
              :height 36
              :align {:horizontal start :vertical center})
            (label #"layer-lock-{{layer.id}}"
              :when "{{layer.lockAll || layer.lockPixels || layer.lockPosition}}"
              :icon lock
              :textColor @muted
              :width 16
              :tooltip Locked)
            {:width 6 :height 1 :style clear}))
        (row #layer-buttons
          :role toolbar
          :label "Layer actions"
          :width "100%"
          :height 28
          :alignItems center
          :justify end
          :gap 2
          (node :use tIcon #layer-link :with {:icon link :tip "Link layers, not available yet" :off true})
          (node :use tIcon #layer-fx :with {:icon sparkles :tip "Layer style, not available yet" :off true})
          (node :use tIcon #layer-add-mask :with {:icon circle-dot :tip "Add layer mask" :run addLayerMask})
          (node :use tIcon
            #layer-adjust
            :with {:icon contrast :tip "New fill or adjustment layer" :on "{{state.adjustMenu}}"}
            :onClick [{:toggle state.adjustMenu}]
            (node :use menu
              #adjust-menu
              :with {:open state.adjustMenu
                :placement above
                :width 200
                :icons true
                :items "{{map(state.adjustments, {title: it.title + '...', icon: it.icon, run: 'addLayer', with: {args: {kind: it.id, name: it.title}}})}}"}))
          (node :use tIcon #layer-new-group :with {:icon folder :tip "Create a new group" :run newGroup})
          (node :use tIcon #layer-new :with {:icon plus-square :tip "Create a new layer" :run newLayer})
          (node :use tIcon #layer-delete :with {:icon trash-2 :tip "Delete layer" :run deleteActive})
          {:width 6 :height 1 :style clear}))}
    :temperaHistory {:node (column #history :fill true
        (list #history-list
          :label History
          :width "100%"
          :grow 1
          :color @panel
          :rowHeight 24
          :items "{{state.doc.history}}"
          :value "{{text(state.doc.position)}}"
          :textColor @text
          :selectedTextColor @text
          :selectedBackground @selected
          :accentColor @accent
          :onChange {:run goToHistory :with {:args {:position "{{number(event.value)}}"}}})
        (node :style dim
          :fitText false
          #history-dropped
          :when "{{state.doc.dropped > 0}}"
          :font small
          :text "{{state.doc.dropped}} older steps dropped to stay within the undo memory"
          :width "100%"
          :height 18
          :padding 4))}
    :temperaFilterDialog {:node {:style clear
        :width 0
        :height 0
        :children [(node :use dialog
            #filter-dialog
            :with {:open state.filter.open
              :title "{{computed.filterTitle}}"
              :message ""
              :width 380
              :height 250
              :buttons [{:title Cancel :run discardFilter} {:title OK :run commitFilter :default true}]}
            (column #filter-fields :anchors {:left 20 :right 20 :top 44 :bottom 52} :gap 2
              {:use tFilterSlider
                :when "{{state.filter.kind != 'add-noise' && state.filter.kind != 'motion-blur'}}"
                :with {:title Radius :key radius :min 1 :max 100}}
              {:use tFilterSlider
                :when "{{state.filter.kind != 'gaussian-blur' && state.filter.kind != 'motion-blur'}}"
                :with {:title Amount :key amount :min 1 :max 500}}
              {:use tFilterSlider
                :when "{{state.filter.kind == 'unsharp-mask'}}"
                :with {:title Threshold :key threshold :max 255}}
              {:use tFilterSlider
                :when "{{state.filter.kind == 'motion-blur'}}"
                :with {:title Angle :key angle :min -180 :max 180}}
              {:use tFilterSlider
                :when "{{state.filter.kind == 'motion-blur'}}"
                :with {:title Distance :key distance :min 1 :max 200}}
              (row #filter-live-row :width "100%" :height 28 :gap 8 :alignItems center
                (checkbox #filter-live
                  :label Preview
                  :width 14
                  :height 14
                  :bind state.filter.live
                  :activeColor @accent
                  :trackColor @border
                  :knobColor @input)
                {:type label :text Preview :textColor @text :fitText true})))]}}
    :temperaNewDialog {:node {:style clear
        :width 0
        :height 0
        :children [(node :use dialog
            #new-dialog
            :with {:open state.newDoc.open
              :title "New Document"
              :message ""
              :width 360
              :height 264
              :buttons [{:title Cancel} {:title Create :run createDocument :default true}]}
            (column #new-fields :anchors {:left 20 :right 20 :top 44 :bottom 52} :gap 6
              (row #new-preset-row :width "100%" :height 28 :gap 8 :alignItems center
                {:style dim :fitText false :text Preset :width 70 :height 20}
                (node :use tPicker
                  #new-preset
                  :with {:title "{{find(state.newPresets, it.width == number(state.newDoc.width) && it.height == number(state.newDoc.height)).title ?? 'Custom'}}"
                    :tip "Document size presets"
                    :width 200
                    :open state.newPresetMenu
                    :inDialog true
                    :items "{{map(state.newPresets, {title: it.title, checked: it.width == number(state.newDoc.width) && it.height == number(state.newDoc.height), run: 'newPreset', with: {args: {width: it.width, height: it.height}}})}}"}))
              {:type row
                :repeat {:for f :in "{{['width', 'height']}}"}
                :width "100%"
                :height 28
                :gap 8
                :alignItems center
                :children [{:style dim
                    :fitText false
                    :text "{{upper(slice(f, 0, 1)) + slice(f, 1)}}"
                    :width 70
                    :height 20}
                  (textarea #"new-{{f}}"
                    :label "{{f}}"
                    :width 100
                    :height 24
                    :padding 4
                    :color @input
                    :textColor @text
                    :borderWidth 1
                    :borderColor @border
                    :radius @kit.radius
                    :bind "{{'state.newDoc.' + f}}")
                  {:style dim :text Pixels}]}
              {:type row
                :width "100%"
                :height 28
                :gap 8
                :alignItems center
                :children [{:style dim :fitText false :text "Bit depth" :width 70 :height 20}
                  (tabs #new-format
                    :width 200
                    :height 24
                    :bind state.newDoc.format
                    :tabs [(node #rgba8 :label "8 bit" :closable false)
                      (node #rgba16f :label "16 bit float" :closable false)]
                    :textColor @muted
                    :activeTextColor @text
                    :activeBackground @selected
                    :accentColor @accent)]}))]}}}}
