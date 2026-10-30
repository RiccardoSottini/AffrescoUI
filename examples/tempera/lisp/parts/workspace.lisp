{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Tempera's workspace: the menu bar, the options bar of the current tool, the tools bar (one column of tool groups, a right click opening a group's other tools) with the colours and the quick mask, the document tab, the canvas with its rulers and guides, and the status bar under it. The canvas is the host's temperaCanvas surface: the pointer on it paints, selects and pans in the host, which sends temperaChanged."
  :computed {:canvasWidth "{{(nodes.canvas.width ?? 0) > 0 ? nodes.canvas.width : window.width - 6 - 40 - 6 - 6 - 304 - 6 - (state.view.rulers ? 16 : 0)}}"
    :canvasHeight "{{(nodes.canvas.height ?? 0) > 0 ? nodes.canvas.height : window.height - 24 - 6 - 36 - 6 - 6 - 26 - 24 - (state.view.rulers ? 16 : 0)}}"
    :rulerStep "{{find([1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000], it * computed.px >= 64) ?? 10000}}"
    :rulerTicksX "{{map(range(min(200, ceil(computed.canvasWidth / (computed.rulerStep * computed.px)) + 2)), floor((state.doc.view.x - computed.canvasWidth / 2 / computed.px) / computed.rulerStep) * computed.rulerStep + it * computed.rulerStep)}}"
    :rulerTicksY "{{map(range(min(200, ceil(computed.canvasHeight / (computed.rulerStep * computed.px)) + 2)), floor((state.doc.view.y - computed.canvasHeight / 2 / computed.px) / computed.rulerStep) * computed.rulerStep + it * computed.rulerStep)}}"
    :rulerMinorX "{{map(range(min(400, 5 * length(computed.rulerTicksX))), computed.rulerTicksX[0] + it * computed.rulerStep / 5)}}"
    :rulerMinorY "{{map(range(min(400, 5 * length(computed.rulerTicksY))), computed.rulerTicksY[0] + it * computed.rulerStep / 5)}}"
    :px "{{state.doc.view.zoom / (window.scale ?? 1)}}"
    :paints "{{contains(['brush', 'pencil', 'eraser', 'clone'], state.doc.tool)}}"
    :selects "{{contains(['marquee', 'ellipse', 'lasso', 'polygon', 'wand'], state.doc.tool)}}"
    :tool "{{find(find(state.tools, some(it, it.id == state.doc.tool)) ?? [], it.id == state.doc.tool) ?? {}}}"
    :toolName "{{split(computed.tool.title ?? '', ' Tool')[0]}}"
    :mode "{{(contains(state.doc.profile, 'Grey') ? 'Gray' : 'RGB') + (state.doc.format == 'rgba16f' ? '/16' : '/8')}}"}
  :styles {:bar {:color @panel :alignItems center :width "100%"}
    :card {:color @panel :radius @kit.cardRadius}
    :dim {:type label :textColor @muted :fitText true}
    :sep {:width 1 :height 20 :color @border}
    :clear {:color transparent}}
  :components {:tIcon {:params {:icon "" :tip "" :run "" :args {} :on false :off false}
      :node {:type label
        :role button
        :label "{{tip}}"
        :tooltip "{{tip}}"
        :icon "{{icon}}"
        :width 24
        :height 22
        :align center
        :radius @kit.radius
        :cursor hand
        :selected "{{on}}"
        :disabled "{{off}}"
        :textColor "{{off ? '@muted' : '@text'}}"
        :color "{{on ? '@selected' : 'transparent'}}"
        :hover {:color "{{off ? 'transparent' : '@hover'}}"}
        :onClick {:run "{{run}}" :with {:args "{{args}}"}}}}
    :tRange {:params {:title "" :key "" :value 0 :min 0 :max 100 :scale 1 :run setOptions :args {} :width 120}
      :node {:let {:shown "{{round(value * scale)}}" :span "{{max - min}}"}
        :role slider
        :label "{{title}}"
        :tooltip "{{title}}: {{shown}}"
        :focusable true
        :width "{{width}}"
        :height 14
        :color transparent
        :cursor hand
        :onPress [{:set state.slide :to "{{clamp(round(min + event.left / max(1, event.width) * span), min, max)}}"}
          {:run "{{run}}"
            :with {:args "{{merge(args, parseJson('{' + json(key) + ': ' + json(state.slide / scale) + '}'))}}"}}]
        :onDrag {:run "{{run}}"
          :with {:args "{{merge(args, parseJson('{' + json(key) + ': ' + json(clamp(round(state.slide + event.dx / max(1, event.width) * span), min, max) / scale) + '}'))}}"}}
        :keys {:left [{:run "{{run}}"
              :with {:args "{{merge(args, parseJson('{' + json(key) + ': ' + json(max(min, shown - 1) / scale) + '}'))}}"}}]
          :right [{:run "{{run}}"
              :with {:args "{{merge(args, parseJson('{' + json(key) + ': ' + json(min(max, shown + 1) / scale) + '}'))}}"}}]}
        :focused {:borderWidth 1 :borderColor @accent}
        :children [{:anchors {:left 4 :right 4 :top 4} :height 2 :color @border}
          {:x 4 :y 4 :height 2 :width "{{(width - 8) * clamp((shown - min) / max(1, span), 0, 1)}}" :color @muted}
          {:type path
            :d "M 0 6 L 4 0 L 8 6 Z"
            :fillColor @text
            :width 8
            :height 6
            :y 7
            :x "{{(width - 8) * clamp((shown - min) / max(1, span), 0, 1)}}"}]}}
    :tField {:params {:title ""
        :key ""
        :value 0
        :min 0
        :max 100
        :scale 1
        :unit "%"
        :run setOptions
        :args {}
        :width 46
        :pop true
        :off false
        :tip ""}
      :node {:let {:shown "{{round(value * scale)}}" :where "{{'state.pop.' + key + run}}"}
        :type row
        :width "{{(empty(title) ? 0 : length(title) * 6 + 10) + width + (pop ? 14 : 0)}}"
        :height 22
        :alignItems center
        :gap 4
        :tooltip "{{tip}}"
        :children [{:style dim :when "{{!empty(title)}}" :text "{{title}}:"}
          {:type textarea
            :label "{{title}}"
            :text "{{shown}}{{unit}}"
            :width "{{width}}"
            :height 20
            :padding 3
            :textColor "{{off ? '@muted' : '@text'}}"
            :color @input
            :borderWidth 1
            :borderColor @border
            :radius @kit.radius
            :disabled "{{off}}"
            :onSubmit {:run "{{run}}"
              :with {:args "{{merge(args, parseJson('{' + json(key) + ': ' + json(clamp(number(trim(split(split(event.text, '%')[0], ' ')[0])) ?? shown, min, max) / scale) + '}'))}}"}}}
          {:type label
            :when "{{pop}}"
            :role button
            :label "{{title}} slider"
            :icon chevron-down
            :font small
            :width 10
            :height 20
            :align center
            :textColor @muted
            :cursor hand
            :disabled "{{off}}"
            :hover {:textColor @text}
            :onClick {:toggle "{{where}}"}
            :children [{:overlay {:placement below-end :open "{{where}}" :dismiss outside}
                :width 150
                :height 26
                :color @panel
                :radius @kit.cardRadius
                :borderWidth 1
                :borderColor @border
                :shadow {:y 3 :blur 10 :color "#00000060"}
                :children [{:use tRange
                    :x 8
                    :y 6
                    :with {:title "{{title}}"
                      :key "{{key}}"
                      :value "{{value}}"
                      :min "{{min}}"
                      :max "{{max}}"
                      :scale "{{scale}}"
                      :run "{{run}}"
                      :args "{{args}}"
                      :width 134}}]}]}]}}
    :tButton {:params {:title "" :tip "" :run "" :args {} :off false}
      :node {:type label
        :role button
        :label "{{title}}"
        :tooltip "{{tip}}"
        :text "{{title}}"
        :width "{{length(title) * 6 + 22}}"
        :height 22
        :align center
        :radius 999
        :color @selected
        :textColor "{{off ? '@muted' : '@text'}}"
        :disabled "{{off}}"
        :cursor hand
        :borderWidth 1
        :borderColor @border
        :hover {:color "{{off ? '@selected' : '@hover'}}"}
        :onClick {:run "{{run}}" :with {:args "{{args}}"}}}}
    :tCheck {:params {:title "" :key "" :checked false :off false}
      :node {:type row
        :width "{{length(title) * 6 + 26}}"
        :height 24
        :alignItems center
        :gap 5
        :children [{:type checkbox
            :label "{{title}}"
            :width 13
            :height 13
            :checked "{{checked}}"
            :disabled "{{off}}"
            :activeColor "{{off ? '@muted' : '@accent'}}"
            :trackColor @border
            :knobColor @input
            :onChange {:run setOptions :with {:args "{{parseJson('{' + json(key) + ': ' + json(event.value) + '}')}}"}}}
          {:type label :text "{{title}}" :textColor "{{off ? '@muted' : '@text'}}" :fitText true}]}}
    :tChoice {:params {:tabs [] :value "" :key "" :tip ""}
      :node {:type row
        :role group
        :label "{{tip}}"
        :width "{{length(tabs) * 25}}"
        :height 22
        :gap 1
        :children [{:use tIcon
            :repeat {:for c :in "{{tabs}}"}
            :with {:icon "{{c.icon}}"
              :tip "{{c.title}}"
              :on "{{c.id == value}}"
              :run setOptions
              :args "{{parseJson('{' + json(key) + ': ' + json(c.id) + '}')}}"}}]}}
    :tPicker {:params {:title "" :tip "" :open "" :items [] :width 64 :inDialog false}
      :node {:type label
        :role button
        :label "{{tip}}"
        :tooltip "{{tip}}"
        :text "{{title}}"
        :width "{{width}}"
        :height 22
        :padding 4
        :radius @kit.radius
        :color @input
        :textColor @text
        :borderWidth 1
        :borderColor @border
        :cursor hand
        :hover {:borderColor @muted}
        :onClick {:toggle "{{open}}"}
        :children [{:type label
            :icon chevron-down
            :font small
            :textColor @muted
            :anchors {:right 3 :top 5}
            :fitText true}
          {:use tMenu :with {:open "{{open}}" :width 200 :items "{{items}}" :close "{{inDialog ? open : ''}}"}}]}}
    :tMenuRow {:params {:item {} :path "" :mark check :indent 26 :close ""}
      :node {:anchors {:left 4 :right 4 :top 0 :bottom 0}
        :radius @kit.radius
        :role "{{item.separator ? 'separator' : 'menuitem'}}"
        :label "{{item.title ?? ''}}"
        :focusable "{{!item.separator}}"
        :disabled "{{item.separator || item.enabled == false}}"
        :mnemonic "{{item.title ?? ''}}"
        :color transparent
        :tooltip "{{item.run == 'unavailable' ? 'Not available yet' : ''}}"
        :selected "{{item.checked == null ? null : item.checked == true}}"
        :hover {:color "{{item.separator || item.enabled == false ? 'transparent' : '@menuHover'}}"}
        :focused {:color @menuHover}
        :onClick [{:if "{{item.submenu != null}}"
            :then [{:toggle "{{path}}"}]
            :else [{:if "{{empty(close)}}" :then [{:dismiss true}] :else [{:set "{{close}}" :to false}]}
              {:if "{{item.run != null}}" :then [{:run "{{item.run}}" :with "{{item.with}}"}]}]}]
        :children [{:when "{{item.separator}}" :anchors {:left 0 :right 0 :top 3} :height 1 :color @border}
          {:when "{{item.checked == true && mark == 'square'}}" :x 8 :y 8 :width 5 :height 5 :color @text}
          {:type label
            :when "{{!item.separator && (item.checked == true && mark == 'check' || indent > 26 && item.icon != null)}}"
            :icon "{{item.checked == true && mark == 'check' ? 'check' : item.icon}}"
            :textColor "{{item.enabled == false ? '@muted' : '@text'}}"
            :anchors {:left "{{mark == 'square' ? 20 : 7}}" :top 4}
            :fitText true}
          {:type label
            :when "{{!item.separator}}"
            :text "{{item.title ?? ''}}"
            :textColor "{{item.enabled == false ? '@muted' : '@text'}}"
            :anchors {:left "{{indent}}" :top 4}
            :fitText true}
          {:type label
            :when "{{!item.separator}}"
            :icon "{{item.submenu != null ? 'chevron-right' : ''}}"
            :text "{{item.submenu != null ? '' : join(split(join(split(item.keyLabel ?? '', 'Down'), '↓'), 'Up'), '↑')}}"
            :textColor @muted
            :anchors {:right 10 :top 4}
            :fitText true}]}}
    :tSubmenu {:params {:items [] :open "" :width 220}
      :node {:let {:rows "{{map(items, merge({separator: false, checked: null, enabled: true, icon: null, keyLabel: null, submenu: null, run: null, with: null, title: ''}, it))}}"}
        :type column
        :overlay {:placement right :open "{{open}}" :dismiss outside :modal true}
        :role menu
        :arrows vertical
        :width "{{width}}"
        :height "{{sum(rows, it.separator ? 7 : 22) + 10}}"
        :color @panel
        :radius @kit.cardRadius
        :borderWidth 1
        :borderColor @border
        :shadow {:y 4 :blur 14 :color "#00000060"}
        :children [{:height 5 :width 1 :style clear}
          {:use tMenuRow
            :repeat {:for item :in "{{rows}}"}
            :width "100%"
            :height "{{item.separator ? 7 : 22}}"
            :with {:item "{{item}}"}}]}}
    :tMenu {:params {:items [] :open state.menuOpen :placement below :width 240 :mark check :icons false :close ""}
      :node {:let {:rows "{{map(items, merge({separator: false, checked: null, enabled: true, icon: null, keyLabel: null, submenu: null, run: null, with: null, title: ''}, it))}}"}
        :type column
        :overlay {:placement "{{placement}}" :open "{{open}}" :dismiss outside :modal true}
        :role menu
        :arrows vertical
        :width "{{width}}"
        :height "{{sum(rows, it.separator ? 7 : 22) + 10}}"
        :color @panel
        :radius @kit.cardRadius
        :borderWidth 1
        :borderColor @border
        :shadow {:y 4 :blur 14 :color "#00000060"}
        :children [{:height 5 :width 1 :style clear}
          {:repeat {:for item :in "{{rows}}" :index i}
            :style clear
            :width "100%"
            :height "{{item.separator ? 7 : 22}}"
            :children [{:use tMenuRow
                :with {:item "{{item}}"
                  :path "{{open + 'Sub.m' + i}}"
                  :mark "{{mark}}"
                  :indent "{{icons || mark == 'square' ? 40 : 26}}"
                  :close "{{close}}"}}
              {:use tSubmenu
                :when "{{item.submenu != null}}"
                :with {:items "{{item.submenu}}" :open "{{open + 'Sub.m' + i}}" :width "{{width}}"}}]}]}}
    :temperaMenuBar {:node (row #menu-bar :style bar :color @background :height 24 :gap 2 :arrows horizontal
        (label #app-mark :icon palette :width 32 :height 24 :align center :textColor @accent :tooltip Tempera)
        (node :repeat {:for m
            :in "{{[{key: 'file', title: 'File'}, {key: 'edit', title: 'Edit'}, {key: 'image', title: 'Image'}, {key: 'layer', title: 'Layer'}, {key: 'type', title: 'Text'}, {key: 'select', title: 'Select'}, {key: 'filter', title: 'Filter'}, {key: 'view', title: 'View'}, {key: 'window', title: 'Window'}, {key: 'help', title: 'Help'}]}}"}
          :let {:key "{{m.key}}"}
          #"menu-{{key}}"
          :role button
          :label "{{m.title}}"
          :mnemonic "{{m.title}}"
          :focusable true
          :width "{{length(m.title) * 6.5 + 18}}"
          :height 20
          :radius 999
          :color "{{state.menuBar[key] ? '@panel' : 'transparent'}}"
          :hover {:color @panel}
          :focused {:color @panel}
          :onClick {:toggle "{{'state.menuBar.' + key}}"}
          {:type label :text "{{m.title}}" :textColor @text :anchors {:left 9 :top 3} :fitText true}
          {:use tMenu :with {:items "{{menus[key] ?? []}}" :open "{{'state.menuBar.' + key}}" :width 260}}))}
    :temperaOptionsBar {:node (row #options-bar
        :role toolbar
        :label Options
        :style card
        :anchors {:left 6 :right 6}
        :height 36
        :gap 8
        :alignItems center
        {:width 4 :height 1 :style clear}
        (row #options-tool
          :label "Current tool"
          :tooltip "{{computed.tool.title}}"
          :height 26
          :gap 4
          :alignItems center
          :width "{{length(computed.toolName) * 6.5 + 46}}"
          :radius 999
          :color @accentSoft
          {:type label :icon "{{computed.tool.icon}}" :textColor @accent :width 26 :height 26 :align center}
          (label #options-tool-name :text "{{computed.toolName}}" :textColor @text :fitText true))
        (row #options-paint :when "{{computed.paints}}" :alignItems center :gap 8 :height 30 :grow 100
          (row #brush-picker
            :role button
            :label "Brush preset picker"
            :tooltip "Brush preset picker"
            :width 58
            :height 26
            :alignItems center
            :gap 3
            :radius @kit.radius
            :cursor hand
            :color "{{state.brushMenu ? '@selected' : 'transparent'}}"
            :hover {:color @hover}
            :onClick {:toggle state.brushMenu}
            {:style clear
              :width 24
              :height 24
              :children [{:let {:d "{{clamp(4 + state.doc.options.size / 6, 4, 20)}}"}
                  :x "{{12 - d / 2}}"
                  :y "{{12 - d / 2}}"
                  :width "{{d}}"
                  :height "{{d}}"
                  :radius 999
                  :color @text
                  :borderWidth "{{state.doc.options.hardness < 60 ? 2 : 0}}"
                  :borderColor @muted}]}
            {:type label :font small :text "{{state.doc.options.size}}" :textColor @text :fitText true}
            {:type label :icon chevron-down :font small :textColor @muted :fitText true}
            (column #brush-popover
              :overlay {:placement below :open state.brushMenu :dismiss outside}
              :width 250
              :height 236
              :color @panel
              :radius @kit.cardRadius
              :borderWidth 1
              :borderColor @border
              :shadow {:y 3 :blur 10 :color "#00000060"}
              :gap 4
              (row :repeat {:for o
                  :in "{{[{key: 'size', title: 'Size', max: 500, unit: ' px'}, {key: 'hardness', title: 'Hardness', max: 100, unit: '%'}, {key: 'spacing', title: 'Spacing', max: 200, unit: '%'}]}}"}
                #"option-{{o.key}}"
                :width "100%"
                :height 26
                :alignItems center
                :gap 8
                {:width 2 :height 1 :style clear}
                {:style dim :fitText false :text "{{o.title}}:" :width 62 :height 16}
                {:use tRange
                  :with {:title "{{o.title}}"
                    :key "{{o.key}}"
                    :value "{{state.doc.options[o.key]}}"
                    :min 1
                    :max "{{o.max}}"
                    :width 112}}
                {:type label :text "{{state.doc.options[o.key]}}{{o.unit}}" :textColor @text :fitText true})
              {:width "100%" :height 1 :color @border}
              {:type column
                :role list
                :label "Brush presets"
                :width "100%"
                :grow 1
                :children [{:repeat {:for b :in state.brushes}
                    :type label
                    :role listitem
                    :label "{{b}}"
                    :text "{{b}}"
                    :width "100%"
                    :height 21
                    :padding 4
                    :textColor @text
                    :cursor hand
                    :hover {:color @menuHover}
                    :onClick [{:run setOptions :with {:args {:preset "{{b}}"}}} {:set state.brushMenu :to false}]}]}))
          (node :use tIcon
            #brush-settings
            :with {:icon sliders-horizontal :tip "Brush settings, not available yet" :off true})
          {:style sep}
          (node :use tPicker
            #paint-mode
            :with {:title Normal
              :tip "Painting mode: other modes aren't available yet"
              :width 84
              :open state.paintModeMenu
              :items [{:title Normal :checked true}]})
          (node :use tField #option-opacity :with {:title Opacity :key opacity :value "{{state.doc.options.opacity}}"})
          (node :use tIcon
            #opacity-pressure
            :with {:icon pencil-line :tip "Pressure for opacity, not available yet" :off true})
          (node :use tField #option-flow :with {:title Flow :key flow :value "{{state.doc.options.flow}}"})
          (node :use tIcon #airbrush :with {:icon spray-can :tip "Airbrush, not available yet" :off true})
          (node :use tField
            #option-smoothing
            :with {:title Smoothing
              :key smoothing
              :value 0
              :off true
              :width 40
              :tip "Stroke smoothing isn't available yet"})
          {:type label :icon rotate-cw :textColor @muted :width 14 :height 20 :align center :tooltip "Brush angle"}
          (node :use tField
            #option-angle
            :with {:title ""
              :key angle
              :value 0
              :unit "°"
              :pop false
              :off true
              :width 36
              :tip "The brush angle isn't available yet"})
          (node :use tIcon
            #size-pressure
            :with {:icon pencil-line :tip "Pressure for size, not available yet" :off true}))
        (row #options-select :when "{{computed.selects}}" :alignItems center :gap 8 :height 30 :grow 100
          (node :use tChoice
            #option-mode
            :with {:key mode
              :value "{{state.doc.options.mode}}"
              :tip "Selection mode"
              :tabs [(node #replace :icon square :title "New selection")
                (node #add :icon copy-plus :title "Add to selection (Shift)")
                (node #subtract :icon copy-minus :title "Subtract from selection (Alt)")
                (node #intersect :icon copy-x :title "Intersect with selection")]})
          {:style sep}
          (node :use tField
            #option-wand-tolerance
            :when "{{state.doc.tool == 'wand'}}"
            :with {:title Tolerance
              :key tolerance
              :value "{{state.doc.options.tolerance}}"
              :max 255
              :unit ""
              :pop false
              :width 36})
          (node :use tCheck #option-anti-alias :with {:title Anti-alias :checked true :off true})
          (node :use tCheck
            #option-wand-contiguous
            :when "{{state.doc.tool == 'wand'}}"
            :with {:title Contiguous :key contiguous :checked "{{state.doc.options.contiguous}}"})
          (node :use tCheck
            #option-wand-all
            :when "{{state.doc.tool == 'wand'}}"
            :with {:title "Sample All Layers" :key allLayers :checked "{{state.doc.options.allLayers}}"})
          (node :use tButton
            #option-feather
            :with {:title "Feather 8 px"
              :tip "Feather the selection by 8 pixels (Shift+F6)"
              :run featherSelection
              :off "{{!state.doc.selection}}"})
          (node :use tButton
            #option-quick-mask
            :with {:title "Quick Mask" :tip "Edit the selection as a mask (Q)" :run toggleQuickMask}))
        (row #options-fill :when "{{state.doc.tool == 'fill'}}" :alignItems center :gap 8 :height 30 :grow 100
          (node :use tPicker
            #fill-source
            :with {:title Foreground
              :tip "Fill source: patterns aren't available yet"
              :width 90
              :open state.fillSourceMenu
              :items [{:title Foreground :checked true}]})
          (node :use tField
            #option-fill-opacity
            :with {:title Opacity :key fillOpacity :value "{{state.doc.options.fillOpacity}}"})
          (node :use tField
            #option-fill-tolerance
            :with {:title Tolerance
              :key tolerance
              :value "{{state.doc.options.tolerance}}"
              :max 255
              :unit ""
              :pop false
              :width 36})
          (node :use tCheck #option-fill-anti-alias :with {:title Anti-alias :checked true :off true})
          (node :use tCheck
            #option-fill-contiguous
            :with {:title Contiguous :key contiguous :checked "{{state.doc.options.contiguous}}"})
          (node :use tCheck
            #option-fill-all
            :with {:title "All Layers" :key allLayers :checked "{{state.doc.options.allLayers}}"}))
        (row #options-gradient :when "{{state.doc.tool == 'gradient'}}" :alignItems center :gap 8 :height 30 :grow 100
          (gradient #option-gradient
            :label Gradient
            :width 120
            :height 22
            :focusable true
            :value "{{state.gradientEdit}}"
            :accentColor @accent
            :onChange {:run setGradient :with {:args "{{event.value}}"}})
          (node :use tPicker
            #gradient-preset
            :with {:title ""
              :width 20
              :tip "Gradient presets"
              :open state.gradientMenu
              :items "{{map(state.gradients, {title: it, run: 'setOptions', with: {args: {gradientPreset: it}}})}}"})
          (node :use tChoice
            #option-shape
            :with {:key shape
              :value "{{state.doc.options.shape}}"
              :tip "Gradient shape"
              :tabs [(node #linear :icon align-vertical-space-around :title Linear)
                (node #radial :icon circle-dot :title Radial)
                (node #angle :icon rotate-cw :title Angle)
                (node #reflected :icon flip-vertical :title Reflected)
                (node #diamond :icon diamond :title Diamond)]})
          {:style sep}
          (node :use tPicker
            #gradient-mode
            :with {:title Normal
              :tip "Mode: other modes aren't available yet"
              :width 84
              :open state.paintModeMenu
              :items [{:title Normal :checked true}]})
          (node :use tField
            #option-gradient-opacity
            :with {:title Opacity :key fillOpacity :value "{{state.doc.options.fillOpacity}}"})
          (node :use tCheck #gradient-reverse :with {:title Reverse :checked false :off true})
          (node :use tCheck #gradient-dither :with {:title Dither :checked false :off true})
          (node :use tCheck #gradient-transparency :with {:title Transparency :checked true :off true}))
        (node :use tPicker
          #option-crop
          :when "{{state.doc.tool == 'crop'}}"
          :with {:title Ratio
            :tip Crop
            :open state.cropMenu
            :items [{:title "Crop to Selection" :run cropToSelection}
              {:title "Square, 1 : 1" :run cropToRatio :with {:args {:width 1 :height 1}}}
              {:title "4 : 3" :run cropToRatio :with {:args {:width 4 :height 3}}}
              {:title "3 : 2" :run cropToRatio :with {:args {:width 3 :height 2}}}
              {:title "16 : 9" :run cropToRatio :with {:args {:width 16 :height 9}}}
              {:title "Portrait, 4 : 5" :run cropToRatio :with {:args {:width 4 :height 5}}}]})
        (node :use tCheck
          #option-sample-all
          :when "{{state.doc.tool == 'eyedropper'}}"
          :with {:title "Sample All Layers" :key allLayers :checked "{{state.doc.options.allLayers}}"})
        (node :use tCheck
          #option-auto-select
          :when "{{state.doc.tool == 'move'}}"
          :with {:title Auto-Select :key autoSelect :checked "{{state.doc.options.autoSelect}}"})
        (node :style dim
          #option-move
          :when "{{state.doc.tool == 'move'}}"
          :text "{{state.doc.options.autoSelect ? 'Drag to move the layer painted under the pointer' : 'Drag to move the active layer'}}")
        (row #options-view
          :when "{{state.doc.tool == 'hand' || state.doc.tool == 'zoom'}}"
          :alignItems center
          :gap 4
          :height 30
          :grow 100
          (node :use tIcon #view-zoom-in :with {:icon zoom-in :run zoomIn :tip "Zoom In"})
          (node :use tIcon #view-zoom-out :with {:icon zoom-out :run zoomOut :tip "Zoom Out"})
          (node :use tPicker
            #view-actual
            :with {:title "100%"
              :width 50
              :tip Zoom
              :open state.zoomMenu
              :items [{:title "100%" :run actualPixels} {:title "Fit Screen" :run fitOnScreen}]}))
        {:grow 1 :height 1 :style clear}
        (row #options-end :width 112 :height 30 :alignItems center :justify end :gap 4
          (node :use tIcon #search :with {:icon search :tip "Search the commands (Cmd+F)" :run showSearch})
          (row #workspace-switcher
            :role button
            :label Workspace
            :tooltip "Choose a workspace"
            :width 40
            :height 22
            :alignItems center
            :gap 2
            :radius @kit.radius
            :cursor hand
            :hover {:color @hover}
            :onClick {:toggle state.workspaceMenu}
            {:type label :icon layout-panel-left :textColor @text :width 22 :height 20 :align center}
            {:type label :icon chevron-down :font small :textColor @muted :fitText true}
            {:use tMenu
              :with {:open state.workspaceMenu
                :placement below-end
                :width 200
                :items [{:title Standard :checked true :run resetWorkspace}
                  {:separator true}
                  {:title "Reset the Workspace" :run resetWorkspace}]}})
          {:width 6 :height 1 :style clear}))}
    :temperaToolBar {:node (column #tool-bar
        :role toolbar
        :label Tools
        :width 40
        :height "100%"
        :style card
        :alignItems center
        :gap 1
        {:height 6 :width 1 :style clear}
        (node #"tool-group-{{n}}"
          :repeat {:for g :in state.tools :index n}
          :style clear
          :let {:t "{{find(g, it.id == state.doc.tool) ?? g[0]}}"}
          :width 32
          :height "{{empty(g) ? 7 : 26}}"
          {:when "{{empty(g)}}" :x 14 :y 2 :width 3 :height 3 :radius 999 :color @border}
          (node :when "{{!empty(g)}}"
            :type label
            #"tool-{{t.id ?? n}}"
            :role button
            :label "{{t.title}}"
            :tooltip "{{t.title + (t.key ? ' (' + t.key + ')' : '') + (t.id ? '' : ', not available yet')}}"
            :disabled "{{!t.id}}"
            :selected "{{t.id == state.doc.tool}}"
            :icon "{{t.icon}}"
            :font iconsLarge
            :fill true
            :align center
            :radius 8
            :cursor hand
            :textColor "{{t.id ? '@text' : '@muted'}}"
            :color "{{t.id == state.doc.tool ? '@selected' : 'transparent'}}"
            :hover {:color "{{t.id ? '@hover' : 'transparent'}}"}
            :onClick {:run setTool :with {:args {:tool "{{t.id}}"}}}
            :onContextMenu {:toggle "{{'state.flyout.g' + n}}"}
            {:when "{{t.id == state.doc.tool}}" :x 0 :anchors {:top 6 :bottom 6} :width 3 :radius 2 :color @accent}
            {:when "{{length(g) > 1}}"
              :width 3
              :height 3
              :radius 999
              :color "{{t.id == state.doc.tool ? '@accent' : '@muted'}}"
              :anchors {:right 2 :bottom 2}}
            {:use tMenu
              :when "{{length(g) > 1}}"
              :with {:open "{{'state.flyout.g' + n}}"
                :placement right
                :icons true
                :mark square
                :width 230
                :items "{{map(g, {title: it.title, icon: it.icon, enabled: it.id != null, checked: it.id == state.doc.tool, keyLabel: slice(it.key ?? '', max(0, length(it.key ?? '') - 1)), run: 'setTool', with: {args: {tool: it.id}}})}}"}}))
        {:height 6 :width 1 :style clear}
        (node #colours :role group :label Colours :style clear :width 32 :height 36
          (node #background-colour
            :role button
            :label "Background colour"
            :x 11
            :y 12
            :width 19
            :height 19
            :radius 999
            :color "{{state.doc.background}}"
            :borderWidth 1
            :borderColor @muted
            :tooltip "Background colour"
            :cursor hand
            :onClick {:run showPanel :with {:args (node :slot colour #colour)}})
          {:x 1 :y 2 :width 21 :height 21 :color @panel :radius 999}
          (node #foreground-colour
            :role button
            :label "Foreground colour"
            :x 2
            :y 3
            :width 19
            :height 19
            :radius 999
            :color "{{state.doc.foreground}}"
            :borderWidth 1
            :borderColor @muted
            :tooltip "Foreground colour"
            :cursor hand
            :onClick {:run showPanel :with {:args (node :slot colour #colour)}})
          (label #colour-swap
            :role button
            :label "Swap colours"
            :icon arrow-left-right
            :font small
            :x 22
            :y 0
            :width 10
            :height 10
            :cursor hand
            :tooltip "Switch Foreground and Background Colours (X)"
            :textColor @muted
            :hover {:textColor @text}
            :onClick {:run swapColours})
          (node #colour-reset
            :role button
            :label "Default colours"
            :style clear
            :x 1
            :y 26
            :width 9
            :height 9
            :cursor hand
            :tooltip "Default Foreground and Background Colours (D)"
            :onClick {:run resetColours}
            {:x 3 :y 3 :width 6 :height 6 :radius 999 :color "#ffffff" :borderWidth 1 :borderColor @muted}
            {:width 6 :height 6 :radius 999 :color "#000000" :borderWidth 1 :borderColor @muted}))
        {:height 2 :width 1 :style clear}
        (node :use tIcon
          #quick-mask
          :width 32
          :height 24
          :with {:icon circle-dashed
            :tip "Paint the selection as a mask (Q)"
            :run toggleQuickMask
            :on "{{state.doc.options.quickMask}}"})
        (label #screen-mode
          :role button
          :label "Screen mode"
          :tooltip "Change Screen Mode (F)"
          :icon monitor
          :width 32
          :height 24
          :align center
          :radius 8
          :cursor hand
          :textColor @text
          :hover {:color @hover}
          :onClick {:run cycleScreenMode}
          :onContextMenu {:toggle state.screenMenu}
          {:width 3 :height 3 :radius 999 :color @muted :anchors {:right 2 :bottom 2}}
          {:use tMenu
            :with {:open state.screenMenu
              :placement right
              :mark square
              :width 250
              :items "{{[ {title: 'Standard Screen Mode', keyLabel: 'F', checked: state.screenMode == 'standard', run: 'setScreenMode', with: {args: {mode: 'standard'}}}, {title: 'Full Screen Mode with Menu Bar', keyLabel: 'F', checked: state.screenMode == 'full', run: 'setScreenMode', with: {args: {mode: 'full'}}}]}}"}})
        (node :use tIcon
          #edit-toolbar
          :width 32
          :height 24
          :with {:icon ellipsis :tip "Edit the toolbar, not available yet" :off true}))}
    :temperaCanvasArea {:node (column #canvas-area
        :role main
        :label Canvas
        :grow 1
        :height "100%"
        :color @canvasBack
        :radius @kit.cardRadius
        (tabs #document-tabs
          :label Documents
          :width "100%"
          :height 26
          :color @header
          :radius {:topLeft @kit.cardRadius :topRight @kit.cardRadius}
          :accentEdge top
          :tabs [(node #doc
              :closable true
              :label "{{state.docName}} — {{computed.activeLayer.name ?? ''}} · {{round(computed.zoomPercent)}}% · {{computed.mode}}")]
          :value doc
          :textColor @muted
          :activeTextColor @text
          :activeBackground @panel
          :accentColor @accent
          :onClose {:run closeDocument})
        (row #ruler-row :when "{{state.view.rulers}}" :width "100%" :height 16
          (node #ruler-corner :width 16 :height 16 :color @ruler)
          (node #ruler-top
            :label "Horizontal ruler"
            :grow 1
            :height 16
            :color @ruler
            :cursor hand
            :tooltip "Click to add a vertical guide"
            :onClick {:run addGuide
              :with {:args {:axis vertical :at "{{state.doc.view.x + (event.left - event.width / 2) / computed.px}}"}}}
            {:repeat {:for t :in computed.rulerMinorX}
              :x "{{computed.canvasWidth / 2 + (t - state.doc.view.x) * computed.px}}"
              :y 12
              :width 1
              :height 4
              :color @muted}
            {:repeat {:for t :in computed.rulerTicksX}
              :x "{{computed.canvasWidth / 2 + (t - state.doc.view.x) * computed.px}}"
              :width 1
              :height 16
              :color @muted}
            {:type label
              :repeat {:for t :in computed.rulerTicksX}
              :font small
              :text "{{t}}"
              :textColor @muted
              :x "{{computed.canvasWidth / 2 + (t - state.doc.view.x) * computed.px + 2}}"
              :y 0
              :fitText true}))
        (row #canvas-row :width "100%" :grow 1
          (node #ruler-left
            :label "Vertical ruler"
            :when "{{state.view.rulers}}"
            :width 16
            :height "100%"
            :color @ruler
            :cursor hand
            :tooltip "Click to add a horizontal guide"
            :onClick {:run addGuide
              :with {:args {:axis horizontal :at "{{state.doc.view.y + (event.top - event.height / 2) / computed.px}}"}}}
            {:repeat {:for t :in computed.rulerMinorY}
              :y "{{computed.canvasHeight / 2 + (t - state.doc.view.y) * computed.px}}"
              :x 12
              :width 4
              :height 1
              :color @muted}
            {:repeat {:for t :in computed.rulerTicksY}
              :y "{{computed.canvasHeight / 2 + (t - state.doc.view.y) * computed.px}}"
              :width 16
              :height 1
              :color @muted}
            {:type label
              :align {:horizontal start :vertical center}
              :repeat {:for t :in computed.rulerTicksY}
              :font small
              :text "{{t}}"
              :textColor @muted
              :y "{{computed.canvasHeight / 2 + (t - state.doc.view.y) * computed.px + 2}}"
              :x 1
              :width 15
              :height 12})
          (node #canvas-frame :style clear :grow 1 :height "100%"
            (raster #canvas :label Image :image doc :fill true :checker true)
            (node #guide-v
              :when "{{state.view.guides}}"
              :repeat {:for g :in state.guides.vertical}
              :x "{{computed.canvasWidth / 2 + (g - state.doc.view.x) * computed.px}}"
              :width 1
              :anchors {:top 0 :bottom 0}
              :color @guide)
            (node #guide-h
              :when "{{state.view.guides}}"
              :repeat {:for g :in state.guides.horizontal}
              :y "{{computed.canvasHeight / 2 + (g - state.doc.view.y) * computed.px}}"
              :height 1
              :anchors {:left 0 :right 0}
              :color @guide)))
        {:use temperaStatusBar})}
    :temperaStatusBar {:node (row #status-bar
        :role status
        :label Status
        :style bar
        :height 24
        :gap 12
        :radius {:bottomLeft @kit.cardRadius :bottomRight @kit.cardRadius}
        {:width 4 :height 1 :style clear}
        (node :style dim #status-message :live polite :text "{{state.doc.message}}")
        {:grow 1 :height 1 :style clear}
        (textarea #status-zoom
          :label Zoom
          :text "{{computed.zoomPercent}}%"
          :width 52
          :height 20
          :radius 999
          :padding 4
          :textColor @text
          :color @panel
          :hover {:color @input}
          :onSubmit {:run zoom
            :with {:args {:to "{{clamp(number(trim(split(event.text, '%')[0])) ?? computed.zoomPercent, 1, 3200) / 100}}"}}})
        (node :style dim
          #status-doc
          :when "{{state.statusInfo == 'doc'}}"
          :text "Doc: {{fixed(state.doc.width * state.doc.height * (state.doc.format == 'rgba16f' ? 6 : 3) / 1048576, 2)}}M")
        (node :style dim
          #status-size
          :when "{{state.statusInfo == 'size'}}"
          :text "{{state.doc.width}} × {{state.doc.height}} px")
        (node :style dim
          #status-profile
          :when "{{state.statusInfo == 'profile'}}"
          :text "{{state.doc.profile + (empty(state.doc.proof) ? '' : ' (proof: ' + state.doc.proof + ')')}}")
        (node :style dim
          #status-memory
          :when "{{state.statusInfo == 'memory'}}"
          :text "Undo {{fixed(state.doc.memory / 1048576, 1)}} MiB")
        (label #status-info
          :role button
          :label "Status information"
          :tooltip "Choose what the status bar shows"
          :icon chevron-right
          :width 18
          :height 18
          :align center
          :radius 3
          :cursor hand
          :textColor @muted
          :hover {:color @hover :textColor @text}
          :onClick {:toggle state.statusMenu}
          {:use tMenu
            :with {:open state.statusMenu
              :placement above
              :width 200
              :items "{{map([ {id: 'doc', title: 'Document Sizes'}, {id: 'size', title: 'Document Dimensions'}, {id: 'profile', title: 'Document Profile'}, {id: 'memory', title: 'Undo Memory'}], {title: it.title, checked: state.statusInfo == it.id, run: 'setStatusInfo', with: {args: {id: it.id}}})}}"}})
        {:width 4 :height 1 :style clear})}}}
