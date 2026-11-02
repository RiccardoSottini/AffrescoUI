{:$schema "../../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :note "The ribbon's parts: large buttons (an icon over a one- or two-line title), small ones (an icon, a title, a menu arrow), icon-only ones, drop-down fields, galleries, and groups, each a card with its title along the top. Each tab is a component of groups; the screen shows the chosen one. A button is a label, so the use gives it its onClick, tooltip and menu."
 :components {:rbBig {:params {:icon "" :title "" :menu false :on false :enabled true :width 48 :drop ""}
                      :note "With drop (the button's own id), a split button: the arrow under the title (id: drop + '-more') opens that menu, the rest runs the use's onClick."
                      :node (column :width "{{width}}" :height 70 :color transparent :disabled "{{!enabled}}"
                              (label :width "{{width}}"
                                :height "{{drop == '' ? 70 : 52}}"
                                :padding 2
                                :wrap true
                                :style "{{!enabled ? 'rbButtonOff' : on ? 'rbButtonOn' : 'rbButton'}}"
                                :align {:horizontal center :vertical start}
                                :runs "{{concat([{ icon: icon, size: 30 }, { text: '\n', size: 6 }, { text: drop == '' ? title : join(split(title, '\n'), ' '), size: drop == '' ? 12 : 11 }], menu && drop == '' ? [{ text: ' ', size: 12 }, { icon: 'chevron-down', size: 10 }] : [])}}")
                              (label :id "{{drop + '-more'}}"
                                :when "{{drop != ''}}"
                                :width "{{width}}"
                                :height 18
                                :align center
                                :disabled "{{!enabled}}"
                                :tooltip "More options"
                                :style "{{!enabled ? 'rbButtonOff' : 'rbButton'}}"
                                :runs [{:icon chevron-down :size 10}]
                                :onClick {:run openDrop :with {:id "{{drop}}"}}))}
              :rbSmall {:params {:icon ""
                                 :title ""
                                 :menu false
                                 :on false
                                 :enabled true
                                 :tint nil
                                 :drop ""
                                 :colors false}
                        :note "With drop (the button's own id), a split button: its arrow (id: drop + '-arrow') opens that menu (or, with colors, the colour grid), the rest runs the use's onClick."
                        :node (column :fitContent width :height 24 :color transparent :disabled "{{!enabled}}"
                                (row :fitContent width :height "{{tint == null ? 24 : 21}}" :color transparent
                                  (label :height "{{tint == null ? 24 : 21}}"
                                    :fitText true
                                    :padding 4
                                    :style "{{!enabled ? 'rbButtonOff' : on ? 'rbButtonOn' : 'rbButton'}}"
                                    :runs "{{concat([{ icon: icon, size: 16 }], title == '' ? [] : [{ text: ' ' + title, size: 12 }], menu && drop == '' ? [{ text: ' ', size: 12 }, { icon: 'chevron-down', size: 10 }] : [])}}")
                                  (label :id "{{drop + '-arrow'}}"
                                    :when "{{drop != ''}}"
                                    :width 13
                                    :height "{{tint == null ? 24 : 21}}"
                                    :align center
                                    :disabled "{{!enabled}}"
                                    :tooltip "More options"
                                    :style "{{!enabled ? 'rbButtonOff' : 'rbButton'}}"
                                    :runs [{:icon chevron-down :size 10}]
                                    :onClick {:run "{{colors ? 'openColors' : 'openDrop'}}" :with {:id "{{drop}}"}}))
                                (node :width 16
                                  :height 3
                                  :x 4
                                  :color "{{computed.palette[tint] ?? tint ?? 'transparent'}}"
                                  :when "{{tint != null}}"))}
              :rbField {:params {:text "" :width 100 :drop ""}
                        :note "A drop-down field: a click anywhere on it opens the menu named drop (the field's own id)."
                        :node (row :width "{{width}}"
                                :height 22
                                :role button
                                :label "{{text}}"
                                :onClick {:run openDrop :with {:id "{{drop}}"}}
                                :color @rbFieldBg
                                :borderWidth 1
                                :borderColor @rbFieldLine
                                :radius 11
                                :cursor hand
                                :hover {:borderColor @rbFieldHover}
                                :alignItems center
                                (label :grow 1
                                  :height 20
                                  :padding 7
                                  :textColor @rbInk
                                  :runs [{:text "{{text}}" :size 12}])
                                (label :width 16
                                  :height 20
                                  :align center
                                  :textColor @rbInk
                                  :runs [{:icon chevron-down :size 11}]))}
              :rbGroup {:params {:title "" :launcher false}
                        :note "A group is a card: its title in small capitals along the top, the launcher at the title's end (it opens the format pane), its commands below."
                        :node (column :fitContent width
                                :height 92
                                :radius 10
                                :color @card
                                :borderWidth 1
                                :borderColor @rbLine
                                :alignItems stretch
                                (row :fitContent width :height 18 :color transparent :alignItems center :gap 4
                                  (node :width 6 :height 1 :color transparent)
                                  (label :style rbGroupTitle
                                    :fitText true
                                    :runs [{:text "{{upper(title)}}" :size 9 :bold true}])
                                  (node :grow 1 :height 1 :color transparent)
                                  (label :id "{{'launch-' + join(split(lower(title), ' '), '-')}}"
                                    :when "{{launcher}}"
                                    :width 16
                                    :height 16
                                    :style rbButton
                                    :align center
                                    :tooltip "{{'More ' + lower(title) + ' options'}}"
                                    :role button
                                    :label "{{title + ' options'}}"
                                    :runs [{:icon settings-2 :size 10}]
                                    :onClick openFormatPane)
                                  (node :width 4 :height 1 :color transparent))
                                (row :fitContent width :height 72 :gap 2 :color transparent :alignItems start
                                  (node :width 3 :height 1 :color transparent)
                                  (row :fitContent width :height 72 :gap 2 :color transparent :alignItems start
                                    (node :slot true))
                                  (node :width 3 :height 1 :color transparent)))}
              :rbStack {:params {}
                        :node (column :fitContent width :height 72 :gap 0 :color transparent :justify space-evenly
                                (node :slot true))}
              :rbRow {:params {}
                      :node (row :fitContent width :height 24 :gap 1 :color transparent :alignItems center
                              (node :slot true))}
              :rbGallery {:params {:width 200 :more ""}
                          :note "A ribbon gallery: one row of cells (the use's children). Every cell is shown, so the arrows that scroll are off; the last opens the whole list as a menu (more, the gallery's own id), when there is one."
                          :node (row :width "{{width}}"
                                  :height 70
                                  :color @rbFieldBg
                                  :borderWidth 1
                                  :borderColor @rbFieldLine
                                  :radius 10
                                  (node :width 4 :height 68 :color transparent)
                                  (row :grow 1 :height 68 :gap 2 :color transparent :alignItems center :overflow clip
                                    (node :slot true))
                                  (node :width 2 :height 68 :color transparent)
                                  (column :width 16 :height 68 :color transparent
                                    (label :width 16
                                      :height 22
                                      :style rbButtonOff
                                      :align center
                                      :tooltip "Every item is shown"
                                      :runs [{:icon chevron-up :size 10}])
                                    (label :width 16
                                      :height 22
                                      :style rbButtonOff
                                      :align center
                                      :tooltip "Every item is shown"
                                      :runs [{:icon chevron-down :size 10}])
                                    (label :id "{{more + '-all'}}"
                                      :width 16
                                      :height 22
                                      :style "{{more == '' ? 'rbButtonOff' : 'rbButton'}}"
                                      :align center
                                      :disabled "{{more == ''}}"
                                      :tooltip "{{more == '' ? 'Every item is shown' : 'The whole list'}}"
                                      :runs [{:icon chevrons-down :size 10}]
                                      :onClick {:run openDrop :with {:id "{{more}}"}})))}}}
