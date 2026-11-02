{:$schema "../../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :note "Arcadia's game management: a game's menu from the library (right click, Shift+F10) or the game page's gear, its properties (launch options, overlay, cloud saves, updates, installed files) kept on the game's record, and uninstalling with a question first."
 :state {:manage nil
         :gameMenu {:open false :x 0 :y 0}
         :properties {:open false :tab general :verify ""}
         :uninstall {:open false}}
 :computed {:manageGame "{{find(data.games, it.id == state.manage)}}"
            :gameMenuItems "{{computed.manageGame == null ? [] : [ { title: t(computed.manageGame.status == 'ready' ? 'menu.game.play' : computed.manageGame.status == 'running' ? 'menu.game.stop' : computed.manageGame.status == 'update' ? 'menu.game.update' : computed.manageGame.status == 'downloading' ? 'menu.game.downloading' : 'menu.game.install'), enabled: computed.manageGame.status != 'downloading', run: 'primary', with: { game: computed.manageGame } }, { separator: true }, { title: t(computed.manageGame.favourite ? 'menu.game.unfavourite' : 'menu.game.favourite'), run: 'toggleFavourite', with: { id: computed.manageGame.id } }, { title: t('menu.game.page'), run: 'openGame', with: { id: computed.manageGame.id } }, { title: t('menu.game.properties'), run: 'openProperties', with: { id: computed.manageGame.id, tab: 'general' } }, { separator: true }, { title: t('menu.game.uninstall'), enabled: computed.manageGame.installed && computed.manageGame.status != 'running', run: 'askUninstall', with: { id: computed.manageGame.id } } ]}}"}
 :components {:gameMenu {:note "A game's menu (right click, Shift+F10 or the gear): play or install, favourite, properties, uninstall."
                         :params {}
                         :node (node#game-menu :use contextMenu
                                 :with {:open state.gameMenu.open
                                        :width 230
                                        :x "{{min(state.gameMenu.x, window.width - 238)}}"
                                        :y "{{min(state.gameMenu.y, window.height - 190)}}"
                                        :color @color.raised
                                        :textColor @color.text
                                        :dimColor @color.dim
                                        :highlight @color.hover
                                        :items "{{computed.gameMenuItems}}"})}
              :propertiesDialog {:note "A game's properties: launch options and in-game settings, updates, installed files, extra content."
                                 :params {}
                                 :node (node :use dialog
                                         :with {:open state.properties.open
                                                :width 640
                                                :height 400
                                                :title "{{t('properties.title', { game: computed.manageGame.title ?? '' })}}"
                                                :color @color.panel
                                                :textColor @color.text
                                                :dimColor @color.muted
                                                :highlight @color.accent
                                                :buttons [{:title "{{t('dialog.close')}}" :default true}]}
                                         (row#properties-body :anchors {:left 20 :right 20 :top 52 :bottom 60} :gap 20
                                           (column#properties-tabs :width 150
                                             :alignSelf stretch
                                             :gap 2
                                             :role tablist
                                             :arrows vertical
                                             (label :id "{{'properties-tab-' + tab}}"
                                               :repeat {:for tab :in [general updates files dlc]}
                                               :anchors {:left 0 :right 0}
                                               :height 30
                                               :padding 10
                                               :radius 3
                                               :role tab
                                               :focusable true
                                               :cursor hand
                                               :selected "{{state.properties.tab == tab}}"
                                               :text "{{t('properties.tab.' + tab)}}"
                                               :color "{{state.properties.tab == tab ? '@color.select' : 'transparent'}}"
                                               :textColor "{{state.properties.tab == tab ? '@color.bright' : '@color.muted'}}"
                                               :hover {:textColor @color.bright}
                                               :onClick {:set state.properties.tab :to "{{tab}}"}))
                                           (panel :width 1 :alignSelf stretch :color @color.border)
                                           (node#properties-page :grow 1 :alignSelf stretch :color transparent
                                             (node :when "{{state.properties.tab == 'general'}}"
                                               :fill true
                                               :color transparent
                                               (label :font caps
                                                 :text "{{upper(t('properties.launch'))}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 0}
                                                 :height 18)
                                               (textarea#properties-launch :anchors {:left 0 :right 0 :top 24}
                                                 :height 32
                                                 :padding 8
                                                 :radius 3
                                                 :borderWidth 1
                                                 :borderColor @color.border
                                                 :focused {:borderColor @color.accent}
                                                 :color @color.field
                                                 :textColor @color.text
                                                 :placeholderColor @color.dim
                                                 :font small
                                                 :maxLength 200
                                                 :placeholder "{{t('properties.launchHint')}}"
                                                 :label "{{t('properties.launch')}}"
                                                 :text "{{computed.manageGame.launchOptions ?? ''}}"
                                                 :onChange {:update games
                                                            :id "{{computed.manageGame.id}}"
                                                            :fields {:launchOptions "{{event.value}}"}})
                                               (label :font small
                                                 :text "{{t('properties.launchNote')}}"
                                                 :textColor @color.dim
                                                 :anchors {:left 0 :right 0 :top 62}
                                                 :height 18)
                                               (label :font caps
                                                 :text "{{upper(t('properties.inGame'))}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 100}
                                                 :height 18)
                                               (row#properties-overlay :anchors {:left 0 :right 0 :top 124}
                                                 :height 30
                                                 :alignItems center
                                                 (label :grow 1
                                                   :height 22
                                                   :text "{{t('properties.overlay')}}"
                                                   :textColor @color.text)
                                                 (toggle#properties-overlay-toggle :width 40
                                                   :height 22
                                                   :label "{{t('properties.overlay')}}"
                                                   :checked "{{computed.manageGame.overlay != false}}"
                                                   :activeColor @color.accent
                                                   :onChange {:update games
                                                              :id "{{computed.manageGame.id}}"
                                                              :fields {:overlay "{{event.value}}"}}))
                                               (row#properties-cloud :anchors {:left 0 :right 0 :top 156}
                                                 :height 30
                                                 :alignItems center
                                                 (label :grow 1
                                                   :height 22
                                                   :text "{{t('properties.cloud')}}"
                                                   :textColor @color.text)
                                                 (toggle#properties-cloud-toggle :width 40
                                                   :height 22
                                                   :label "{{t('properties.cloud')}}"
                                                   :checked "{{computed.manageGame.cloud != false}}"
                                                   :activeColor @color.accent
                                                   :onChange {:update games
                                                              :id "{{computed.manageGame.id}}"
                                                              :fields {:cloud "{{event.value}}"}})))
                                             (node :when "{{state.properties.tab == 'updates'}}"
                                               :fill true
                                               :color transparent
                                               (label :font caps
                                                 :text "{{upper(t('properties.automatic'))}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 0}
                                                 :height 18)
                                               (row#properties-updates-auto :anchors {:left 0 :right 0 :top 24}
                                                 :height 28
                                                 :gap 10
                                                 :alignItems center
                                                 :focusable true
                                                 :role radio
                                                 :selected "{{(computed.manageGame.updates ?? 'auto') == 'auto'}}"
                                                 :cursor hand
                                                 :label "{{t('properties.updates.auto')}}"
                                                 :radius 3
                                                 :hover {:color @color.hover}
                                                 :onClick {:update games
                                                           :id "{{computed.manageGame.id}}"
                                                           :fields {:updates auto}}
                                                 (panel :color transparent :width 2 :height 1)
                                                 (panel :width 14
                                                   :height 14
                                                   :radius 7
                                                   :borderWidth 2
                                                   :borderColor "{{(computed.manageGame.updates ?? 'auto') == 'auto' ? '@color.accent' : '@color.dim'}}"
                                                   :color transparent
                                                   (node :when "{{(computed.manageGame.updates ?? 'auto') == 'auto'}}"
                                                     :anchors {:left 3 :top 3}
                                                     :width 4
                                                     :height 4
                                                     :radius 2
                                                     :color @color.accent))
                                                 (label :grow 1
                                                   :height 22
                                                   :text "{{t('properties.updates.auto')}}"
                                                   :textColor @color.text))
                                               (row#properties-updates-launch :anchors {:left 0 :right 0 :top 56}
                                                 :height 28
                                                 :gap 10
                                                 :alignItems center
                                                 :focusable true
                                                 :role radio
                                                 :selected "{{(computed.manageGame.updates ?? 'auto') == 'launch'}}"
                                                 :cursor hand
                                                 :label "{{t('properties.updates.launch')}}"
                                                 :radius 3
                                                 :hover {:color @color.hover}
                                                 :onClick {:update games
                                                           :id "{{computed.manageGame.id}}"
                                                           :fields {:updates launch}}
                                                 (panel :color transparent :width 2 :height 1)
                                                 (panel :width 14
                                                   :height 14
                                                   :radius 7
                                                   :borderWidth 2
                                                   :borderColor "{{(computed.manageGame.updates ?? 'auto') == 'launch' ? '@color.accent' : '@color.dim'}}"
                                                   :color transparent
                                                   (node :when "{{(computed.manageGame.updates ?? 'auto') == 'launch'}}"
                                                     :anchors {:left 3 :top 3}
                                                     :width 4
                                                     :height 4
                                                     :radius 2
                                                     :color @color.accent))
                                                 (label :grow 1
                                                   :height 22
                                                   :text "{{t('properties.updates.launch')}}"
                                                   :textColor @color.text))
                                               (row#properties-updates-high :anchors {:left 0 :right 0 :top 88}
                                                 :height 28
                                                 :gap 10
                                                 :alignItems center
                                                 :focusable true
                                                 :role radio
                                                 :selected "{{(computed.manageGame.updates ?? 'auto') == 'high'}}"
                                                 :cursor hand
                                                 :label "{{t('properties.updates.high')}}"
                                                 :radius 3
                                                 :hover {:color @color.hover}
                                                 :onClick {:update games
                                                           :id "{{computed.manageGame.id}}"
                                                           :fields {:updates high}}
                                                 (panel :color transparent :width 2 :height 1)
                                                 (panel :width 14
                                                   :height 14
                                                   :radius 7
                                                   :borderWidth 2
                                                   :borderColor "{{(computed.manageGame.updates ?? 'auto') == 'high' ? '@color.accent' : '@color.dim'}}"
                                                   :color transparent
                                                   (node :when "{{(computed.manageGame.updates ?? 'auto') == 'high'}}"
                                                     :anchors {:left 3 :top 3}
                                                     :width 4
                                                     :height 4
                                                     :radius 2
                                                     :color @color.accent))
                                                 (label :grow 1
                                                   :height 22
                                                   :text "{{t('properties.updates.high')}}"
                                                   :textColor @color.text))
                                               (label :font small
                                                 :text "{{t('properties.updatesNote')}}"
                                                 :textColor @color.dim
                                                 :wrap true
                                                 :anchors {:left 0 :right 0 :top 128}
                                                 :height 40))
                                             (node :when "{{state.properties.tab == 'files'}}"
                                               :fill true
                                               :color transparent
                                               (label :font caps
                                                 :text "{{upper(t('properties.size'))}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 0}
                                                 :height 18)
                                               (label#properties-size :text "{{t('properties.sizeValue', { size: formatNumber(computed.manageGame.sizeGb ?? 0) })}}"
                                                 :textColor @color.text
                                                 :font default
                                                 :anchors {:left 0 :right 0 :top 24}
                                                 :height 22)
                                               (label :font caps
                                                 :text "{{upper(t('properties.folder'))}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 60}
                                                 :height 18)
                                               (label#properties-folder :text "{{'Arcadia/games/' + computed.manageGame.title}}"
                                                 :textColor @color.text
                                                 :font small
                                                 :anchors {:left 0 :right 0 :top 84}
                                                 :height 22)
                                               (node#properties-verify :use quietButton
                                                 :anchors {:left 0 :top 128}
                                                 :height 32
                                                 :with {:title "{{t('properties.verify')}}" :icon shield-check}
                                                 :disabled "{{state.properties.verify == 'running'}}"
                                                 :onClick [{:set state.properties.verify :to running}
                                                           {:after 1500 :do [{:set state.properties.verify :to done}]}])
                                               (label#properties-verify-result :live polite
                                                 :font small
                                                 :anchors {:left 0 :right 0 :top 168}
                                                 :height 20
                                                 :text "{{state.properties.verify == 'running' ? t('properties.verifying') : state.properties.verify == 'done' ? t('properties.verified') : ''}}"
                                                 :textColor "{{state.properties.verify == 'done' ? '@color.play' : '@color.muted'}}"))
                                             (node :when "{{state.properties.tab == 'dlc'}}"
                                               :fill true
                                               :color transparent
                                               (label :font caps
                                                 :text "{{upper(t('properties.dlcTitle'))}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 0}
                                                 :height 18)
                                               (label#properties-dlc-empty :text "{{t('properties.dlcNone')}}"
                                                 :textColor @color.muted
                                                 :anchors {:left 0 :right 0 :top 24}
                                                 :height 22)))))}
              :uninstallDialog {:note "The question before a game's files are removed; the game stays in the library."
                                :params {}
                                :node (node :use dialog
                                        :with {:open state.uninstall.open
                                               :width 440
                                               :height 170
                                               :title "{{t('uninstall.title', { game: computed.manageGame.title ?? '' })}}"
                                               :message "{{t('uninstall.message', { game: computed.manageGame.title ?? '', size: formatNumber(computed.manageGame.sizeGb ?? 0) })}}"
                                               :color @color.panel
                                               :textColor @color.text
                                               :dimColor @color.muted
                                               :highlight @color.accent
                                               :buttons [{:title "{{t('uninstall.confirm')}}"
                                                          :run uninstall
                                                          :with {:id "{{state.manage}}"}
                                                          :default true}
                                                         {:title "{{t('dialog.cancel')}}"}]})}}}
