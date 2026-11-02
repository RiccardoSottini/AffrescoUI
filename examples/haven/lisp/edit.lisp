{:note "Haven's dashboard editing, included by index.screen.json: Arrange cards in the top bar's menu outlines each card with a pencil, and a card's editor changes its configuration in a visual form or as code."
 :state {:editing false
         :dashMenu false
         :cards {:lights (light-tiles :title "")
                 :scenes (scene-buttons :title "")
                 :security (alarm-panel :title "")
                 :climate (thermostat :title "" :modes true)
                 :cover (cover :title "")
                 :devices (device-tiles :title "" :actions true)}
         :cardEdit {:open false :id climate :code false :draft {} :text "" :error ""}}
 :computed {:cardTitle "{{ { lights: state.cards.lights.title || t('lights.title'), scenes: state.cards.scenes.title || t('scenes.label'), security: state.cards.security.title || t('alarm.section'), climate: state.cards.climate.title || t('climate.title'), cover: state.cards.cover.title || t('blinds.title'), devices: state.cards.devices.title || t('devices.title') } }}"
            :cardIcon "{{ { lights: 'lightbulb', scenes: 'clapperboard', security: 'shield', climate: 'thermometer', cover: 'blinds', devices: 'cpu' } }}"
            :cardCode "{{'{\\n' + join(map(entries(state.cardEdit.draft), '  ' + json(it.key) + ': ' + json(it.value)), ',\\n') + '\\n}'}}"}
 :actions {:editDashboard {:title "Arrange cards"
                           :category "View"
                           :icon pencil
                           :do [{:set state.page :to home} {:set state.editing :to true}]}
           :doneEditing [{:set state.editing :to false} {:set state.cardEdit.open :to false}]
           :editCard [{:note "with: card, the key of the card in state.cards"}
                      {:set state.cardEdit
                       :to {:open true :id "{{card}}" :code false :draft "{{state.cards[card]}}" :text "" :error ""}}]
           :showCardCode [{:set state.cardEdit.text :to "{{computed.cardCode}}"} {:set state.cardEdit.code :to true}]
           :showCardForm [{:if "{{type(parseJson(state.cardEdit.text)) == 'object'}}"
                           :then [{:set state.cardEdit.draft
                                   :to "{{merge(parseJson(state.cardEdit.text), { type: state.cards[state.cardEdit.id].type })}}"}
                                  {:set state.cardEdit.error :to ""}
                                  {:set state.cardEdit.code :to false}]
                           :else [{:set state.cardEdit.error :to "{{t('edit.badCode')}}"}]}]
           :saveCard [{:if "{{state.cardEdit.code}}" :then [{:run showCardForm}]}
                      {:if "{{empty(state.cardEdit.error)}}"
                       :then [{:set "{{'state.cards.' + state.cardEdit.id}}" :to "{{state.cardEdit.draft}}"}
                              {:set state.cardEdit.open :to false}]}]}
 :components {:editFrame {:note "While editing, a card's outline and pencil: a click anywhere on the card opens its editor"
                          :params {:card "" :name ""}
                          :node (node :when "{{state.editing}}"
                                  :color transparent
                                  :borderWidth 2
                                  :borderColor @editLine
                                  :radius 14
                                  :hover {:borderColor @accent :color @editVeil}
                                  :cursor hand
                                  :focusable true
                                  :role button
                                  :label "{{t('edit.card', { name: name })}}"
                                  :tooltip "{{t('edit.card', { name: name })}}"
                                  :onClick {:run editCard :with {:card "{{card}}"}}
                                  (label :icon pencil
                                    :font iconsSmall
                                    :text ""
                                    :textColor @onAccent
                                    :color @accent
                                    :anchors {:right 8 :top 4}
                                    :width 28
                                    :height 28
                                    :radius 14
                                    :align center))}
              :pill {:note "A rounded switch bound to the state path at, with its name: a track and a round thumb, drawn over a transparent toggle"
                     :params {:at "" :on false :name "" :thumb @accent :track @accentTrack}
                     :node (node :width 40 :height 24 :color transparent
                             (node :anchors {:left 3 :right 3 :top 5}
                               :height 14
                               :radius 7
                               :color "{{on ? track : '@off'}}")
                             (node :anchors {:left "{{on ? 18 : 0}}" :top 2}
                               :width 20
                               :height 20
                               :radius 10
                               :color "{{on ? thumb : '@thumbOff'}}"
                               :shadow {:y 1 :blur 3 :color "#00000047"})
                             (toggle :fill 0
                               :bind "{{at}}"
                               :label "{{name}}"
                               :cursor hand
                               :trackColor transparent
                               :activeColor transparent
                               :knobColor transparent))}
              :cardEditor {:note "A card's configuration: its title and options in a form, or all of it as code; Save keeps it, Cancel or Escape drops it"
                           :params {}
                           :node (node :dir auto
                                   :overlay {:placement center :open state.cardEdit.open :dismiss outside :modal true}
                                   :width 880
                                   :height 440
                                   :radius 28
                                   :color @card
                                   :shadow {:y 8 :blur 24 :color "#00000040"}
                                   :role dialog
                                   :label "{{t('edit.cardTitle', { kind: t('edit.kind.' + state.cardEdit.id) })}}"
                                   (label#cardEditorTitle :font heading
                                     :textColor @text
                                     :text "{{t('edit.cardTitle', { kind: t('edit.kind.' + state.cardEdit.id) })}}"
                                     :anchors {:left 24 :right 24 :top 20}
                                     :height 24)
                                   (node#cardForm :when "{{!state.cardEdit.code}}"
                                     :anchors {:left 24 :right 360 :top 64 :bottom 72}
                                     :color transparent
                                     (node#cardTitleBox :anchors {:left 0 :right 0 :top 0}
                                       :height 56
                                       :color @raised
                                       :hover {:color @raisedHover}
                                       :radius {:topLeft 4 :topRight 4}
                                       (label :font small
                                         :text "{{t('edit.title')}}"
                                         :textColor @muted
                                         :anchors {:left 16 :top 6}
                                         :height 16
                                         :fitText true)
                                       (textarea#cardTitleField :bind state.cardEdit.draft.title
                                         :placeholder "{{computed.cardTitle[state.cardEdit.id]}}"
                                         :placeholderColor @muted
                                         :textColor @text
                                         :color transparent
                                         :padding 16
                                         :anchors {:left 0 :right 0 :top 18 :bottom 0}
                                         :label "{{t('edit.title')}}")
                                       (node :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted))
                                     (node#cardOption :repeat {:for opt
                                                               :in "{{filter(keys(state.cardEdit.draft), it != 'type' && it != 'title')}}"
                                                               :index i}
                                       :anchors {:left 0 :right 0 :top "{{76 + i * 52}}"}
                                       :height 44
                                       :color transparent
                                       (label :text "{{t('edit.opt.' + opt)}}"
                                         :textColor @text
                                         :anchors {:left 0 :right 56 :top 0 :bottom 0}
                                         :align {:horizontal start :vertical center})
                                       (node#cardOptionSwitch :use pill
                                         :with {:at "{{'state.cardEdit.draft.' + opt}}"
                                                :on "{{state.cardEdit.draft[opt]}}"
                                                :name "{{t('edit.opt.' + opt)}}"}
                                         :anchors {:right 0 :top 10})))
                                   (editor#cardCode :when "{{state.cardEdit.code}}"
                                     :bind state.cardEdit.text
                                     :lineNumbers true
                                     :font symbols
                                     :textColor @text
                                     :lineNumberColor @muted
                                     :activeLineNumberColor @text
                                     :lineColor @raised
                                     :caretColor @accent
                                     :color @field
                                     :radius 4
                                     :dir ltr
                                     :anchors {:left 24 :right 360 :top 64 :bottom 96}
                                     :label "{{t('edit.code')}}")
                                   (label#cardCodeError :when "{{state.cardEdit.code && !empty(state.cardEdit.error)}}"
                                     :font small
                                     :text "{{state.cardEdit.error}}"
                                     :textColor @alert
                                     :anchors {:left 24 :bottom 74}
                                     :fitText true
                                     :live assertive)
                                   (node#cardPreview :anchors {:right 0 :top 0 :bottom 65}
                                     :width 336
                                     :color @background
                                     :role group
                                     :label "{{t('edit.preview')}}"
                                     :radius {:topRight 28}
                                     (node :style caption :text "{{t('edit.preview')}}" :anchors {:left 24 :top 24})
                                     (node :style card
                                       :anchors {:left 24 :right 24 :top 56}
                                       :height "{{state.cardEdit.id == 'climate' ? (state.cardEdit.draft.modes ? 312 : 256) : (state.cardEdit.draft.actions ? 124 : 68)}}"
                                       (node :style cardTitle
                                         :icon "{{computed.cardIcon[state.cardEdit.id]}}"
                                         :text " {{state.cardEdit.draft.title || computed.cardTitle[state.cardEdit.id]}}"
                                         :anchors {:left 24 :right 16 :top 20}
                                         :fitText false
                                         :height 28)
                                       (node :when "{{state.cardEdit.id == 'climate'}}"
                                         :style caption
                                         :text "{{t('climate.now', { temp: computed.room.temp, humidity: computed.room.humidity })}}"
                                         :anchors {:right 16 :top 26})
                                       (node :when "{{state.cardEdit.id == 'climate'}}"
                                         :use thermoDial
                                         :with {:p preview :small true}
                                         :anchors {:left 69 :top 44})
                                       (row :when "{{state.cardEdit.id == 'climate'}}"
                                         :anchors {:left 0 :right 0 :top 200}
                                         :height 40
                                         :gap 12
                                         :justify center
                                         (label :repeat {:for m :in "{{['minus', 'plus']}}"}
                                           :icon "{{m}}"
                                           :text ""
                                           :textColor @text
                                           :color @raised
                                           :width 40
                                           :height 40
                                           :radius 20
                                           :align center))
                                       (row :when "{{state.cardEdit.id == 'climate' && state.cardEdit.draft.modes == true}}"
                                         :anchors {:left 0 :right 0 :bottom 16}
                                         :height 44
                                         :gap 12
                                         :justify center
                                         (label :repeat {:for mode :in state.modes}
                                           :icon "{{mode == 'Heat' ? 'flame' : (mode == 'Cool' ? 'snowflake' : 'sun-snow')}}"
                                           :text ""
                                           :color "{{mode == state.mode ? computed.modeSoft : '@raised'}}"
                                           :textColor "{{mode == state.mode ? computed.modeColor : '@muted'}}"
                                           :width 44
                                           :height 44
                                           :radius 22
                                           :align center))
                                       (row :when "{{state.cardEdit.draft.actions == true}}"
                                         :anchors {:left 16 :right 16 :bottom 16}
                                         :height 40
                                         :gap 8
                                         (label :repeat {:for d :in "{{state.devices}}"}
                                           :font small
                                           :text "{{d.name}}"
                                           :textColor @text
                                           :color @raised
                                           :grow 1
                                           :basis 0
                                           :height 40
                                           :radius 8
                                           :align center))))
                                   (node :anchors {:left 0 :right 0 :bottom 64} :height 1 :color @divider)
                                   (node#cardCodeSwitch :style button
                                     :color transparent
                                     :hover {:color transparent}
                                     :textColor @accent
                                     :padding 12
                                     :fitText true
                                     :text "{{t(state.cardEdit.code ? 'edit.showForm' : 'edit.showCode')}}"
                                     :anchors {:left 16 :bottom 14}
                                     :height 36
                                     :onClick {:if "{{state.cardEdit.code}}"
                                               :then [{:run showCardForm}]
                                               :else [{:run showCardCode}]})
                                   (node#cardSave :style button
                                     :color @accent
                                     :hover {:color @accent}
                                     :textColor @onAccent
                                     :text "{{t('edit.save')}}"
                                     :anchors {:right 16 :bottom 14}
                                     :width 88
                                     :height 36
                                     :radius 18
                                     :onClick saveCard)
                                   (node#cardCancel :style button
                                     :color transparent
                                     :hover {:color @accentSoft}
                                     :textColor @accent
                                     :text "{{t('edit.cancel')}}"
                                     :anchors {:right 112 :bottom 14}
                                     :width 88
                                     :height 36
                                     :radius 18
                                     :onClick {:set state.cardEdit.open :to false}))}}}
