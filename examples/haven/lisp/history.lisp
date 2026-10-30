{:note "Haven's logbook and history pages, included by index.screen.json: the logbook lists today's and yesterday's events with a search; history draws a day of on and off states per device and the living room's temperature. The events are invented."
  :state {:logQuery ""}
  :stateTypes {:logQuery {:type string :maxLength 64}}
  :computed {:logShown "{{filter(data.logbook, state.logQuery == '' || contains(lower(it.rule != '' ? t('rule.' + it.rule) : it.name), lower(trim(state.logQuery))))}}"}
  :components {:logbookPage {:params {}
      :node {:dir auto
        :color transparent
        :children [(node #logBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
            {:type label
              :text "{{t('page.logbook')}}"
              :font heading
              :textColor @onHeader
              :anchors {:left 24 :top 0 :bottom 0}
              :fitText true
              :align {:horizontal start :vertical center}}
            (node #logRange
              :type label
              :font iconsMedium
              :icon calendar
              :text ""
              :textColor @off
              :anchors {:right 16 :top 8}
              :width 40
              :height 40
              :radius 20
              :align center
              :role button
              :label "{{t('energy.pick')}}"
              :tooltip "{{t('log.twoDays')}}"
              :cursor arrow
              :disabled true))
          (node #logFilters :type row :anchors {:left 24 :right 24 :top 72} :height 48 :gap 16
            (node #logPeriod
              :type label
              :icon calendar
              :text "  {{t('log.period')}}"
              :textColor @text
              :width 280
              :height 48
              :padding 12
              :color @field
              :borderWidth 1
              :borderColor @border
              :radius 4
              :align {:horizontal start :vertical center}
              :role button
              :label "{{t('log.period')}}"
              :tooltip "{{t('log.twoDays')}}"
              :cursor arrow
              :disabled true)
            (node #logSearchBox
              :width 320
              :height 48
              :radius 24
              :color transparent
              :borderWidth 1
              :borderColor @border
              :hover {:color @accentSoft}
              {:type label
                :icon "{{state.logQuery == '' ? 'plus' : 'search'}}"
                :text ""
                :textColor @accent
                :anchors {:left 14 :top 0 :bottom 0}
                :width 24
                :align center}
              (node #logSearch
                :type textarea
                :bind state.logQuery
                :placeholder "{{t('hist.choose')}}"
                :placeholderColor @accent
                :textColor @text
                :color transparent
                :anchors {:left 44 :right 16 :top 0 :bottom 0}
                :label "{{t('hist.choose')}}")))
          (node #logCard :style card :anchors {:left 24 :right 24 :top 136 :bottom 24}
            {:style caption
              :when "{{length(computed.logShown) == 0}}"
              :text "{{t('log.empty')}}"
              :anchors {:left 24 :top 24}}
            (node #logList
              :type column
              :anchors {:left 0 :right 0 :top 8 :bottom 8}
              :scroll vertical
              :role list
              :label "{{t('page.logbook')}}"
              (node #logDay
                :repeat {:for day :in "['today', 'yesterday']" :key "{{day}}"}
                :let {:events "{{filter(computed.logShown, it.day == day)}}"}
                :when "{{length(events) > 0}}"
                :anchors {:left 0 :right 0}
                :height "{{40 + length(events) * 56}}"
                :color transparent
                :role group
                :label "{{t('log.' + day)}}"
                {:style caption :text "{{upper(t('log.' + day))}}" :anchors {:left 24 :top 16}}
                {:type column
                  :anchors {:left 0 :right 0 :top 40 :bottom 0}
                  :children [(node #logRow
                      :repeat {:for ev :in events :key "{{ev.id}}"}
                      :anchors {:left 0 :right 0}
                      :height 56
                      :color transparent
                      :hover {:color @cardHover}
                      :role listitem
                      :label "{{ev.rule != '' ? t('rule.' + ev.rule) : ev.name}}: {{t('log.' + ev.what)}}, {{ev.time}}"
                      {:type label
                        :icon "{{ev.icon}}"
                        :text ""
                        :anchors {:left 20 :top 8}
                        :width 40
                        :height 40
                        :radius 20
                        :align center
                        :color "{{ev.rule != '' ? '@accentSoft' : '@raised'}}"
                        :textColor "{{ev.rule != '' ? '@accent' : '@muted'}}"}
                      {:type label
                        :text "{{ev.rule != '' ? t('rule.' + ev.rule) : ev.name}} {{t('log.' + ev.what)}}"
                        :textColor @text
                        :anchors {:left 76 :right 24 :top 9}
                        :height 20}
                      {:type label
                        :font small
                        :text "{{ev.time}}"
                        :textColor @muted
                        :anchors {:left 76 :top 31}
                        :fitText true})]})))]}}
    :historyPage {:params {}
      :node {:dir auto
        :color transparent
        :children [(node #histBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
            {:type label
              :text "{{t('page.history')}}"
              :font heading
              :textColor @onHeader
              :anchors {:left 24 :top 0 :bottom 0}
              :fitText true
              :align {:horizontal start :vertical center}}
            (node #histRange
              :type label
              :font iconsMedium
              :icon calendar
              :text ""
              :textColor @off
              :anchors {:right 16 :top 8}
              :width 40
              :height 40
              :radius 20
              :align center
              :role button
              :label "{{t('energy.pick')}}"
              :tooltip "{{t('energy.onlyToday')}}"
              :cursor arrow
              :disabled true))
          (node #histFilters :type row :anchors {:left 24 :right 24 :top 72} :height 48 :gap 16
            (node #histPeriod
              :type label
              :icon calendar
              :text "  {{t('hist.today')}}"
              :textColor @text
              :width 280
              :height 48
              :padding 12
              :color @field
              :borderWidth 1
              :borderColor @border
              :radius 4
              :align {:horizontal start :vertical center}
              :role button
              :label "{{t('hist.today')}}"
              :tooltip "{{t('energy.onlyToday')}}"
              :cursor arrow
              :disabled true)
            (node #histTarget
              :type label
              :icon plus
              :text "  {{t('hist.choose')}}"
              :textColor @accent
              :width 320
              :height 48
              :padding 12
              :color transparent
              :borderWidth 1
              :borderColor @border
              :radius 24
              :align {:horizontal start :vertical center}
              :hover {:color @accentSoft}
              :cursor hand
              :focusable true
              :role button
              :label "{{t('hist.choose')}}"
              :onClick {:set state.quickPick.open :to true}))
          (node #histTimeline
            :style card
            :anchors {:left 24 :right 24 :top 136}
            :height "{{76 + length(data.timeline) * 40 + 28}}"
            :role group
            :label "{{t('hist.timeline')}}"
            {:style cardTitle :text "{{t('hist.timeline')}}"}
            {:type column
              :anchors {:left 24 :right 24 :top 60}
              :height "{{length(data.timeline) * 40}}"
              :role list
              :children [(node #histEntity
                  :repeat {:for ent :in data.timeline :key "{{ent.id}}"}
                  :anchors {:left 0 :right 0}
                  :height 40
                  :color transparent
                  :role listitem
                  :label "{{ent.name}}"
                  {:type label
                    :text "{{ent.name}}"
                    :textColor @text
                    :anchors {:left 0 :top 0 :bottom 0}
                    :width 140
                    :align {:horizontal start :vertical center}}
                  {:type row
                    :anchors {:left 148 :right 0 :top 6}
                    :height 28
                    :gap 0
                    :children [{:repeat {:for seg :in ent.segs :index si}
                        :type label
                        :grow "{{seg.h}}"
                        :basis 0
                        :anchors {:top 0 :bottom 0}
                        :radius {:topLeft "{{si == 0 ? 4 : 0}}"
                          :bottomLeft "{{si == 0 ? 4 : 0}}"
                          :topRight "{{si == length(ent.segs) - 1 ? 4 : 0}}"
                          :bottomRight "{{si == length(ent.segs) - 1 ? 4 : 0}}"}
                        :color "{{seg.on ? '@accent' : '@raised'}}"
                        :textColor "{{seg.on ? '@onAccent' : '@muted'}}"
                        :font small
                        :align center
                        :text "{{seg.h >= 2 ? t(ent.k + (seg.on ? '.on' : '.off')) : ''}}"
                        :tooltip "{{t(ent.k + (seg.on ? '.on' : '.off'))}}"}]})]}
            {:type row
              :anchors {:left 172 :right 24 :bottom 12}
              :height 20
              :justify space-between
              :children [{:style caption :text "00:00"}
                {:style caption :text "06:00"}
                {:style caption :text "12:00"}
                {:style caption :text "18:00"}
                {:style caption :text "{{t('hist.now')}}"}]})
          (node #histTemp
            :style card
            :anchors {:left 24 :right 24 :top "{{136 + 104 + length(data.timeline) * 40 + 16}}" :bottom 24}
            :role group
            :label "{{t('hist.temp', { name: state.rooms[0].name })}}"
            {:style cardTitle :text "{{t('hist.temp', { name: state.rooms[0].name })}}"}
            {:style caption
              :text "{{t('hist.range', { min: min(state.tempHistory), max: max(state.tempHistory) })}}"
              :anchors {:right 24 :top 24}}
            (node #histScale :type column :anchors {:left 24 :right 24 :top 52 :bottom 32} :justify space-between
              {:repeat {:for deg :in "{{[23, 22, 21, 20, 19]}}"}
                :anchors {:left 0 :right 0}
                :height 16
                :color transparent
                :children [{:type label
                    :font small
                    :text "{{deg}}°"
                    :textColor @muted
                    :anchors {:left 0 :top 0}
                    :width 28
                    :height 16
                    :align {:horizontal end :vertical center}}
                  {:anchors {:left 40 :right 0 :top 8} :height 1 :color @divider}]})
            {:use lineChart
              :with {:values "{{map(state.tempHistory, it - 19)}}" :top 4 :color @warm :lineWidth 2}
              :anchors {:left 64 :right 24 :top 60 :bottom 40}}
            {:type row
              :anchors {:left 64 :right 24 :bottom 12}
              :height 16
              :justify space-between
              :children [{:repeat {:for h :in "{{['00:00', '06:00', '12:00', '18:00', t('hist.now')]}}"}
                  :type label
                  :font small
                  :text "{{h}}"
                  :textColor @muted
                  :fitText true}]})]}}}}
