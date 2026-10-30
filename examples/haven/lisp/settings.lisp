{:note "Haven's settings page, included by index.screen.json: grouped cards of rows that open the other pages or a sub-page (devices and entities, profile, general settings from the schema, about)."
  :state {:settingsView main
    :langMenu false
    :genMenu false
    :devTab devices
    :devQuery ""
    :settingsGroups [(node #home
        :rows [(node #devices :icon cpu :color "#4a50b8")
          (node #automations :icon bot :color "#2e8b63")
          (node #areas :icon layout-grid :color "#d9573a")
          (node #energy :icon zap :color "#c99312")])
      (node #you
        :rows [(node #profile :icon user :color "#9a4f9e")
          (node #general :icon sliders-horizontal :color "#6b6f80")
          (node #about :icon info :color "#3f8f8a")])]}
  :stateTypes {:settingsView {:enum [main devices profile general about]}
    :settingsGroups {:type array :items {:type object}}}
  :actions {:pickSetting [{:note "with: key, a settings key, and value"} {:set "{{'settings.' + key}}" :to "{{value}}"}]
    :pickLanguage [{:note "with: lang, a language code"} {:set settings.ui.language :to "{{lang}}"}]}
  :computed {:subPages "{{['devices', 'profile', 'general', 'about']}}"
    :devInBar "{{window.width - computed.sideWidth >= 900}}"
    :devTop "{{computed.devInBar ? 56 : 104}}"
    :deviceMakers "{{ { door: ['Keystead', 0, 'Zigbee'], vacuum: ['Tidyworks', 1, 'Wi-Fi'], speaker: ['Aurelle', 0, 'Wi-Fi'] } }}"
    :deviceRows "{{filter(map(state.devices, { id: it.id, cells: [it.name, computed.deviceMakers[it.id][0], t('kind.' + it.id), state.rooms[computed.deviceMakers[it.id][1]].name, computed.deviceMakers[it.id][2]] }), contains(lower(it.cells[0]), lower(state.devQuery)))}}"
    :deviceColumns "{{[{ id: 'name', title: t('table.device'), width: floor((window.width - computed.sideWidth - 48) * 0.28) }, { id: 'maker', title: t('table.maker'), width: floor((window.width - computed.sideWidth - 48) * 0.18) }, { id: 'kind', title: t('table.model'), width: floor((window.width - computed.sideWidth - 48) * 0.18) }, { id: 'area', title: t('table.area'), width: floor((window.width - computed.sideWidth - 48) * 0.18) }, { id: 'via', title: t('table.integration'), width: floor((window.width - computed.sideWidth - 48) * 0.18) }]}}"
    :roomEntityRows "{{filter(map(state.rooms, { id: 'r' + index, cells: [it.name, t('summary.lights', { on: length(filter(it.lights, it.on)), total: length(it.lights) }), it.temp + '°  →  ' + it.target + '°'] }), contains(lower(it.cells[0]), lower(state.devQuery)))}}"
    :roomEntityColumns "{{[{ id: 'room', title: t('table.room'), width: floor((window.width - computed.sideWidth - 48) * 0.4) }, { id: 'lights', title: t('lights.title'), width: floor((window.width - computed.sideWidth - 48) * 0.3) }, { id: 'climate', title: t('climate.title'), width: floor((window.width - computed.sideWidth - 48) * 0.3) }]}}"}
  :components {:settingsRow {:note "One row of a settings card: an icon in a circle of colour, a title and a line under it, a chevron."
      :params {:row {}}
      :node {:anchors {:left 0 :right 0}
        :height 72
        :color transparent
        :hover {:color @cardHover}
        :cursor hand
        :focusable true
        :role button
        :label "{{t('set.' + row.id)}}"
        :onClick [{:set state.settingsView :to "{{contains(computed.subPages, row.id) ? row.id : 'main'}}"}
          {:set state.page :to "{{contains(computed.subPages, row.id) ? 'settings' : row.id}}"}]
        :children [{:type label
            :font icons
            :icon "{{row.icon}}"
            :text ""
            :textColor "#ffffff"
            :color "{{row.color}}"
            :anchors {:left 16 :top 16}
            :width 40
            :height 40
            :radius 20
            :align center}
          {:type label
            :text "{{t('set.' + row.id)}}"
            :textColor @text
            :anchors {:left 72 :right 48 :top 14}
            :height 22
            :align {:horizontal start :vertical center}}
          {:type label
            :font small
            :text "{{t('set.' + row.id + '.sub')}}"
            :textColor @muted
            :anchors {:left 72 :right 48 :top 38}
            :height 20
            :align {:horizontal start :vertical center}}
          {:type label
            :font icons
            :icon chevron-right
            :mirror true
            :text ""
            :textColor @muted
            :anchors {:right 16 :top 24}
            :width 24
            :height 24
            :align center}]}}
    :settingsRadio {:note "A radio button and its label, filled with the accent when it is the chosen one."
      :params {:text "" :chosen false}
      :node {:type row
        :height 36
        :width 104
        :gap 10
        :alignItems center
        :color transparent
        :cursor hand
        :focusable true
        :role radio
        :label "{{text}}"
        :checked "{{chosen}}"
        :children [{:width 20
            :height 20
            :radius 10
            :color transparent
            :borderWidth 2
            :borderColor "{{chosen ? '@accent' : '@muted'}}"
            :children [{:when "{{chosen}}" :anchors {:left 5 :top 5} :width 10 :height 10 :radius 5 :color @accent}]}
          {:type label :text "{{text}}" :textColor @text :fitText true}]}}
    :settingsSection {:note "A row of the profile card: its title and line on the start side, its choices on the end side."
      :params {:title "" :sub ""}
      :node {:anchors {:left 0 :right 0}
        :height 76
        :color transparent
        :role radiogroup
        :label "{{title}}"
        :children [{:type label :text "{{title}}" :textColor @text :anchors {:left 20 :top 16} :height 22 :fitText true}
          {:type label
            :font small
            :text "{{sub}}"
            :textColor @muted
            :anchors {:left 20 :top 40}
            :height 20
            :fitText true}
          {:type row :anchors {:right 20 :top 20} :width 360 :height 36 :gap 8 :justify end :children [{:slot true}]}]}}
    :settingsPageMain {:params {}
      :node {:dir auto
        :color transparent
        :children [(node #settingsBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
            (node #settingsBack
              :type label
              :font icons
              :icon arrow-left
              :mirror true
              :text ""
              :textColor @onHeader
              :when "{{state.settingsView != 'main'}}"
              :anchors {:left 8 :top 8}
              :width 40
              :height 40
              :radius 20
              :align center
              :hover {:color @headerHover}
              :cursor hand
              :focusable true
              :role button
              :label "{{t('set.back')}}"
              :tooltip "{{t('set.back')}}"
              :onClick {:set state.settingsView :to main})
            (node #settingsTitle
              :type label
              :font heading
              :textColor @onHeader
              :fitText true
              :text "{{state.settingsView == 'main' ? t('page.settings') : t('set.' + state.settingsView)}}"
              :anchors {:left "{{state.settingsView == 'main' ? 24 : 64}}" :top 0 :bottom 0}
              :align {:horizontal start :vertical center}))
          (node #settingsMain
            :when "{{state.settingsView == 'main'}}"
            :type column
            :anchors {:left 24 :right 24 :top 80 :bottom 0}
            :gap 24
            :scroll vertical
            (node #settingsGroup
              :repeat {:for g :in state.settingsGroups :key "{{g.id}}"}
              :style card
              :anchors {:left 0 :right 0}
              :height "{{length(g.rows) * 72 + 16}}"
              :role list
              :label "{{t('set.group.' + g.id)}}"
              {:type column
                :anchors {:left 0 :right 0 :top 8 :bottom 8}
                :gap 0
                :children [(node :use settingsRow
                    #settingsRow
                    :repeat {:for r :in g.rows :key "{{r.id}}"}
                    :with {:row "{{r}}"})]}))
          (node #devTabs
            :when "{{state.settingsView == 'devices'}}"
            :type row
            :anchors {:left "{{computed.devInBar ? 260 : 0}}"
              :right "{{computed.devInBar ? 24 : 0}}"
              :top "{{computed.devInBar ? 0 : 56}}"}
            :height "{{computed.devInBar ? 56 : 48}}"
            :color "{{computed.devInBar ? 'transparent' : '@card'}}"
            :justify center
            :role tablist
            :arrows horizontal
            :label "{{t('set.devices')}}"
            (node #devTab
              :repeat {:for tab :in "['integrations', 'devices', 'entities', 'helpers']"}
              :type label
              :text "{{t('dev.' + tab)}}"
              :textColor "{{computed.devInBar ? (state.devTab == tab ? '@onHeader' : '@onHeaderDim') : (state.devTab == tab ? '@accent' : '@muted')}}"
              :anchors {:top 0 :bottom 0}
              :width 128
              :align center
              :hover {:color "{{computed.devInBar ? '@headerHover' : '@cardHover'}}"}
              :cursor hand
              :focusable true
              :role tab
              :selected "{{state.devTab == tab}}"
              :onClick {:set state.devTab :to "{{tab}}"}
              {:when "{{state.devTab == tab}}"
                :anchors {:left 0 :right 0 :bottom 0}
                :height 2
                :color "{{computed.devInBar ? '@onHeader' : '@accent'}}"}))
          (node #devTabsRule
            :when "{{state.settingsView == 'devices' && !computed.devInBar}}"
            :anchors {:left 0 :right 0 :top 104}
            :height 1
            :color @divider)
          (node #devFilters
            :when "{{state.settingsView == 'devices'}}"
            :style button
            :icon filter
            :text " {{t('dev.filters')}}"
            :color transparent
            :hover {:color transparent}
            :textColor @muted
            :borderWidth 1
            :borderColor @border
            :anchors {:left 24 :top "{{computed.devTop + 16}}"}
            :width 112
            :height 40
            :radius 8
            :tooltip "{{t('dev.filtersSoon')}}"
            :cursor arrow
            :disabled true)
          (node #devSearchBox
            :when "{{state.settingsView == 'devices'}}"
            :anchors {:left 148 :right 24 :top "{{computed.devTop + 16}}"}
            :height 40
            :radius 8
            :color @card
            :borderWidth 1
            :borderColor @border
            {:type label
              :icon search
              :text ""
              :textColor @muted
              :anchors {:left 10 :top 0 :bottom 0}
              :width 24
              :align center}
            (node #devSearch
              :type textarea
              :bind state.devQuery
              :placeholder "{{t('dev.searchN.' + state.devTab, { n: length(state.devTab == 'entities' ? state.rooms : state.devices) })}}"
              :placeholderColor @muted
              :textColor @text
              :color transparent
              :anchors {:left 42 :right 12 :top 0 :bottom 0}
              :label "{{t('dev.search')}}"))
          (node #devEmpty
            :when "{{state.settingsView == 'devices' && (state.devTab == 'integrations' || state.devTab == 'helpers')}}"
            :style card
            :anchors {:left 24 :right 24 :top "{{computed.devTop + 80}}"}
            :height 160
            {:type label
              :font iconsLarge
              :icon "{{state.devTab == 'helpers' ? 'wand-2' : 'puzzle'}}"
              :text ""
              :textColor @muted
              :anchors {:left 0 :right 0 :top 32}
              :height 48
              :align center}
            {:type label
              :text "{{t('dev.empty.' + state.devTab)}}"
              :textColor @muted
              :anchors {:left 24 :right 24 :top 96}
              :height 24
              :align center})
          (node #devAdd
            :when "{{state.settingsView == 'devices'}}"
            :style button
            :icon plus
            :text " {{t('dev.add.' + state.devTab)}}"
            :color @raised
            :hover {:color @raised}
            :textColor @muted
            :anchors {:right 32 :bottom 32}
            :width 200
            :height 52
            :radius 16
            :shadow {:y 4 :blur 12 :color "#00000040"}
            :role button
            :label "{{t('dev.add.' + state.devTab)}}"
            :tooltip "{{t('dev.addSoon')}}"
            :cursor arrow
            :disabled true)
          (node #settingsDevices
            :when "{{state.settingsView == 'devices' && (state.devTab == 'devices' || state.devTab == 'entities')}}"
            :type column
            :anchors {:left 0 :right 0 :top "{{computed.devTop + 72}}" :bottom 0}
            :gap 0
            :scroll vertical
            (node #deviceTableCard
              :when "{{state.devTab == 'devices'}}"
              :anchors {:left 0 :right 0}
              :height "{{length(computed.deviceRows) * 52 + 29}}"
              :color @card
              (node #deviceTable
                :type table
                :anchors {:left 12 :right 12 :top 0 :bottom 0}
                :rowHeight 52
                :color @card
                :items "{{computed.deviceRows}}"
                :columns "{{computed.deviceColumns}}"
                :label "{{t('devices.title')}}"
                :textColor @text
                :selectedTextColor @text
                :selectedBackground @selected
                :accentColor @border)
              {:anchors {:left 0 :right 0 :top 28} :height 1 :color @divider}
              (node #deviceTableLine
                :repeat {:for row :in computed.deviceRows}
                :anchors {:left 0 :right 0 :top "{{80 + index * 52}}"}
                :height 1
                :color @divider))
            (node #roomTableCard
              :when "{{state.devTab == 'entities'}}"
              :anchors {:left 0 :right 0}
              :height "{{length(computed.roomEntityRows) * 52 + 29}}"
              :color @card
              (node #roomEntityTable
                :type table
                :anchors {:left 12 :right 12 :top 0 :bottom 0}
                :rowHeight 52
                :color @card
                :items "{{computed.roomEntityRows}}"
                :columns "{{computed.roomEntityColumns}}"
                :label "{{t('set.entities')}}"
                :textColor @text
                :selectedTextColor @text
                :selectedBackground @selected
                :accentColor @border
                :onOpen [{:run pickRoom :with (node #"{{event.value}}")} {:set state.page :to home}])
              {:anchors {:left 0 :right 0 :top 28} :height 1 :color @divider}
              (node #roomEntityTableLine
                :repeat {:for row :in computed.roomEntityRows}
                :anchors {:left 0 :right 0 :top "{{80 + index * 52}}"}
                :height 1
                :color @divider)))
          (node #settingsUser
            :when "{{state.settingsView == 'profile'}}"
            :style card
            :anchors {:left 24 :right 24 :top 80}
            :height 88
            :role group
            :label Alex
            {:type label
              :font heading
              :text A
              :textColor @onAccent
              :color @accent
              :anchors {:left 20 :top 20}
              :width 48
              :height 48
              :radius 24
              :align center}
            {:type label :font heading :text Alex :textColor @text :anchors {:left 84 :top 22} :height 24 :fitText true}
            {:type label
              :font small
              :text "{{t('profile.owner')}}"
              :textColor @muted
              :anchors {:left 84 :top 48}
              :height 18
              :fitText true}
            (node #profileLogout
              :style button
              :color transparent
              :hover {:color transparent}
              :textColor @muted
              :text "{{t('profile.logout')}}"
              :anchors {:right 16 :top 26}
              :width 104
              :height 36
              :radius 18
              :tooltip "{{t('profile.logoutSoon')}}"
              :cursor arrow
              :disabled true))
          (node #settingsProfile
            :when "{{state.settingsView == 'profile'}}"
            :style card
            :anchors {:left 24 :right 24 :top 184}
            :height 168
            {:type column
              :anchors {:left 0 :right 0 :top 8 :bottom 8}
              :gap 0
              :children [(node :use settingsSection
                  #profileLanguage
                  :with {:title "{{t('set.language')}}" :sub "{{t('set.language.sub')}}"}
                  :role group
                  (node #pickLanguage
                    :type label
                    :text "{{t('set.value.' + settings.ui.language)}}"
                    :textColor @text
                    :width 240
                    :height 36
                    :padding 12
                    :color @raised
                    :radius {:topLeft 4 :topRight 4}
                    :align {:horizontal start :vertical center}
                    :hover {:color @raisedHover}
                    :cursor hand
                    :focusable true
                    :role combobox
                    :label "{{t('set.language')}}"
                    :isOpen "{{state.langMenu}}"
                    :onClick {:toggle state.langMenu}
                    {:type label
                      :icon chevron-down
                      :text ""
                      :textColor @muted
                      :anchors {:right 8 :top 6}
                      :width 24
                      :height 24
                      :align center}
                    {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted}
                    (node #langMenu
                      :use menu
                      :with {:open state.langMenu
                        :placement below
                        :width 240
                        :items [{:title English
                            :checked "{{settings.ui.language == 'en'}}"
                            :run pickLanguage
                            :with {:lang en}}
                          {:title "Español"
                            :checked "{{settings.ui.language == 'es'}}"
                            :run pickLanguage
                            :with {:lang es}}
                          {:title "Français"
                            :checked "{{settings.ui.language == 'fr'}}"
                            :run pickLanguage
                            :with {:lang fr}}]})))
                (node :use settingsSection
                  #profileTheme
                  :with {:title "{{t('set.theme')}}" :sub "{{t('set.theme.sub')}}"}
                  (node :use settingsRadio
                    #pickAuto
                    :with {:text "{{t('set.auto')}}" :chosen "{{state.theme == 'auto'}}"}
                    :onClick {:set state.theme :to auto})
                  (node :use settingsRadio
                    #pickLight
                    :with {:text "{{t('set.light')}}" :chosen "{{state.theme == 'light'}}"}
                    :onClick {:set state.theme :to light})
                  (node :use settingsRadio
                    #pickDark
                    :with {:text "{{t('set.dark')}}" :chosen "{{state.theme == 'dark'}}"}
                    :onClick {:set state.theme :to dark}))]})
          (node #settingsGeneral
            :when "{{state.settingsView == 'general'}}"
            :anchors {:top 80}
            :x "{{max(24, (window.width - computed.sideWidth - 1040) / 2)}}"
            :width "{{min(1040, window.width - computed.sideWidth - 48)}}"
            :height "{{length(settingsList) * 72 + 16}}"
            :style card
            {:type column
              :anchors {:left 0 :right 0 :top 8 :bottom 8}
              :gap 0
              :children [(node #generalRow
                  :repeat {:for s :in "{{filter(settingsList, true)}}" :index i}
                  :anchors {:left 0 :right 0}
                  :height 72
                  :color transparent
                  :role group
                  :label "{{s.title}}"
                  {:when "{{i > 0}}" :anchors {:left 20 :right 20 :top 0} :height 1 :color @divider}
                  {:type label
                    :text "{{s.title}}"
                    :textColor @text
                    :anchors {:left 20 :top 14}
                    :height 22
                    :fitText true}
                  {:type label
                    :font small
                    :text "{{s.description}}"
                    :textColor @muted
                    :anchors {:left 20 :right 420 :top 38}
                    :height 20}
                  {:when "{{s.modified}}"
                    :type label
                    :font small
                    :text "{{t('set.reset')}}"
                    :textColor @accent
                    :anchors {:right 176 :top 26}
                    :fitText true
                    :cursor hand
                    :focusable true
                    :role button
                    :label "{{t('set.reset')}} {{s.title}}"
                    :onClick {:remove "{{'settings.' + s.key}}"}}
                  (node :when "{{s.type == 'boolean'}}"
                    :use pill
                    :with {:at "{{'settings.' + s.key}}" :on "{{s.value}}" :name "{{s.title}}"}
                    #generalSwitch
                    :anchors {:right 20 :top 24})
                  (node :when "{{s.type == 'number' || s.type == 'integer'}}"
                    #generalField
                    :type textarea
                    :text "{{text(s.value)}}"
                    :anchors {:right 20 :top 14}
                    :width 140
                    :height 44
                    :padding 12
                    :radius {:topLeft 4 :topRight 4}
                    :color @raised
                    :hover {:color @raisedHover}
                    :textColor @text
                    :label "{{s.title}}"
                    :onChange {:if "{{number(event.value) != null && number(event.value) >= s.minimum && number(event.value) <= s.maximum}}"
                      :then [{:set "{{'settings.' + s.key}}" :to "{{number(event.value)}}"}]})
                  {:when "{{s.type == 'number' || s.type == 'integer'}}"
                    :anchors {:right 20 :top 57}
                    :width 140
                    :height 1
                    :color @muted}
                  (node :when "{{s.type == 'string' && s.enum != null}}"
                    #generalPick
                    :type label
                    :text "{{t('set.value.' + s.value)}}"
                    :textColor @text
                    :anchors {:right 20 :top 14}
                    :width 240
                    :height 44
                    :padding 12
                    :color @raised
                    :radius {:topLeft 4 :topRight 4}
                    :align {:horizontal start :vertical center}
                    :hover {:color @raisedHover}
                    :cursor hand
                    :focusable true
                    :role combobox
                    :label "{{s.title}}"
                    :isOpen "{{state.genMenu}}"
                    :onClick {:toggle state.genMenu}
                    {:type label
                      :icon chevron-down
                      :text ""
                      :textColor @muted
                      :anchors {:right 8 :top 10}
                      :width 24
                      :height 24
                      :align center}
                    {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted}
                    {:use menu
                      :with {:open state.genMenu
                        :placement below
                        :width 240
                        :items "{{map(s.enum, { title: t('set.value.' + it), checked: s.value == it, run: 'pickSetting', with: { key: s.key, value: it } })}}"}}))]})
          (node #settingsAbout
            :when "{{state.settingsView == 'about'}}"
            :style card
            :anchors {:left 24 :right 24 :top 80}
            :height 468
            {:type row
              :anchors {:left 0 :right 0 :top 32}
              :height 72
              :justify center
              :children [{:color @accent
                  :width 72
                  :height 72
                  :radius 36
                  :children [{:type label
                      :font iconsLarge
                      :icon home
                      :text ""
                      :textColor @onAccent
                      :anchors {:left 0 :right 0 :top 0 :bottom 0}
                      :align center}]}]}
            {:type label
              :text Haven
              :font title
              :textColor @text
              :anchors {:left 0 :right 0 :top 120}
              :height 36
              :align center}
            {:type label
              :text "{{t('app.tagline')}}"
              :textColor @muted
              :anchors {:left 0 :right 0 :top 156}
              :height 22
              :align center}
            {:type panel :color @border :anchors {:left 0 :right 0 :top 198} :height 1}
            {:type column
              :anchors {:left 20 :right 20 :top 206}
              :height 84
              :gap 0
              :children [{:anchors {:left 0 :right 0}
                  :height 28
                  :color transparent
                  :children [{:type label
                      :text "{{t('about.version')}}"
                      :textColor @muted
                      :anchors {:left 0 :top 0 :bottom 0}
                      :fitText true}
                    {:type label :text "2026.10" :textColor @text :anchors {:right 0 :top 0 :bottom 0} :fitText true}]}
                {:anchors {:left 0 :right 0}
                  :height 28
                  :color transparent
                  :children [{:type label
                      :text "{{t('about.devices')}}"
                      :textColor @muted
                      :anchors {:left 0 :top 0 :bottom 0}
                      :fitText true}
                    {:type label
                      :text "{{length(state.devices) + length(state.rooms)}}"
                      :textColor @text
                      :anchors {:right 0 :top 0 :bottom 0}
                      :fitText true}]}
                {:anchors {:left 0 :right 0}
                  :height 28
                  :color transparent
                  :children [{:type label
                      :text "{{t('about.license')}}"
                      :textColor @muted
                      :anchors {:left 0 :top 0 :bottom 0}
                      :fitText true}
                    {:type label :text Apache-2.0 :textColor @text :anchors {:right 0 :top 0 :bottom 0} :fitText true}]}]}
            {:type panel :color @border :anchors {:left 0 :right 0 :top 300} :height 1}
            {:type column
              :anchors {:left 0 :right 0 :top 304 :bottom 8}
              :gap 0
              :role list
              :children [(node #aboutLink
                  :repeat {:for l
                    :in "{{[{ key: 'docs', icon: 'book-open' }, { key: 'issue', icon: 'bug' }, { key: 'source', icon: 'code' }]}}"}
                  :anchors {:left 0 :right 0}
                  :height 52
                  :color transparent
                  :role link
                  :label "{{t('about.' + l.key)}}"
                  :tooltip "{{t('about.offline')}}"
                  :cursor arrow
                  :disabled true
                  {:type label
                    :icon "{{l.icon}}"
                    :text ""
                    :textColor @muted
                    :anchors {:left 20 :top 14}
                    :width 24
                    :height 24
                    :align center}
                  {:type label
                    :text "{{t('about.' + l.key)}}"
                    :textColor @text
                    :anchors {:left 64 :right 56 :top 0 :bottom 0}
                    :align {:horizontal start :vertical center}}
                  {:type label
                    :icon external-link
                    :text ""
                    :textColor @muted
                    :anchors {:right 20 :top 14}
                    :width 24
                    :height 24
                    :align center})]})]}}}}
