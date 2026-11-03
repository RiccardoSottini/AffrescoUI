{:$schema "../../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :note "Arcadia's library: the game list on the left (home, a filter, search, collections that fold, each game in its play-state colour), the home (what's new, recent games by day, a shelf per collection and every game sorted) and a page per game (the banner with its logo, play and the stats, the sections, the activity and the side panels)."
 :state {:selected starfall
         :query ""
         :readyOnly false
         :recentFirst false
         :favOnly false
         :filterOpen false
         :newsOpen false
         :showNews true
         :note {:open false :id nil :text ""}
         :collapsed {}
         :running nil
         :playOpen false
         :sortOpen false
         :infoOpen false
         :post ""
         :collections [{:id coop :title "Co-op Nights" :games [ember pebble neon]}
                       {:id cozy :title "Cozy Sundays" :games [tidebreaker mossheart driftwood lumen]}]}
 :computed {:libList "{{sort(filter(computed.owned, (!state.readyOnly || it.installed) && (!state.favOnly || it.favourite) && (empty(state.query) || fuzzy(state.query, it.title) != null)), state.recentFirst ? it.lastPlayed ?? 0 : lower(it.title), state.recentFirst ? 'desc' : 'asc')}}"
            :collected "{{split(join(map(state.collections, join(it.games, ',')), ','), ',')}}"
            :sections "{{concat([{ id: 'favourites', title: t('lib.favourites'), ids: map(filter(computed.owned, it.favourite), it.id) }], state.collections, [{ id: 'uncategorised', title: t('lib.uncategorised'), ids: map(filter(computed.owned, !it.favourite && !contains(computed.collected, it.id)), it.id) }])}}"
            :recent "{{slice(sort(filter(computed.owned, it.lastPlayed != null), it.lastPlayed, 'desc'), 0, 10)}}"
            :recentGroups "{{filter([{ label: t('time.today'), games: filter(computed.recent, state.today - it.lastPlayed < 86400000) }, { label: t('time.yesterday'), games: filter(computed.recent, state.today - it.lastPlayed >= 86400000 && state.today - it.lastPlayed < 172800000) }, { label: t('time.thisWeek'), games: filter(computed.recent, state.today - it.lastPlayed >= 172800000 && state.today - it.lastPlayed < 604800000) }, { label: t('time.lastWeek'), games: filter(computed.recent, state.today - it.lastPlayed >= 604800000 && state.today - it.lastPlayed < 1209600000) }, { label: t('time.earlier'), games: filter(computed.recent, state.today - it.lastPlayed >= 1209600000) }], length(it.games) > 0)}}"
            :allGames "{{settings.library.sortBy == 'playtime' ? sort(computed.owned, it.hours, 'desc') : settings.library.sortBy == 'recent' ? sort(computed.owned, it.lastPlayed ?? 0, 'desc') : sort(computed.owned, lower(it.title))}}"
            :lastPlayed "{{computed.game.lastPlayed == null ? t('time.never') : state.today - computed.game.lastPlayed < 86400000 ? t('time.today') : state.today - computed.game.lastPlayed < 172800000 ? t('time.yesterday') : formatDate(computed.game.lastPlayed, 'd MMM', 'utc')}}"}
 :styles {:caps (label :font caps :textColor @color.muted :fitText true)
          :iconButton (label :text ""
                        :fitText true
                        :padding 7
                        :radius 3
                        :cursor hand
                        :focusable true
                        :role button
                        :textColor @color.muted
                        :hover {:color @color.hover :textColor @color.bright})
          :sectionLink (label :fitText true
                         :padding 10
                         :cursor hand
                         :focusable true
                         :role link
                         :textColor @color.text
                         :hover {:textColor @color.bright})
          :card {:color @color.panel :radius 12}}
 :components {:noteDialog {:note "A note about a game, kept with it in the library."
                           :params {}
                           :node (node :use dialog
                                   :with {:open state.note.open
                                          :width 460
                                          :height 240
                                          :title "{{t('note.title')}}"
                                          :message "{{t('note.help')}}"
                                          :buttons [{:title "{{t('dialog.save')}}" :run saveNote :default true}
                                                    {:title "{{t('dialog.cancel')}}"}]}
                                   (textarea#note-text :wrap true
                                     :anchors {:left 20 :right 20 :top 76}
                                     :height 96
                                     :padding 8
                                     :borderWidth 1
                                     :borderColor @color.border
                                     :focused {:borderColor @color.accent}
                                     :bind state.note.text
                                     :maxLength 500
                                     :radius 3
                                     :color @color.field
                                     :textColor @color.text
                                     :label "{{t('note.title')}}"
                                     :autofocus true))}
              :libraryPage {:note "The library: the game list on the left, the home or a game's page on the right."
                            :params {}
                            :node (row :fill true
                                    (node#library-sidebar :use librarySidebar)
                                    (node#library-main :grow 1
                                      :anchors {:top 0 :bottom 0}
                                      :gradient {:kind linear :stops [@color.main @color.page]}
                                      (node#library-home :when "{{computed.here.page == 'home'}}" :use libraryHome)
                                      (node#game-page :when "{{computed.here.page == 'game' && computed.game != null}}"
                                        :use gamePage)
                                      (node#hub-page :when "{{computed.here.page == 'hub' && computed.game != null}}"
                                        :use hubPage)))}
              :librarySidebar {:note "Home, the games filter with recent-first and ready-to-play, search, and the games by collection."
                               :params {}
                               :node (column :width 272 :anchors {:top 0 :bottom 0} :color @color.side
                                       (row :height 40
                                         :anchors {:left 0 :right 0}
                                         :color @color.sideTop
                                         :alignItems center
                                         (label#library-home-button :grow 1
                                           :height 40
                                           :font strong
                                           :icon layout-dashboard
                                           :text "{{t('lib.home')}}"
                                           :padding 14
                                           :cursor hand
                                           :focusable true
                                           :role button
                                           :textColor "{{computed.here.page == 'home' ? '@color.bright' : '@color.text'}}"
                                           :color "{{computed.here.page == 'home' ? '@color.hover' : 'transparent'}}"
                                           :hover {:color @color.hover}
                                           :onClick libraryHome)
                                         (node#library-collections :style iconButton
                                           :icon "{{some(computed.sections, !(state.collapsed[it.id] ?? false)) ? 'list-collapse' : 'list'}}"
                                           :tooltip "{{t(some(computed.sections, !(state.collapsed[it.id] ?? false)) ? 'lib.collapseAll' : 'lib.expandAll')}}"
                                           :onClick toggleCollections))
                                       (row :height 36 :anchors {:left 0 :right 0} :alignItems center
                                         (label#library-kind :font caps
                                           :text "{{t('lib.games')}}"
                                           :icon chevron-down
                                           :textColor @color.muted
                                           :grow 1
                                           :padding 14
                                           :height 36)
                                         (node#library-recent :style iconButton
                                           :icon clock-3
                                           :tooltip "{{t('lib.recentFirst')}}"
                                           :textColor "{{state.recentFirst ? '@color.accent' : '@color.muted'}}"
                                           :selected "{{state.recentFirst}}"
                                           :onClick {:toggle state.recentFirst})
                                         (node#library-ready :style iconButton
                                           :icon circle-play
                                           :tooltip "{{t('lib.readyOnly')}}"
                                           :textColor "{{state.readyOnly ? '@color.accent' : '@color.muted'}}"
                                           :selected "{{state.readyOnly}}"
                                           :onClick {:toggle state.readyOnly})
                                         (panel :width 6 :height 1 :color transparent))
                                       (row :height 34 :anchors {:left 0 :right 0} :alignItems center
                                         (panel :width 10 :height 1 :color transparent)
                                         (textarea#library-search :grow 1
                                           :height 28
                                           :padding 8
                                           :bind state.query
                                           :placeholder "{{t('lib.search')}}"
                                           :placeholderColor @color.dim
                                           :textColor @color.text
                                           :color @color.field
                                           :radius 3
                                           :label "{{t('lib.search')}}"
                                           :focused {:borderWidth 1 :borderColor @color.accent}
                                           (label :icon search
                                             :text ""
                                             :textColor @color.dim
                                             :fitText true
                                             :anchors {:right 8 :top 6}))
                                         (node#library-filter :style iconButton
                                           :icon sliders-horizontal
                                           :tooltip "{{t('lib.advanced')}}"
                                           :textColor "{{state.readyOnly || state.recentFirst || state.favOnly ? '@color.accent' : '@color.muted'}}"
                                           :isOpen "{{state.filterOpen}}"
                                           :onClick {:toggle state.filterOpen}
                                           (node :use menu
                                             :with {:open state.filterOpen
                                                    :width 230
                                                    :items [{:title "{{t('lib.readyOnly')}}"
                                                             :checked "{{state.readyOnly}}"
                                                             :run toggleFilter
                                                             :with {:key readyOnly}}
                                                            {:title "{{t('lib.favOnly')}}"
                                                             :checked "{{state.favOnly}}"
                                                             :run toggleFilter
                                                             :with {:key favOnly}}
                                                            {:title "{{t('lib.recentFirst')}}"
                                                             :checked "{{state.recentFirst}}"
                                                             :run toggleFilter
                                                             :with {:key recentFirst}}
                                                            {:separator true}
                                                            {:title "{{t('lib.clearFilters')}}" :run clearFilters}]}))
                                         (panel :width 6 :height 1 :color transparent))
                                       (column#library-list :grow 1
                                         :anchors {:left 0 :right 0}
                                         :scroll vertical
                                         :arrows vertical
                                         :role list
                                         :label "{{t('lib.games')}}"
                                         (column :repeat {:for section
                                                          :in "{{computed.sections}}"
                                                          :key "{{section.id}}"}
                                           :let {:games "{{filter(computed.libList, contains(section.ids ?? section.games, it.id))}}"
                                                 :open "{{!(state.collapsed[section.id] ?? false)}}"}
                                           :anchors {:left 0 :right 0}
                                           :height "{{30 + (open ? length(games) * 28 : 0)}}"
                                           :when "{{length(games) > 0 || empty(state.query)}}"
                                           (label :id "{{'section-' + section.id}}"
                                             :font caps
                                             :height 30
                                             :anchors {:left 0 :right 0}
                                             :padding 12
                                             :icon "{{open ? 'chevron-down' : 'chevron-right'}}"
                                             :mirror true
                                             :text "{{upper(section.title)}} ({{length(games)}})"
                                             :textColor @color.muted
                                             :cursor hand
                                             :focusable true
                                             :role button
                                             :isOpen "{{open}}"
                                             :hover {:textColor @color.bright}
                                             :onClick {:set "{{'state.collapsed.' + section.id}}" :to "{{open}}"})
                                           (node :id "{{'row-' + section.id + '-' + g.id}}"
                                             :when "{{open}}"
                                             :repeat {:for g :in "{{games}}" :key "{{section.id + g.id}}"}
                                             :use gameRow
                                             :with {:game "{{g}}"}))))}
              :gameRow {:note "A game in the list: its cover as an icon and its name in its play-state colour, with what it is doing."
                        :params {:game {}}
                        :node (row :height 28
                                :anchors {:left 0 :right 0}
                                :gap 8
                                :alignItems center
                                :focusable true
                                :role listitem
                                :cursor hand
                                :label "{{game.title}}"
                                :selected "{{computed.here.page == 'game' && computed.here.id == game.id}}"
                                :color "{{computed.here.page == 'game' && computed.here.id == game.id ? '@color.select' : 'transparent'}}"
                                :hover {:color @color.hover}
                                :onClick {:run openGame :with {:id "{{game.id}}"}}
                                :onDoubleClick {:run primary :with {:game "{{game}}"}}
                                :onContextMenu {:run openGameMenu :with {:id "{{game.id}}"}}
                                (panel :width 18 :height 1 :color transparent)
                                (image :file "{{'../assets/art/' + (game.art ?? game.id) + '/cover.svg'}}"
                                  :width 18
                                  :height 18
                                  :fit cover
                                  :radius 2)
                                (label :grow 1
                                  :height 18
                                  :font small
                                  :runs [{:text "{{game.title}}"}
                                         {:text "{{game.status == 'running' ? ' – ' + t('status.running') : game.status == 'update' ? ' – ' + t('status.update') : game.status == 'downloading' ? ' – ' + t('status.downloading') : ''}}"}]
                                  :textColor "{{game.status == 'running' ? '@color.ingame' : game.status == 'update' || game.status == 'downloading' ? '@color.accent' : game.installed ? '@color.text' : '@color.dim'}}"))}
              :capsule {:note "A game's cover, 2:3, that lifts and glows under the pointer; dimmed when the game isn't installed."
                        :params {:game {} :width 150}
                        :node (node :width "{{width}}"
                                :height "{{width * 1.5}}"
                                :radius 12
                                :color @color.raised
                                :focusable true
                                :role button
                                :label "{{game.title}}"
                                :cursor hand
                                :tooltip "{{game.title}}"
                                :shadow {:y 2 :blur 6 :color "#00000060"}
                                :hover {:scale 1.04 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
                                :focused {:scale 1.04 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
                                :onClick {:run openGame :with {:id "{{game.id}}"}}
                                :onContextMenu {:run openGameMenu :with {:id "{{game.id}}"}}
                                (image :file "{{'../assets/art/' + (game.art ?? game.id) + '/cover.svg'}}"
                                  :fill true
                                  :fit cover
                                  :radius 12)
                                (node :when "{{!game.installed}}" :fill true :color "#100c1480" :radius 12))}
              :shelfHeader {:params {:title "" :count nil}
                            :node (row :height 34 :anchors {:left 0 :right 0} :alignItems center :gap 8
                                    (label :font caps :text "{{upper(title)}}" :textColor @color.text :fitText true)
                                    (label :when "{{count != null}}"
                                      :font caps
                                      :text "({{count}})"
                                      :textColor @color.muted
                                      :fitText true)
                                    (panel :grow 1 :height 1 :color transparent)
                                    (node :slot true))}
              :libraryHome {:note "The library's home: what's new, recent games grouped by day, a shelf per collection and every game."
                            :params {}
                            :node (column#home-scroll :fill true :scroll vertical
                                    (column#home-content :anchors {:left 28 :right 28}
                                      :gap 6
                                      :height "{{412 + 286 * length(filter(computed.sections, it.id != 'uncategorised' && length(it.ids ?? it.games) > 0)) + 239 * ceil(length(computed.owned) / max(1, floor(((nodes['home-scroll'].width ?? 1008) - 42) / 164)))}}"
                                      (panel :height 10 :width 1 :color transparent)
                                      (node#news-header :use shelfHeader :with {:title "{{t('lib.whatsNew')}}"}
                                        (node#news-settings :style iconButton
                                          :icon settings
                                          :tooltip "{{t('lib.whatsNewSettings')}}"
                                          :isOpen "{{state.newsOpen}}"
                                          :onClick {:toggle state.newsOpen}
                                          (node :use menu
                                            :with {:open state.newsOpen
                                                   :width 220
                                                   :placement below-end
                                                   :items [{:title "{{t('lib.showNews')}}"
                                                            :checked "{{state.showNews}}"
                                                            :run toggleFilter
                                                            :with {:key showNews}}]})))
                                      (row#news-shelf :when "{{state.showNews}}"
                                        :height 222
                                        :anchors {:left 0 :right 0}
                                        :gap 14
                                        :scroll horizontal
                                        :arrows horizontal
                                        (column :id "{{'news-' + i}}"
                                          :repeat {:for n :in "{{data.news}}" :index i}
                                          :let {:g "{{find(data.games, it.id == n.game)}}"}
                                          :width 232
                                          :height 216
                                          :style card
                                          :focusable true
                                          :role link
                                          :label "{{n.title}}"
                                          :cursor hand
                                          :hover {:color @color.hover :shadow {:y 4 :blur 14 :color "#00000080"}}
                                          :onClick {:run openGame :with {:id "{{n.game}}"}}
                                          (image :file "{{'../assets/art/' + n.game + '/hero.svg'}}"
                                            :anchors {:left 0 :right 0}
                                            :height 120
                                            :fit cover
                                            :radius {:topLeft 8 :topRight 8})
                                          (label :font caps
                                            :height 26
                                            :anchors {:left 0 :right 0}
                                            :padding 10
                                            :runs [{:text "{{upper(t('news.' + n.kind))}}" :color @color.link}
                                                   {:text " · {{t('time.daysAgo', { n: n.ago })}}" :color @color.muted}])
                                          (label :font strong
                                            :text "{{n.title}}"
                                            :textColor @color.bright
                                            :wrap true
                                            :anchors {:left 10 :right 10}
                                            :height 40)
                                          (label :font small
                                            :text "{{g.title}}"
                                            :textColor @color.muted
                                            :anchors {:left 10 :right 10}
                                            :height 18)))
                                      (node#recent-header :use shelfHeader :with {:title "{{t('lib.recent')}}"})
                                      (row#recent-shelf :height 262
                                        :anchors {:left 0 :right 0}
                                        :gap 18
                                        :scroll horizontal
                                        (column :repeat {:for group :in "{{computed.recentGroups}}" :index gi}
                                          :gap 8
                                          :height 256
                                          :width "{{gi == 0 ? 345 + (length(group.games) - 1) * 164 : length(group.games) * 164 - 14}}"
                                          (label :font small
                                            :text "{{group.label}}"
                                            :textColor @color.muted
                                            :height 18
                                            :anchors {:left 0 :right 0})
                                          (row :gap 14 :height 225 :anchors {:left 0 :right 0} :arrows horizontal
                                            (stack :repeat {:for g :in "{{group.games}}" :index i}
                                              :children [(node :id "{{'recent-big-' + g.id}}"
                                                           :when "{{gi == 0 && i == 0}}"
                                                           :use bigCapsule
                                                           :with {:game "{{g}}"})
                                                         (node :id "{{'recent-' + g.id}}"
                                                           :when "{{!(gi == 0 && i == 0)}}"
                                                           :use capsule
                                                           :with {:game "{{g}}"})]
                                              :width "{{gi == 0 && i == 0 ? 345 : 150}}"
                                              :height 225))))
                                      (column :repeat {:for section
                                                       :in "{{slice(computed.sections, 0, length(computed.sections) - 1)}}"
                                                       :key "{{section.id}}"}
                                        :let {:games "{{filter(computed.owned, contains(section.ids ?? section.games, it.id))}}"}
                                        :anchors {:left 0 :right 0}
                                        :height 280
                                        :gap 6
                                        :when "{{length(games) > 0}}"
                                        (node :use shelfHeader
                                          :with {:title "{{section.title}}" :count "{{length(games)}}"})
                                        (row :height 236
                                          :anchors {:left 0 :right 0}
                                          :gap 14
                                          :scroll horizontal
                                          :arrows horizontal
                                          (node :id "{{'shelf-' + section.id + '-' + g.id}}"
                                            :repeat {:for g :in "{{games}}"}
                                            :use capsule
                                            :with {:game "{{g}}"})))
                                      (node#all-header :use shelfHeader
                                        :with {:title "{{t('lib.allGames')}}" :count "{{length(computed.owned)}}"}
                                        (label :font caps
                                          :text "{{t('lib.sortBy')}}"
                                          :textColor @color.muted
                                          :fitText true)
                                        (label#sort-button :font small
                                          :text "{{t('sort.' + settings.library.sortBy)}}"
                                          :icon chevron-down
                                          :padding 8
                                          :fitText true
                                          :color @color.raised
                                          :radius 3
                                          :textColor @color.text
                                          :cursor hand
                                          :focusable true
                                          :role button
                                          :isOpen "{{state.sortOpen}}"
                                          :hover {:color @color.hover}
                                          :onClick {:toggle state.sortOpen}
                                          (node :use menu
                                            :with {:open state.sortOpen
                                                   :width 180
                                                   :placement below-end
                                                   :items [{:title "{{t('sort.name')}}"
                                                            :checked "{{settings.library.sortBy == 'name'}}"
                                                            :run setSort
                                                            :with {:by name}}
                                                           {:title "{{t('sort.playtime')}}"
                                                            :checked "{{settings.library.sortBy == 'playtime'}}"
                                                            :run setSort
                                                            :with {:by playtime}}
                                                           {:title "{{t('sort.recent')}}"
                                                            :checked "{{settings.library.sortBy == 'recent'}}"
                                                            :run setSort
                                                            :with {:by recent}}]})))
                                      (row#all-grid :wrap true
                                        :gap 14
                                        :anchors {:left 0 :right 0}
                                        :height "{{ceil(length(computed.owned) / max(1, floor(((nodes['home-scroll'].width ?? 1008) - 42) / 164))) * 239}}"
                                        (node :id "{{'all-' + g.id}}"
                                          :repeat {:for g :in "{{computed.allGames}}" :key "{{g.id}}"}
                                          :use capsule
                                          :with {:game "{{g}}"}))
                                      (panel :height 24 :width 1 :color transparent)))}
              :bigCapsule {:note "The latest game played, wide: its banner and logo, play time and a play button."
                           :params {:game {}}
                           :node (node :width 345
                                   :height 225
                                   :radius 12
                                   :color @color.raised
                                   :focusable true
                                   :role button
                                   :cursor hand
                                   :label "{{game.title}}"
                                   :shadow {:y 2 :blur 6 :color "#00000060"}
                                   :hover {:scale 1.02 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
                                   :onClick {:run openGame :with {:id "{{game.id}}"}}
                                   (image :file "{{'../assets/art/' + (game.art ?? game.id) + '/hero.svg'}}"
                                     :fill true
                                     :fit cover
                                     :radius 12)
                                   (node :fill true
                                     :radius 12
                                     :color transparent
                                     :gradient {:kind linear
                                                :stops [{:at 0.35 :color "#00000000"} {:at 1 :color "#000000d0"}]})
                                   (label :font heading
                                     :text "{{game.title}}"
                                     :textColor "{{game.logo}}"
                                     :anchors {:left 14 :bottom 40}
                                     :fitText true)
                                   (label :font small
                                     :anchors {:left 14 :bottom 18}
                                     :fitText true
                                     :textColor "#e2d8e6"
                                     :text "{{t('game.hoursTotal', { n: formatNumber(game.hours, 0, 1) })}}")
                                   (label :icon play
                                     :text ""
                                     :font button
                                     :width 40
                                     :height 40
                                     :align center
                                     :radius 999
                                     :anchors {:right 14 :bottom 14}
                                     :textColor "#ffffff"
                                     :gradient {:kind linear :angle 90 :stops [@color.play @color.playEnd]}))}
              :gamePage {:note "A game's page: the banner and logo, play and the stats, the sections, the activity and the side panels."
                         :params {}
                         :node (column#game-scroll :fill true :scroll vertical :let {:g "{{computed.game}}"}
                                 (node#hero :height 310 :anchors {:left 0 :right 0}
                                   (image#hero-image :file "{{'../assets/art/' + (g.art ?? g.id) + '/hero.svg'}}"
                                     :fill true
                                     :fit cover)
                                   (node :fill true
                                     :color transparent
                                     :gradient {:kind linear
                                                :stops [{:at 0.55 :color "#00000000"} {:at 1 :color "#0d0a10c0"}]})
                                   (label :font logo
                                     :text "{{g.title}}"
                                     :textColor "#000000a0"
                                     :anchors {:left 42 :bottom 34}
                                     :fitText true)
                                   (label#details-title :font logo
                                     :text "{{g.title}}"
                                     :textColor "{{g.logo}}"
                                     :anchors {:left 40 :bottom 36}
                                     :fitText true
                                     :role heading))
                                 (row#play-bar :height 80
                                   :anchors {:left 0 :right 0}
                                   :alignItems center
                                   :gap 26
                                   :color @color.shade
                                   (panel :width 6 :height 1 :color transparent)
                                   (row#play-group :height 48 :width "{{g.status == 'ready' ? 196 : 210}}" :radius 24
                                     (label#play-button :grow 1
                                       :height 48
                                       :font button
                                       :align center
                                       :icon "{{g.status == 'ready' ? 'play' : g.status == 'running' ? 'square' : g.status == 'downloading' ? 'pause' : 'download'}}"
                                       :text "{{t('status.button.' + g.status)}}"
                                       :textColor @color.onAccent
                                       :focusable true
                                       :role button
                                       :cursor hand
                                       :radius {:topLeft 24 :bottomLeft 24}
                                       :color "{{g.status == 'ready' ? '@color.play' : '@color.warm'}}"
                                       :hover {:color "{{g.status == 'ready' ? '@color.playEnd' : '@color.warmEnd'}}"}
                                       :onClick {:run primary :with {:game "{{g}}"}})
                                     (label#play-more :width 34
                                       :height 48
                                       :icon chevron-down
                                       :text ""
                                       :align center
                                       :textColor @color.onAccent
                                       :focusable true
                                       :role button
                                       :label "{{t('game.launchOptions')}}"
                                       :cursor hand
                                       :color "{{g.status == 'ready' ? '@color.play' : '@color.warm'}}"
                                       :hover {:color "{{g.status == 'ready' ? '@color.playEnd' : '@color.warmEnd'}}"}
                                       :radius {:topRight 24 :bottomRight 24}
                                       :borderWidth 1
                                       :borderColor @color.shade
                                       :isOpen "{{state.playOpen}}"
                                       :onClick {:toggle state.playOpen}
                                       (node :use menu
                                         :with {:open state.playOpen
                                                :width 240
                                                :items [{:title "{{t('game.play', { title: g.title })}}"
                                                         :enabled "{{g.status == 'ready'}}"
                                                         :run play
                                                         :with {:id "{{g.id}}"}}
                                                        {:title "{{t('game.playSafe')}}"
                                                         :enabled "{{g.status == 'ready'}}"
                                                         :run play
                                                         :with {:id "{{g.id}}" :safe true}}
                                                        {:separator true}
                                                        {:title "{{t('game.launchOptions')}}"
                                                         :run openProperties
                                                         :with {:id "{{g.id}}" :tab general}}]})))
                                   (node#stat-last :use stat
                                     :with {:title "{{t('game.lastPlayed')}}" :value "{{computed.lastPlayed}}"})
                                   (node#stat-time :use stat
                                     :with {:title "{{t('game.playTime')}}"
                                            :value "{{t('game.hours', { n: formatNumber(g.hours, 0, 1) })}}"})
                                   (column#stat-achievements :width 128 :height 40 :gap 4
                                     (label :font caps
                                       :text "{{upper(t('game.achievements'))}}"
                                       :textColor @color.muted
                                       :height 15
                                       :anchors {:left 0 :right 0})
                                     (row :height 18 :gap 8 :alignItems center :anchors {:left 0 :right 0}
                                       (label :font small
                                         :text "{{g.achievements[0]}}/{{g.achievements[1]}}"
                                         :textColor @color.bright
                                         :fitText true)
                                       (node#achievement-track :grow 1 :height 6 :radius 3 :color @color.raised
                                         (node :anchors {:left 0 :top 0 :bottom 0}
                                           :radius 3
                                           :width "{{text(floor(100 * (g.achievements[0] ?? 0) / max(1, g.achievements[1] ?? 0))) + '%'}}"
                                           :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}))))
                                   (node#stat-cloud :use stat
                                     :with {:title "{{t('game.cloud')}}"
                                            :value "{{t(g.installed ? 'game.cloudOk' : 'game.cloudNone')}}"
                                            :icon cloud})
                                   (panel :grow 1 :height 1 :color transparent)
                                   (node#game-settings :style iconButton
                                     :font heading
                                     :icon settings
                                     :tooltip "{{t('game.manage')}}"
                                     :label "{{t('game.manage')}}"
                                     :onClick {:run openGameMenu :with {:id "{{g.id}}"}})
                                   (node#game-info :style iconButton
                                     :font heading
                                     :icon info
                                     :tooltip "{{t('game.info')}}"
                                     :isOpen "{{state.infoOpen}}"
                                     :onClick {:toggle state.infoOpen}
                                     (column#info-card :overlay {:placement below-end
                                                                 :open state.infoOpen
                                                                 :dismiss outside}
                                       :width 340
                                       :height 196
                                       :color @kit.surface
                                       :radius 4
                                       :gap 6
                                       :role dialog
                                       :label "{{g.title}}"
                                       :shadow {:y 8 :blur 24 :color "#00000090"}
                                       (label :font strong
                                         :text "{{g.title}}"
                                         :textColor @color.bright
                                         :anchors {:left 16 :right 16}
                                         :height 34
                                         :align {:vertical end})
                                       (label#details-blurb :text "{{g.blurb}}"
                                         :textColor @color.text
                                         :wrap true
                                         :anchors {:left 16 :right 16}
                                         :height 60)
                                       (label :font small
                                         :text "{{t('game.developer')}}: {{g.developer}}"
                                         :textColor @color.muted
                                         :anchors {:left 16 :right 16}
                                         :height 18)
                                       (label :font small
                                         :text "{{t('game.publisher')}}: {{g.publisher}}"
                                         :textColor @color.muted
                                         :anchors {:left 16 :right 16}
                                         :height 18)
                                       (label :font small
                                         :text "{{t('game.release')}}: {{g.release}}"
                                         :textColor @color.muted
                                         :anchors {:left 16 :right 16}
                                         :height 18)))
                                   (node#game-favourite :style iconButton
                                     :font heading
                                     :icon star
                                     :textColor "{{g.favourite ? '#ffc82c' : '@color.muted'}}"
                                     :selected "{{g.favourite}}"
                                     :tooltip "{{t(g.favourite ? 'game.unfavourite' : 'game.favourite')}}"
                                     :onClick {:run toggleFavourite :with {:id "{{g.id}}"}})
                                   (panel :width 14 :height 1 :color transparent))
                                 (row#game-sections :height 46
                                   :anchors {:left 24 :right 24}
                                   :alignItems center
                                   :arrows horizontal
                                   (node :id "{{'section-link-' + s}}"
                                     :repeat {:for s
                                              :in "{{g.local ? ['support'] : ['store', 'dlc', 'hub', 'discussions', 'guides', 'support']}}"}
                                     :style sectionLink
                                     :text "{{t('game.section.' + s)}}"
                                     :onClick [{:if "{{s == 'store'}}"
                                                :then [{:run openStore :with {:id "{{g.id}}"}}]
                                                :else [{:if "{{s == 'dlc'}}"
                                                        :then [{:run openProperties :with {:id "{{g.id}}" :tab dlc}}]
                                                        :else [{:if "{{s == 'support'}}"
                                                                :then [{:run showInfo :with {:kind support}}]
                                                                :else [{:run openHub
                                                                        :with {:id "{{g.id}}" :section "{{s}}"}}]}]}]}]))
                                 (panel :height 1 :anchors {:left 32 :right 32} :color @color.border)
                                 (row#game-columns :anchors {:left 32 :right 32}
                                   :gap 24
                                   :height "{{max(700, 150 + length(filter(data.activity, it.game == g.id)) * 96 + length(filter(data.news, it.game == g.id)) * 120)}}"
                                   (column#activity :grow 1 :gap 12 :anchors {:top 16 :bottom 0}
                                     (label :font caps
                                       :text "{{upper(t('game.activity'))}}"
                                       :textColor @color.text
                                       :height 22
                                       :anchors {:left 0 :right 0})
                                     (textarea#post-box :height 44
                                       :anchors {:left 0 :right 0}
                                       :padding 12
                                       :bind state.post
                                       :placeholder "{{t('game.say')}}"
                                       :placeholderColor @color.dim
                                       :textColor @color.text
                                       :color @color.panel
                                       :radius 3
                                       :label "{{t('game.say')}}"
                                       :focused {:borderWidth 1 :borderColor @color.accent})
                                     (row :repeat {:for n :in "{{filter(data.news, it.game == g.id)}}" :index i}
                                       :height 108
                                       :anchors {:left 0 :right 0}
                                       :style card
                                       :gap 14
                                       :alignItems center
                                       (image :file "{{'../assets/art/' + (g.art ?? g.id) + '/hero.svg'}}"
                                         :width 180
                                         :height 108
                                         :fit cover
                                         :radius {:topLeft 3 :bottomLeft 3})
                                       (column :grow 1 :height 84 :gap 4
                                         (label :font caps
                                           :anchors {:left 0 :right 0}
                                           :height 16
                                           :runs [{:text "{{upper(t('news.' + n.kind))}}" :color @color.link}
                                                  {:text " · {{t('time.daysAgo', { n: n.ago })}}" :color @color.muted}])
                                         (label :font strong
                                           :text "{{n.title}}"
                                           :textColor @color.bright
                                           :anchors {:left 0 :right 0}
                                           :height 20)
                                         (label :font small
                                           :text "{{n.text}}"
                                           :textColor @color.muted
                                           :wrap true
                                           :anchors {:left 0 :right 12}
                                           :height 36)))
                                     (row :repeat {:for a :in "{{filter(data.activity, it.game == g.id)}}" :index i}
                                       :let {:f "{{find(data.friends, it.id == a.who)}}"}
                                       :height "{{a.kind == 'screenshot' ? 168 : 84}}"
                                       :anchors {:left 0 :right 0}
                                       :style card
                                       :gap 12
                                       (image :file "{{'../assets/art/avatars/' + f.avatar + '.svg'}}"
                                         :width 36
                                         :height 36
                                         :anchors {:top 14}
                                         :fit cover
                                         :radius 2
                                         :borderWidth 1
                                         :borderColor "{{f.presence == 'ingame' ? '@color.ingame' : f.presence == 'offline' ? '@color.offline' : '@color.online'}}")
                                       (column :grow 1 :gap 4 :anchors {:top 14 :bottom 10}
                                         (label :anchors {:left 0 :right 0}
                                           :height 18
                                           :runs [{:text "{{f.name}} " :color @color.online :bold true}
                                                  {:text "{{t('activity.' + a.kind, { text: a.text })}}"
                                                   :color @color.text}])
                                         (label :when "{{a.detail != null}}"
                                           :font small
                                           :text "{{a.detail}}"
                                           :textColor @color.muted
                                           :anchors {:left 0 :right 0}
                                           :height 16)
                                         (image :when "{{a.kind == 'screenshot'}}"
                                           :file "{{'../assets/art/' + (g.art ?? g.id) + '/shot' + a.shot + '.svg'}}"
                                           :width 160
                                           :height 90
                                           :fit cover
                                           :radius 2)
                                         (label :font small
                                           :text "{{t('time.daysAgo', { n: a.ago })}}"
                                           :textColor @color.dim
                                           :anchors {:left 0 :right 0}
                                           :height 16)))
                                     (label :when "{{length(filter(data.activity, it.game == g.id)) + length(filter(data.news, it.game == g.id)) == 0}}"
                                       :text "{{t('game.noActivity')}}"
                                       :textColor @color.muted
                                       :height 40
                                       :anchors {:left 0 :right 0}))
                                   (column#game-side :width 312 :gap 14 :anchors {:top 16 :bottom 0}
                                     (node#friends-card :use sideCard
                                       :with {:title "{{t('game.friendsWhoPlay')}}" :height 118}
                                       (column :let {:who "{{filter(data.friends, it.game == g.id || g.hours > 40 && (it.presence == 'online' || it.avatar == 'owl'))}}"}
                                         :anchors {:left 14 :right 14 :top 40}
                                         :height 70
                                         :gap 8
                                         (label :font small
                                           :text "{{t('game.friendsCount', { n: length(who) })}}"
                                           :textColor @color.muted
                                           :anchors {:left 0 :right 0}
                                           :height 16)
                                         (row :gap 6 :height 36 :anchors {:left 0 :right 0}
                                           (image :repeat {:for f :in "{{slice(who, 0, 7)}}"}
                                             :file "{{'../assets/art/avatars/' + f.avatar + '.svg'}}"
                                             :width 34
                                             :height 34
                                             :fit cover
                                             :radius 2
                                             :tooltip "{{f.name}}"
                                             :borderWidth 1
                                             :borderColor "{{f.game == g.id ? '@color.ingame' : '@color.online'}}"))))
                                     (node#achievements-card :use sideCard
                                       :with {:title "{{t('game.achievements')}}" :height 150}
                                       (column :anchors {:left 14 :right 14 :top 40} :height 100 :gap 8
                                         (label :font small
                                           :textColor @color.muted
                                           :anchors {:left 0 :right 0}
                                           :height 16
                                           :text "{{t('game.unlocked', { got: g.achievements[0], total: g.achievements[1], percent: floor(g.achievements[0] / max(1, g.achievements[1]) * 100) })}}")
                                         (node#achievement-track-side :height 6
                                           :radius 3
                                           :color @color.raised
                                           :anchors {:left 0 :right 0}
                                           (node :anchors {:left 0 :top 0 :bottom 0}
                                             :radius 3
                                             :width "{{text(floor(100 * (g.achievements[0] ?? 0) / max(1, g.achievements[1] ?? 0))) + '%'}}"
                                             :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}))
                                         (row :gap 6 :height 44 :anchors {:left 0 :right 0}
                                           (label :repeat {:for icon
                                                           :in "{{['trophy', 'flame', 'gem', 'crown', 'swords']}}"
                                                           :index k}
                                             :icon "{{icon}}"
                                             :text ""
                                             :font heading
                                             :width 42
                                             :height 44
                                             :align center
                                             :radius 3
                                             :textColor "{{k < 4 ? g.logo : '@color.dim'}}"
                                             :gradient {:kind linear :stops [@color.hover @color.raised]})
                                           (label :font strong
                                             :text "+{{max(0, g.achievements[1] - 5)}}"
                                             :width 44
                                             :height 44
                                             :align center
                                             :textColor @color.muted
                                             :color @color.raised
                                             :radius 3))))
                                     (node#screenshots-card :use sideCard
                                       :with {:title "{{t('game.screenshots')}}" :height 132}
                                       (row :gap 6 :anchors {:left 14 :right 14 :top 40} :height 80
                                         (image :repeat {:for k :in "{{[1, 2, 3]}}"}
                                           :file "{{'../assets/art/' + (g.art ?? g.id) + '/shot' + k + '.svg'}}"
                                           :grow 1
                                           :height 80
                                           :fit cover
                                           :radius 2)))
                                     (node#controller-card :use sideCard
                                       :with {:title "{{t('game.controller')}}" :height 74}
                                       (label :icon gamepad-2
                                         :text "{{t('controller.' + g.controller)}}"
                                         :textColor @color.text
                                         :anchors {:left 14 :right 14 :top 42}
                                         :height 20))
                                     (node#notes-card :use sideCard :with {:title "{{t('game.notes')}}" :height 74}
                                       (label#write-note :icon pencil
                                         :text "{{empty(g.note) ? t('game.writeNote') : g.note}}"
                                         :onClick {:run editNote :with {:id "{{g.id}}"}}
                                         :textColor @color.link
                                         :cursor hand
                                         :focusable true
                                         :role button
                                         :anchors {:left 14 :top 42}
                                         :width 200
                                         :height 20
                                         :hover {:textColor @color.bright})))))}
              :stat {:params {:title "" :value "" :icon ""}
                     :node (column :height 40 :gap 4 :width "{{max(90, length(value) * 8 + 24)}}"
                             (label :font caps
                               :text "{{upper(title)}}"
                               :textColor @color.muted
                               :height 15
                               :anchors {:left 0 :right 0})
                             (label :font small
                               :icon "{{icon}}"
                               :text "{{value}}"
                               :textColor @color.bright
                               :height 18
                               :anchors {:left 0 :right 0}))}
              :sideCard {:params {:title "" :height 100}
                         :node (node :anchors {:left 0 :right 0} :height "{{height}}" :style card
                                 (label :font caps
                                   :text "{{upper(title)}}"
                                   :textColor @color.text
                                   :anchors {:left 14 :top 14}
                                   :fitText true)
                                 (node :slot true))}}}
