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
    :collections [(node #coop :title "Co-op Nights" :games [ember pebble neon])
      (node #cozy :title "Cozy Sundays" :games [tidebreaker mossheart driftwood lumen])]}
  :computed {:libList "{{sort(filter(computed.owned, (!state.readyOnly || it.installed) && (!state.favOnly || it.favourite) && (empty(state.query) || fuzzy(state.query, it.title) != null)), state.recentFirst ? it.lastPlayed ?? 0 : lower(it.title), state.recentFirst ? 'desc' : 'asc')}}"
    :collected "{{split(join(map(state.collections, join(it.games, ',')), ','), ',')}}"
    :sections "{{concat([{ id: 'favourites', title: t('lib.favourites'), ids: map(filter(computed.owned, it.favourite), it.id) }], state.collections, [{ id: 'uncategorised', title: t('lib.uncategorised'), ids: map(filter(computed.owned, !it.favourite && !contains(computed.collected, it.id)), it.id) }])}}"
    :recent "{{slice(sort(filter(computed.owned, it.lastPlayed != null), it.lastPlayed, 'desc'), 0, 10)}}"
    :recentGroups "{{filter([{ label: t('time.today'), games: filter(computed.recent, state.today - it.lastPlayed < 86400000) }, { label: t('time.yesterday'), games: filter(computed.recent, state.today - it.lastPlayed >= 86400000 && state.today - it.lastPlayed < 172800000) }, { label: t('time.thisWeek'), games: filter(computed.recent, state.today - it.lastPlayed >= 172800000 && state.today - it.lastPlayed < 604800000) }, { label: t('time.lastWeek'), games: filter(computed.recent, state.today - it.lastPlayed >= 604800000 && state.today - it.lastPlayed < 1209600000) }, { label: t('time.earlier'), games: filter(computed.recent, state.today - it.lastPlayed >= 1209600000) }], length(it.games) > 0)}}"
    :allGames "{{settings.library.sortBy == 'playtime' ? sort(computed.owned, it.hours, 'desc') : settings.library.sortBy == 'recent' ? sort(computed.owned, it.lastPlayed ?? 0, 'desc') : sort(computed.owned, lower(it.title))}}"
    :lastPlayed "{{computed.game.lastPlayed == null ? t('time.never') : state.today - computed.game.lastPlayed < 86400000 ? t('time.today') : state.today - computed.game.lastPlayed < 172800000 ? t('time.yesterday') : formatDate(computed.game.lastPlayed, 'd MMM', 'utc')}}"}
  :actions {:toggleCollections {:title "Collapse or expand every collection"
      :category Library
      :do [{:set state.collapsed
          :to "{{some(computed.sections, !(state.collapsed[it.id] ?? false)) ? fromEntries(map(computed.sections, { key: it.id, value: true })) : {}}}"}]}
    :toggleFilter {:title "Turn a library filter on or off"
      :category Library
      :description "key is readyOnly, favOnly, recentFirst or showNews."
      :do [{:toggle "{{'state.' + key}}"}]}
    :clearFilters {:title "Clear the library filters"
      :category Library
      :do [{:set state.readyOnly :to false}
        {:set state.recentFirst :to false}
        {:set state.favOnly :to false}
        {:set state.query :to ""}]}
    :editNote {:title "Write a note about a game"
      :category Game
      :do [{:set state.note :to (node :open true #"{{id}}" :text "{{find(data.games, it.id == id).note ?? ''}}")}]}
    :saveNote {:title "Save the note"
      :category Game
      :do [(node :update games #"{{state.note.id}}" :fields {:note "{{trim(state.note.text)}}"})]}
    :openGame {:title "Open the game's page"
      :category Game
      :do [{:set state.selected :to "{{id}}"} {:run go :with (node :tab library :page game #"{{id}}")}]}
    :libraryHome {:title "My games overview"
      :category View
      :icon layout-dashboard
      :do [{:run go :with {:tab library :page home :id nil}}]}
    :play {:title Play
      :category Game
      :icon play
      :do [(node :update games #"{{id}}" :fields {:status running :lastPlayed "{{state.today}}"})
        {:set state.running :to "{{id}}"}]}
    :stop {:title Stop
      :category Game
      :icon square
      :do [(node :update games #"{{id}}" :fields {:status ready :hours "{{find(data.games, it.id == id).hours + 0.5}}"})
        {:set state.running :to nil}]}
    :install {:title Install
      :category Game
      :icon download
      :description "Queues the game, or its update: it downloads at once if nothing else is."
      :do [{:if "{{find(data.games, it.id == id) != null && find(state.downloads, it.id == id && it.status != 'done') == null}}"
          :then [{:push state.downloads
              :value "{{{ id: id, sizeGb: find(data.games, it.id == id).status == 'update' ? 1.2 : find(data.games, it.id == id).sizeGb, doneGb: 0, status: computed.download ? 'queued' : 'downloading', kind: find(data.games, it.id == id).status == 'update' ? 'update' : 'install' }}}"}
            (node :update games #"{{id}}" :fields {:status downloading})]}]}
    :toggleFavourite {:title "Add to or remove from favourites"
      :category Game
      :icon star
      :do [(node :update games #"{{id}}" :fields {:favourite "{{!find(data.games, it.id == id).favourite}}"})]}
    :setSort {:title "Sort the library"
      :category Library
      :description "Sorts All Games by name, hours played or last played."
      :do [{:set settings.library.sortBy :to "{{by}}"}]}
    :primary {:title "Play, install or update"
      :category Game
      :do [{:if "{{game.status == 'ready'}}"
          :then [{:run play :with (node #"{{game.id}}")}]
          :else [{:if "{{game.status == 'running'}}"
              :then [{:run stop :with (node #"{{game.id}}")}]
              :else [{:if "{{game.status == 'none' || game.status == 'update'}}"
                  :then [{:run install :with (node #"{{game.id}}")}]}]}]}]}}
  :styles {:caps {:type label :font caps :textColor @color.muted :fitText true}
    :iconButton {:type label
      :text ""
      :fitText true
      :padding 7
      :radius 3
      :cursor hand
      :focusable true
      :role button
      :textColor @color.muted
      :hover {:color @color.hover :textColor @color.bright}}
    :sectionLink {:type label
      :fitText true
      :padding 10
      :cursor hand
      :focusable true
      :role link
      :textColor @color.text
      :hover {:textColor @color.bright}}
    :card {:color @color.panel :radius 12}}
  :components {:noteDialog {:note "A note about a game, kept with it in the library."
      :params {}
      :node {:use dialog
        :with {:open state.note.open
          :width 460
          :height 240
          :title "{{t('note.title')}}"
          :message "{{t('note.help')}}"
          :buttons [{:title "{{t('dialog.save')}}" :run saveNote :default true} {:title "{{t('dialog.cancel')}}"}]}
        :children [(node #note-text
            :type textarea
            :wrap true
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
            :autofocus true)]}}
    :libraryPage {:note "The library: the game list on the left, the home or a game's page on the right."
      :params {}
      :node {:type row
        :fill true
        :children [(node :use librarySidebar #library-sidebar)
          (node #library-main
            :grow 1
            :anchors {:top 0 :bottom 0}
            :gradient {:kind linear :stops [@color.main @color.page]}
            (node :when "{{computed.here.page == 'home'}}" :use libraryHome #library-home)
            (node :when "{{computed.here.page == 'game' && computed.game != null}}" :use gamePage #game-page)
            (node :when "{{computed.here.page == 'hub' && computed.game != null}}" :use hubPage #hub-page))]}}
    :librarySidebar {:note "Home, the games filter with recent-first and ready-to-play, search, and the games by collection."
      :params {}
      :node {:type column
        :width 272
        :anchors {:top 0 :bottom 0}
        :color @color.side
        :children [{:type row
            :height 40
            :anchors {:left 0 :right 0}
            :color @color.sideTop
            :alignItems center
            :children [(node #library-home-button
                :type label
                :grow 1
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
              (node #library-collections
                :style iconButton
                :icon "{{some(computed.sections, !(state.collapsed[it.id] ?? false)) ? 'list-collapse' : 'list'}}"
                :tooltip "{{t(some(computed.sections, !(state.collapsed[it.id] ?? false)) ? 'lib.collapseAll' : 'lib.expandAll')}}"
                :onClick toggleCollections)]}
          {:type row
            :height 36
            :anchors {:left 0 :right 0}
            :alignItems center
            :children [(node #library-kind
                :type label
                :font caps
                :text "{{t('lib.games')}}"
                :icon chevron-down
                :textColor @color.muted
                :grow 1
                :padding 14
                :height 36)
              (node #library-recent
                :style iconButton
                :icon clock-3
                :tooltip "{{t('lib.recentFirst')}}"
                :textColor "{{state.recentFirst ? '@color.accent' : '@color.muted'}}"
                :selected "{{state.recentFirst}}"
                :onClick {:toggle state.recentFirst})
              (node #library-ready
                :style iconButton
                :icon circle-play
                :tooltip "{{t('lib.readyOnly')}}"
                :textColor "{{state.readyOnly ? '@color.accent' : '@color.muted'}}"
                :selected "{{state.readyOnly}}"
                :onClick {:toggle state.readyOnly})
              {:type panel :width 6 :height 1 :color transparent}]}
          {:type row
            :height 34
            :anchors {:left 0 :right 0}
            :alignItems center
            :children [{:type panel :width 10 :height 1 :color transparent}
              (node #library-search
                :type textarea
                :grow 1
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
                {:type label :icon search :text "" :textColor @color.dim :fitText true :anchors {:right 8 :top 6}})
              (node #library-filter
                :style iconButton
                :icon sliders-horizontal
                :tooltip "{{t('lib.advanced')}}"
                :textColor "{{state.readyOnly || state.recentFirst || state.favOnly ? '@color.accent' : '@color.muted'}}"
                :isOpen "{{state.filterOpen}}"
                :onClick {:toggle state.filterOpen}
                {:use menu
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
                      {:title "{{t('lib.clearFilters')}}" :run clearFilters}]}})
              {:type panel :width 6 :height 1 :color transparent}]}
          (node #library-list
            :type column
            :grow 1
            :anchors {:left 0 :right 0}
            :scroll vertical
            :arrows vertical
            :role list
            :label "{{t('lib.games')}}"
            {:repeat {:for section :in "{{computed.sections}}" :key "{{section.id}}"}
              :let {:games "{{filter(computed.libList, contains(section.ids ?? section.games, it.id))}}"
                :open "{{!(state.collapsed[section.id] ?? false)}}"}
              :type column
              :anchors {:left 0 :right 0}
              :height "{{30 + (open ? length(games) * 28 : 0)}}"
              :when "{{length(games) > 0 || empty(state.query)}}"
              :children [(node #"{{'section-' + section.id}}"
                  :type label
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
                (node :when "{{open}}"
                  :repeat {:for g :in "{{games}}" :key "{{section.id + g.id}}"}
                  :use gameRow
                  :with {:game "{{g}}"}
                  #"{{'row-' + section.id + '-' + g.id}}")]})]}}
    :gameRow {:note "A game in the list: its cover as an icon and its name in its play-state colour, with what it is doing."
      :params {:game {}}
      :node {:type row
        :height 28
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
        :onClick {:run openGame :with (node #"{{game.id}}")}
        :onDoubleClick {:run primary :with {:game "{{game}}"}}
        :onContextMenu {:run openGameMenu :with (node #"{{game.id}}")}
        :children [{:type panel :width 18 :height 1 :color transparent}
          {:type image
            :file "{{'art/' + (game.art ?? game.id) + '/cover.svg'}}"
            :width 18
            :height 18
            :fit cover
            :radius 2}
          {:type label
            :grow 1
            :height 18
            :font small
            :runs [{:text "{{game.title}}"}
              {:text "{{game.status == 'running' ? ' – ' + t('status.running') : game.status == 'update' ? ' – ' + t('status.update') : game.status == 'downloading' ? ' – ' + t('status.downloading') : ''}}"}]
            :textColor "{{game.status == 'running' ? '@color.ingame' : game.status == 'update' || game.status == 'downloading' ? '@color.accent' : game.installed ? '@color.text' : '@color.dim'}}"}]}}
    :capsule {:note "A game's cover, 2:3, that lifts and glows under the pointer; dimmed when the game isn't installed."
      :params {:game {} :width 150}
      :node {:width "{{width}}"
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
        :onClick {:run openGame :with (node #"{{game.id}}")}
        :onContextMenu {:run openGameMenu :with (node #"{{game.id}}")}
        :children [{:type image
            :file "{{'art/' + (game.art ?? game.id) + '/cover.svg'}}"
            :fill true
            :fit cover
            :radius 12}
          {:when "{{!game.installed}}" :fill true :color "#100c1480" :radius 12}]}}
    :shelfHeader {:params {:title "" :count nil}
      :node {:type row
        :height 34
        :anchors {:left 0 :right 0}
        :alignItems center
        :gap 8
        :children [{:type label :font caps :text "{{upper(title)}}" :textColor @color.text :fitText true}
          {:when "{{count != null}}" :type label :font caps :text "({{count}})" :textColor @color.muted :fitText true}
          {:type panel :grow 1 :height 1 :color transparent}
          {:slot true}]}}
    :libraryHome {:note "The library's home: what's new, recent games grouped by day, a shelf per collection and every game."
      :params {}
      :node (node #home-scroll :type column :fill true :scroll vertical
        (node #home-content
          :type column
          :anchors {:left 28 :right 28}
          :gap 6
          :height "{{412 + 286 * length(filter(computed.sections, it.id != 'uncategorised' && length(it.ids ?? it.games) > 0)) + 239 * ceil(length(computed.owned) / max(1, floor(((nodes['home-scroll'].width ?? 1008) - 42) / 164)))}}"
          {:type panel :height 10 :width 1 :color transparent}
          (node :use shelfHeader #news-header :with {:title "{{t('lib.whatsNew')}}"}
            (node :style iconButton
              :icon settings
              #news-settings
              :tooltip "{{t('lib.whatsNewSettings')}}"
              :isOpen "{{state.newsOpen}}"
              :onClick {:toggle state.newsOpen}
              {:use menu
                :with {:open state.newsOpen
                  :width 220
                  :placement below-end
                  :items [{:title "{{t('lib.showNews')}}"
                      :checked "{{state.showNews}}"
                      :run toggleFilter
                      :with {:key showNews}}]}}))
          (node #news-shelf
            :when "{{state.showNews}}"
            :type row
            :height 222
            :anchors {:left 0 :right 0}
            :gap 14
            :scroll horizontal
            :arrows horizontal
            (node :repeat {:for n :in "{{data.news}}" :index i}
              :let {:g "{{find(data.games, it.id == n.game)}}"}
              #"{{'news-' + i}}"
              :type column
              :width 232
              :height 216
              :style card
              :focusable true
              :role link
              :label "{{n.title}}"
              :cursor hand
              :hover {:color @color.hover :shadow {:y 4 :blur 14 :color "#00000080"}}
              :onClick {:run openGame :with (node #"{{n.game}}")}
              {:type image
                :file "{{'art/' + n.game + '/hero.svg'}}"
                :anchors {:left 0 :right 0}
                :height 120
                :fit cover
                :radius {:topLeft 8 :topRight 8}}
              {:type label
                :font caps
                :height 26
                :anchors {:left 0 :right 0}
                :padding 10
                :runs [{:text "{{upper(t('news.' + n.kind))}}" :color @color.link}
                  {:text " · {{t('time.daysAgo', { n: n.ago })}}" :color @color.muted}]}
              {:type label
                :font strong
                :text "{{n.title}}"
                :textColor @color.bright
                :wrap true
                :anchors {:left 10 :right 10}
                :height 40}
              {:type label
                :font small
                :text "{{g.title}}"
                :textColor @color.muted
                :anchors {:left 10 :right 10}
                :height 18}))
          (node :use shelfHeader #recent-header :with {:title "{{t('lib.recent')}}"})
          (node #recent-shelf :type row :height 262 :anchors {:left 0 :right 0} :gap 18 :scroll horizontal
            {:repeat {:for group :in "{{computed.recentGroups}}" :index gi}
              :type column
              :gap 8
              :height 256
              :width "{{gi == 0 ? 345 + (length(group.games) - 1) * 164 : length(group.games) * 164 - 14}}"
              :children [{:type label
                  :font small
                  :text "{{group.label}}"
                  :textColor @color.muted
                  :height 18
                  :anchors {:left 0 :right 0}}
                {:type row
                  :gap 14
                  :height 225
                  :anchors {:left 0 :right 0}
                  :arrows horizontal
                  :children [{:repeat {:for g :in "{{group.games}}" :index i}
                      :children [(node :when "{{gi == 0 && i == 0}}"
                          :use bigCapsule
                          :with {:game "{{g}}"}
                          #"{{'recent-big-' + g.id}}")
                        (node :when "{{!(gi == 0 && i == 0)}}"
                          :use capsule
                          :with {:game "{{g}}"}
                          #"{{'recent-' + g.id}}")]
                      :type stack
                      :width "{{gi == 0 && i == 0 ? 345 : 150}}"
                      :height 225}]}]})
          {:repeat {:for section
              :in "{{slice(computed.sections, 0, length(computed.sections) - 1)}}"
              :key "{{section.id}}"}
            :let {:games "{{filter(computed.owned, contains(section.ids ?? section.games, it.id))}}"}
            :type column
            :anchors {:left 0 :right 0}
            :height 280
            :gap 6
            :when "{{length(games) > 0}}"
            :children [{:use shelfHeader :with {:title "{{section.title}}" :count "{{length(games)}}"}}
              {:type row
                :height 236
                :anchors {:left 0 :right 0}
                :gap 14
                :scroll horizontal
                :arrows horizontal
                :children [(node :repeat {:for g :in "{{games}}"}
                    :use capsule
                    :with {:game "{{g}}"}
                    #"{{'shelf-' + section.id + '-' + g.id}}")]}]}
          (node :use shelfHeader #all-header :with {:title "{{t('lib.allGames')}}" :count "{{length(computed.owned)}}"}
            {:type label :font caps :text "{{t('lib.sortBy')}}" :textColor @color.muted :fitText true}
            (node #sort-button
              :type label
              :font small
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
              {:use menu
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
                      :with {:by recent}}]}}))
          (node #all-grid
            :type row
            :wrap true
            :gap 14
            :anchors {:left 0 :right 0}
            :height "{{ceil(length(computed.owned) / max(1, floor(((nodes['home-scroll'].width ?? 1008) - 42) / 164))) * 239}}"
            (node :repeat {:for g :in "{{computed.allGames}}" :key "{{g.id}}"}
              :use capsule
              :with {:game "{{g}}"}
              #"{{'all-' + g.id}}"))
          {:type panel :height 24 :width 1 :color transparent}))}
    :bigCapsule {:note "The latest game played, wide: its banner and logo, play time and a play button."
      :params {:game {}}
      :node {:width 345
        :height 225
        :radius 12
        :color @color.raised
        :focusable true
        :role button
        :cursor hand
        :label "{{game.title}}"
        :shadow {:y 2 :blur 6 :color "#00000060"}
        :hover {:scale 1.02 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
        :onClick {:run openGame :with (node #"{{game.id}}")}
        :children [{:type image
            :file "{{'art/' + (game.art ?? game.id) + '/hero.svg'}}"
            :fill true
            :fit cover
            :radius 12}
          {:fill true
            :radius 12
            :color transparent
            :gradient {:kind linear :stops [{:at 0.35 :color "#00000000"} {:at 1 :color "#000000d0"}]}}
          {:type label
            :font heading
            :text "{{game.title}}"
            :textColor "{{game.logo}}"
            :anchors {:left 14 :bottom 40}
            :fitText true}
          {:type label
            :font small
            :anchors {:left 14 :bottom 18}
            :fitText true
            :textColor "#e2d8e6"
            :text "{{t('game.hoursTotal', { n: formatNumber(game.hours, 0, 1) })}}"}
          {:type label
            :icon play
            :text ""
            :font button
            :width 40
            :height 40
            :align center
            :radius 999
            :anchors {:right 14 :bottom 14}
            :textColor "#ffffff"
            :gradient {:kind linear :angle 90 :stops [@color.play @color.playEnd]}}]}}
    :gamePage {:note "A game's page: the banner and logo, play and the stats, the sections, the activity and the side panels."
      :params {}
      :node (node #game-scroll :type column :fill true :scroll vertical :let {:g "{{computed.game}}"}
        (node #hero :height 310 :anchors {:left 0 :right 0}
          (node #hero-image :type image :file "{{'art/' + (g.art ?? g.id) + '/hero.svg'}}" :fill true :fit cover)
          {:fill true
            :color transparent
            :gradient {:kind linear :stops [{:at 0.55 :color "#00000000"} {:at 1 :color "#0d0a10c0"}]}}
          {:type label
            :font logo
            :text "{{g.title}}"
            :textColor "#000000a0"
            :anchors {:left 42 :bottom 34}
            :fitText true}
          (node #details-title
            :type label
            :font logo
            :text "{{g.title}}"
            :textColor "{{g.logo}}"
            :anchors {:left 40 :bottom 36}
            :fitText true
            :role heading))
        (node #play-bar :type row :height 80 :anchors {:left 0 :right 0} :alignItems center :gap 26 :color @color.shade
          {:type panel :width 6 :height 1 :color transparent}
          (node #play-group :type row :height 48 :width "{{g.status == 'ready' ? 196 : 210}}" :radius 24
            (node #play-button
              :type label
              :grow 1
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
            (node #play-more
              :type label
              :width 34
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
              {:use menu
                :with {:open state.playOpen
                  :width 240
                  :items [{:title "{{t('game.play', { title: g.title })}}"
                      :enabled "{{g.status == 'ready'}}"
                      :run play
                      :with (node #"{{g.id}}")}
                    {:title "{{t('game.playSafe')}}"
                      :enabled "{{g.status == 'ready'}}"
                      :run play
                      :with (node #"{{g.id}}" :safe true)}
                    {:separator true}
                    {:title "{{t('game.launchOptions')}}" :run openProperties :with (node #"{{g.id}}" :tab general)}]}}))
          (node :use stat #stat-last :with {:title "{{t('game.lastPlayed')}}" :value "{{computed.lastPlayed}}"})
          (node :use stat
            #stat-time
            :with {:title "{{t('game.playTime')}}" :value "{{t('game.hours', { n: formatNumber(g.hours, 0, 1) })}}"})
          (node #stat-achievements :type column :width 128 :height 40 :gap 4
            {:type label
              :font caps
              :text "{{upper(t('game.achievements'))}}"
              :textColor @color.muted
              :height 15
              :anchors {:left 0 :right 0}}
            {:type row
              :height 18
              :gap 8
              :alignItems center
              :anchors {:left 0 :right 0}
              :children [{:type label
                  :font small
                  :text "{{g.achievements[0]}}/{{g.achievements[1]}}"
                  :textColor @color.bright
                  :fitText true}
                (node #achievement-track :grow 1 :height 6 :radius 3 :color @color.raised
                  {:anchors {:left 0 :top 0 :bottom 0}
                    :radius 3
                    :width "{{text(floor(100 * (g.achievements[0] ?? 0) / max(1, g.achievements[1] ?? 0))) + '%'}}"
                    :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}})]})
          (node :use stat
            #stat-cloud
            :with {:title "{{t('game.cloud')}}"
              :value "{{t(g.installed ? 'game.cloudOk' : 'game.cloudNone')}}"
              :icon cloud})
          {:type panel :grow 1 :height 1 :color transparent}
          (node #game-settings
            :style iconButton
            :font heading
            :icon settings
            :tooltip "{{t('game.manage')}}"
            :label "{{t('game.manage')}}"
            :onClick {:run openGameMenu :with (node #"{{g.id}}")})
          (node #game-info
            :style iconButton
            :font heading
            :icon info
            :tooltip "{{t('game.info')}}"
            :isOpen "{{state.infoOpen}}"
            :onClick {:toggle state.infoOpen}
            (node #info-card
              :overlay {:placement below-end :open state.infoOpen :dismiss outside}
              :type column
              :width 340
              :height 196
              :color @kit.surface
              :radius 4
              :gap 6
              :role dialog
              :label "{{g.title}}"
              :shadow {:y 8 :blur 24 :color "#00000090"}
              {:type label
                :font strong
                :text "{{g.title}}"
                :textColor @color.bright
                :anchors {:left 16 :right 16}
                :height 34
                :align {:vertical end}}
              (node #details-blurb
                :type label
                :text "{{g.blurb}}"
                :textColor @color.text
                :wrap true
                :anchors {:left 16 :right 16}
                :height 60)
              {:type label
                :font small
                :text "{{t('game.developer')}}: {{g.developer}}"
                :textColor @color.muted
                :anchors {:left 16 :right 16}
                :height 18}
              {:type label
                :font small
                :text "{{t('game.publisher')}}: {{g.publisher}}"
                :textColor @color.muted
                :anchors {:left 16 :right 16}
                :height 18}
              {:type label
                :font small
                :text "{{t('game.release')}}: {{g.release}}"
                :textColor @color.muted
                :anchors {:left 16 :right 16}
                :height 18}))
          (node #game-favourite
            :style iconButton
            :font heading
            :icon star
            :textColor "{{g.favourite ? '#ffc82c' : '@color.muted'}}"
            :selected "{{g.favourite}}"
            :tooltip "{{t(g.favourite ? 'game.unfavourite' : 'game.favourite')}}"
            :onClick {:run toggleFavourite :with (node #"{{g.id}}")})
          {:type panel :width 14 :height 1 :color transparent})
        (node #game-sections :type row :height 46 :anchors {:left 24 :right 24} :alignItems center :arrows horizontal
          (node :repeat {:for s
              :in "{{g.local ? ['support'] : ['store', 'dlc', 'hub', 'discussions', 'guides', 'support']}}"}
            #"{{'section-link-' + s}}"
            :style sectionLink
            :text "{{t('game.section.' + s)}}"
            :onClick [{:if "{{s == 'store'}}"
                :then [{:run openStore :with (node #"{{g.id}}")}]
                :else [{:if "{{s == 'dlc'}}"
                    :then [{:run openProperties :with (node #"{{g.id}}" :tab dlc)}]
                    :else [{:if "{{s == 'support'}}"
                        :then [{:run showInfo :with {:kind support}}]
                        :else [{:run openHub :with (node #"{{g.id}}" :section "{{s}}")}]}]}]}]))
        {:type panel :height 1 :anchors {:left 32 :right 32} :color @color.border}
        (node #game-columns
          :type row
          :anchors {:left 32 :right 32}
          :gap 24
          :height "{{max(700, 150 + length(filter(data.activity, it.game == g.id)) * 96 + length(filter(data.news, it.game == g.id)) * 120)}}"
          (node #activity :type column :grow 1 :gap 12 :anchors {:top 16 :bottom 0}
            {:type label
              :font caps
              :text "{{upper(t('game.activity'))}}"
              :textColor @color.text
              :height 22
              :anchors {:left 0 :right 0}}
            (node #post-box
              :type textarea
              :height 44
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
            {:repeat {:for n :in "{{filter(data.news, it.game == g.id)}}" :index i}
              :type row
              :height 108
              :anchors {:left 0 :right 0}
              :style card
              :gap 14
              :alignItems center
              :children [{:type image
                  :file "{{'art/' + (g.art ?? g.id) + '/hero.svg'}}"
                  :width 180
                  :height 108
                  :fit cover
                  :radius {:topLeft 3 :bottomLeft 3}}
                {:type column
                  :grow 1
                  :height 84
                  :gap 4
                  :children [{:type label
                      :font caps
                      :anchors {:left 0 :right 0}
                      :height 16
                      :runs [{:text "{{upper(t('news.' + n.kind))}}" :color @color.link}
                        {:text " · {{t('time.daysAgo', { n: n.ago })}}" :color @color.muted}]}
                    {:type label
                      :font strong
                      :text "{{n.title}}"
                      :textColor @color.bright
                      :anchors {:left 0 :right 0}
                      :height 20}
                    {:type label
                      :font small
                      :text "{{n.text}}"
                      :textColor @color.muted
                      :wrap true
                      :anchors {:left 0 :right 12}
                      :height 36}]}]}
            {:repeat {:for a :in "{{filter(data.activity, it.game == g.id)}}" :index i}
              :let {:f "{{find(data.friends, it.id == a.who)}}"}
              :type row
              :height "{{a.kind == 'screenshot' ? 168 : 84}}"
              :anchors {:left 0 :right 0}
              :style card
              :gap 12
              :children [{:type image
                  :file "{{'art/avatars/' + f.avatar + '.svg'}}"
                  :width 36
                  :height 36
                  :anchors {:top 14}
                  :fit cover
                  :radius 2
                  :borderWidth 1
                  :borderColor "{{f.presence == 'ingame' ? '@color.ingame' : f.presence == 'offline' ? '@color.offline' : '@color.online'}}"}
                {:type column
                  :grow 1
                  :gap 4
                  :anchors {:top 14 :bottom 10}
                  :children [{:type label
                      :anchors {:left 0 :right 0}
                      :height 18
                      :runs [{:text "{{f.name}} " :color @color.online :bold true}
                        {:text "{{t('activity.' + a.kind, { text: a.text })}}" :color @color.text}]}
                    {:when "{{a.detail != null}}"
                      :type label
                      :font small
                      :text "{{a.detail}}"
                      :textColor @color.muted
                      :anchors {:left 0 :right 0}
                      :height 16}
                    {:when "{{a.kind == 'screenshot'}}"
                      :type image
                      :file "{{'art/' + (g.art ?? g.id) + '/shot' + a.shot + '.svg'}}"
                      :width 160
                      :height 90
                      :fit cover
                      :radius 2}
                    {:type label
                      :font small
                      :text "{{t('time.daysAgo', { n: a.ago })}}"
                      :textColor @color.dim
                      :anchors {:left 0 :right 0}
                      :height 16}]}]}
            {:when "{{length(filter(data.activity, it.game == g.id)) + length(filter(data.news, it.game == g.id)) == 0}}"
              :type label
              :text "{{t('game.noActivity')}}"
              :textColor @color.muted
              :height 40
              :anchors {:left 0 :right 0}})
          (node #game-side :type column :width 312 :gap 14 :anchors {:top 16 :bottom 0}
            (node :use sideCard #friends-card :with {:title "{{t('game.friendsWhoPlay')}}" :height 118}
              {:let {:who "{{filter(data.friends, it.game == g.id || g.hours > 40 && (it.presence == 'online' || it.avatar == 'owl'))}}"}
                :type column
                :anchors {:left 14 :right 14 :top 40}
                :height 70
                :gap 8
                :children [{:type label
                    :font small
                    :text "{{t('game.friendsCount', { n: length(who) })}}"
                    :textColor @color.muted
                    :anchors {:left 0 :right 0}
                    :height 16}
                  {:type row
                    :gap 6
                    :height 36
                    :anchors {:left 0 :right 0}
                    :children [{:repeat {:for f :in "{{slice(who, 0, 7)}}"}
                        :type image
                        :file "{{'art/avatars/' + f.avatar + '.svg'}}"
                        :width 34
                        :height 34
                        :fit cover
                        :radius 2
                        :tooltip "{{f.name}}"
                        :borderWidth 1
                        :borderColor "{{f.game == g.id ? '@color.ingame' : '@color.online'}}"}]}]})
            (node :use sideCard #achievements-card :with {:title "{{t('game.achievements')}}" :height 150}
              {:type column
                :anchors {:left 14 :right 14 :top 40}
                :height 100
                :gap 8
                :children [{:type label
                    :font small
                    :textColor @color.muted
                    :anchors {:left 0 :right 0}
                    :height 16
                    :text "{{t('game.unlocked', { got: g.achievements[0], total: g.achievements[1], percent: floor(g.achievements[0] / max(1, g.achievements[1]) * 100) })}}"}
                  (node #achievement-track-side :height 6 :radius 3 :color @color.raised :anchors {:left 0 :right 0}
                    {:anchors {:left 0 :top 0 :bottom 0}
                      :radius 3
                      :width "{{text(floor(100 * (g.achievements[0] ?? 0) / max(1, g.achievements[1] ?? 0))) + '%'}}"
                      :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}})
                  {:type row
                    :gap 6
                    :height 44
                    :anchors {:left 0 :right 0}
                    :children [{:repeat {:for icon :in "{{['trophy', 'flame', 'gem', 'crown', 'swords']}}" :index k}
                        :type label
                        :icon "{{icon}}"
                        :text ""
                        :font heading
                        :width 42
                        :height 44
                        :align center
                        :radius 3
                        :textColor "{{k < 4 ? g.logo : '@color.dim'}}"
                        :gradient {:kind linear :stops [@color.hover @color.raised]}}
                      {:type label
                        :font strong
                        :text "+{{max(0, g.achievements[1] - 5)}}"
                        :width 44
                        :height 44
                        :align center
                        :textColor @color.muted
                        :color @color.raised
                        :radius 3}]}]})
            (node :use sideCard #screenshots-card :with {:title "{{t('game.screenshots')}}" :height 132}
              {:type row
                :gap 6
                :anchors {:left 14 :right 14 :top 40}
                :height 80
                :children [{:repeat {:for k :in "{{[1, 2, 3]}}"}
                    :type image
                    :file "{{'art/' + (g.art ?? g.id) + '/shot' + k + '.svg'}}"
                    :grow 1
                    :height 80
                    :fit cover
                    :radius 2}]})
            (node :use sideCard #controller-card :with {:title "{{t('game.controller')}}" :height 74}
              {:type label
                :icon gamepad-2
                :text "{{t('controller.' + g.controller)}}"
                :textColor @color.text
                :anchors {:left 14 :right 14 :top 42}
                :height 20})
            (node :use sideCard #notes-card :with {:title "{{t('game.notes')}}" :height 74}
              (label #write-note
                :icon pencil
                :text "{{empty(g.note) ? t('game.writeNote') : g.note}}"
                :onClick {:run editNote :with (node #"{{g.id}}")}
                :textColor @color.link
                :cursor hand
                :focusable true
                :role button
                :anchors {:left 14 :top 42}
                :width 200
                :height 20
                :hover {:textColor @color.bright})))))}
    :stat {:params {:title "" :value "" :icon ""}
      :node {:type column
        :height 40
        :gap 4
        :width "{{max(90, length(value) * 8 + 24)}}"
        :children [{:type label
            :font caps
            :text "{{upper(title)}}"
            :textColor @color.muted
            :height 15
            :anchors {:left 0 :right 0}}
          {:type label
            :font small
            :icon "{{icon}}"
            :text "{{value}}"
            :textColor @color.bright
            :height 18
            :anchors {:left 0 :right 0}}]}}
    :sideCard {:params {:title "" :height 100}
      :node {:anchors {:left 0 :right 0}
        :height "{{height}}"
        :style card
        :children [{:type label
            :font caps
            :text "{{upper(title)}}"
            :textColor @color.text
            :anchors {:left 14 :top 14}
            :fitText true}
          {:slot true}]}}}
  :strings {:en {:lib.home Overview
      :lib.collections Collections
      :lib.games Games
      :lib.recentFirst "Most recent first"
      :lib.readyOnly "Ready to play only"
      :lib.search Search
      :lib.advanced "Advanced filters"
      :lib.favourites Favourites
      :lib.uncategorised Unsorted
      :lib.whatsNew "Latest news"
      :lib.whatsNewSettings "Latest news settings"
      :lib.recent "Jump back in"
      :lib.allGames "All Games"
      :lib.sortBy "SORT BY"
      :sort.name Alphabetical
      :sort.playtime "Hours Played"
      :sort.recent "Last Played"
      :status.running Running
      :status.update "Update Queued"
      :status.downloading Downloading
      :status.button.ready Play
      :status.button.running Stop
      :status.button.update Update
      :status.button.downloading Pause
      :status.button.none Install
      :time.today Today
      :time.yesterday Yesterday
      :time.thisWeek "This Week"
      :time.lastWeek "Last Week"
      :time.earlier Earlier
      :time.never Never
      :time.daysAgo "{n, plural, =0 {Today} one {Yesterday} other {{n} days ago}}"
      :news.update Update
      :news.event Event
      :news.news News
      :game.hours "{n} hours"
      :game.hoursTotal "Played {n} hours"
      :game.lastPlayed "Last Played"
      :game.playTime "Play Time"
      :game.achievements Achievements
      :game.cloud "Cloud Status"
      :game.cloudOk "Up to date"
      :game.cloudNone "Not installed"
      :game.manage Manage
      :game.info "Game info"
      :game.favourite "Add to Favourites"
      :game.unfavourite "Remove from Favourites"
      :game.play "Play {title}"
      :game.playSafe "Play in Safe Mode"
      :game.launchOptions "Launch Options…"
      :game.developer Developer
      :game.publisher Publisher
      :game.release "Release date"
      :game.section.store "Store Page"
      :game.section.dlc DLC
      :game.section.hub "Player hub"
      :game.section.discussions Discussions
      :game.section.guides Guides
      :game.section.support Support
      :game.activity Activity
      :game.say "Say something about this game to your friends…"
      :game.noActivity "Nothing new here yet."
      :game.friendsWhoPlay "Friends Who Play"
      :game.friendsCount "{n, plural, =0 {No friends play this yet} one {1 friend has played recently} other {{n} friends have played recently}}"
      :game.unlocked "You've unlocked {got}/{total} ({percent}%)"
      :game.screenshots Screenshots
      :game.controller Controller
      :game.notes Notes
      :game.writeNote "Write a note"
      :lib.collapseAll "Collapse every collection"
      :lib.expandAll "Expand every collection"
      :lib.favOnly "Favourites only"
      :lib.clearFilters "Clear filters"
      :lib.showNews "Show the latest news"
      :note.title "Your note"
      :dialog.save Save
      :note.help "Notes only you see, about this game."
      :controller.full "Full controller support"
      :controller.partial "Partial controller support"
      :controller.none "Keyboard and mouse"
      :activity.achievement "unlocked {text}"
      :activity.screenshot "shared a screenshot: {text}"
      :activity.played "{text}"
      :activity.review "{text}"}
    :es {:lib.home Resumen
      :lib.collections Colecciones
      :lib.games Juegos
      :lib.recentFirst "Más recientes primero"
      :lib.readyOnly "Solo listos para jugar"
      :lib.search Buscar
      :lib.advanced "Filtros avanzados"
      :lib.favourites Favoritos
      :lib.uncategorised "Sin ordenar"
      :lib.whatsNew "Últimas noticias"
      :lib.whatsNewSettings "Ajustes de últimas noticias"
      :lib.recent "Continuar jugando"
      :lib.allGames "Todos los juegos"
      :lib.sortBy "ORDENAR POR"
      :sort.name "Alfabético"
      :sort.playtime "Horas jugadas"
      :sort.recent "Última partida"
      :status.running "En ejecución"
      :status.update "Actualización en cola"
      :status.downloading Descargando
      :status.button.ready Jugar
      :status.button.running Detener
      :status.button.update Actualizar
      :status.button.downloading Pausar
      :status.button.none Instalar
      :time.today Hoy
      :time.yesterday Ayer
      :time.thisWeek "Esta semana"
      :time.lastWeek "La semana pasada"
      :time.earlier Antes
      :time.never Nunca
      :time.daysAgo "{n, plural, =0 {Hoy} one {Ayer} other {Hace {n} días}}"
      :news.update "Actualización"
      :news.event Evento
      :news.news Noticias
      :game.hours "{n} horas"
      :game.hoursTotal "{n} horas jugadas"
      :game.lastPlayed "Última partida"
      :game.playTime "Tiempo de juego"
      :game.achievements Logros
      :game.cloud "Estado de la nube"
      :game.cloudOk Actualizado
      :game.cloudNone "No instalado"
      :game.manage Gestionar
      :game.info "Información del juego"
      :game.favourite "Añadir a favoritos"
      :game.unfavourite "Quitar de favoritos"
      :game.play "Jugar a {title}"
      :game.playSafe "Jugar en modo seguro"
      :game.launchOptions "Opciones de inicio…"
      :game.developer Desarrollador
      :game.publisher Distribuidor
      :game.release "Fecha de lanzamiento"
      :game.section.store "Página de la tienda"
      :game.section.dlc DLC
      :game.section.hub "Espacio de jugadores"
      :game.section.discussions Debates
      :game.section.guides "Guías"
      :game.section.support Soporte
      :game.activity Actividad
      :game.say "Cuéntales algo a tus amigos sobre este juego…"
      :game.noActivity "Aún no hay novedades."
      :game.friendsWhoPlay "Amigos que juegan"
      :game.friendsCount "{n, plural, =0 {Ningún amigo juega a esto todavía} one {1 amigo ha jugado recientemente} other {{n} amigos han jugado recientemente}}"
      :game.unlocked "Has desbloqueado {got}/{total} ({percent}%)"
      :game.screenshots "Capturas de pantalla"
      :game.controller Mando
      :game.notes Notas
      :game.writeNote "Escribir una nota"
      :lib.collapseAll "Contraer todas las colecciones"
      :lib.expandAll "Desplegar todas las colecciones"
      :lib.favOnly "Solo favoritos"
      :lib.clearFilters "Quitar filtros"
      :lib.showNews "Mostrar las últimas noticias"
      :note.title "Tu nota"
      :dialog.save Guardar
      :note.help "Notas sobre este juego que solo ves tú."
      :controller.full "Compatibilidad total con mando"
      :controller.partial "Compatibilidad parcial con mando"
      :controller.none "Teclado y ratón"
      :activity.achievement "ha desbloqueado {text}"
      :activity.screenshot "ha compartido una captura de pantalla: {text}"
      :activity.played "{text}"
      :activity.review "{text}"}
    :fr {:lib.home "Vue d'ensemble"
      :lib.collections Collections
      :lib.games Jeux
      :lib.recentFirst "Plus récents d'abord"
      :lib.readyOnly "Prêts à jouer uniquement"
      :lib.search Rechercher
      :lib.advanced "Filtres avancés"
      :lib.favourites Favoris
      :lib.uncategorised "Non classés"
      :lib.whatsNew "Dernières nouvelles"
      :lib.whatsNewSettings "Paramètres des dernières nouvelles"
      :lib.recent Reprendre
      :lib.allGames "Tous les jeux"
      :lib.sortBy "TRIER PAR"
      :sort.name "Alphabétique"
      :sort.playtime "Heures de jeu"
      :sort.recent "Dernière partie"
      :status.running "En cours d'exécution"
      :status.update "Mise à jour en attente"
      :status.downloading "Téléchargement"
      :status.button.ready Jouer
      :status.button.running "Arrêter"
      :status.button.update "Mettre à jour"
      :status.button.downloading Suspendre
      :status.button.none Installer
      :time.today "Aujourd'hui"
      :time.yesterday Hier
      :time.thisWeek "Cette semaine"
      :time.lastWeek "La semaine dernière"
      :time.earlier "Plus tôt"
      :time.never Jamais
      :time.daysAgo "{n, plural, =0 {Aujourd'hui} one {Hier} other {Il y a {n} jours}}"
      :news.update "Mise à jour"
      :news.event "Événement"
      :news.news "Actualités"
      :game.hours "{n} heures"
      :game.hoursTotal "{n} heures de jeu"
      :game.lastPlayed "Dernière partie"
      :game.playTime "Temps de jeu"
      :game.achievements "Succès"
      :game.cloud "État du cloud"
      :game.cloudOk "À jour"
      :game.cloudNone "Non installé"
      :game.manage "Gérer"
      :game.info "Infos sur le jeu"
      :game.favourite "Ajouter aux favoris"
      :game.unfavourite "Retirer des favoris"
      :game.play "Jouer à {title}"
      :game.playSafe "Jouer en mode sans échec"
      :game.launchOptions "Options de lancement…"
      :game.developer "Développeur"
      :game.publisher "Éditeur"
      :game.release "Date de sortie"
      :game.section.store "Page de la boutique"
      :game.section.dlc DLC
      :game.section.hub "Espace joueurs"
      :game.section.discussions Discussions
      :game.section.guides Guides
      :game.section.support Assistance
      :game.activity "Activité"
      :game.say "Dites quelque chose à vos amis à propos de ce jeu…"
      :game.noActivity "Rien de nouveau pour l'instant."
      :game.friendsWhoPlay "Amis qui y jouent"
      :game.friendsCount "{n, plural, =0 {Aucun ami n'y joue pour l'instant} one {1 ami y a joué récemment} other {{n} amis y ont joué récemment}}"
      :game.unlocked "Vous avez débloqué {got}/{total} ({percent} %)"
      :game.screenshots "Captures d'écran"
      :game.controller Manette
      :game.notes Notes
      :game.writeNote "Écrire une note"
      :lib.collapseAll "Replier toutes les collections"
      :lib.expandAll "Déplier toutes les collections"
      :lib.favOnly "Favoris uniquement"
      :lib.clearFilters "Effacer les filtres"
      :lib.showNews "Afficher les dernières nouvelles"
      :note.title "Votre note"
      :dialog.save Enregistrer
      :note.help "Des notes sur ce jeu, visibles par vous seul."
      :controller.full "Prise en charge complète de la manette"
      :controller.partial "Prise en charge partielle de la manette"
      :controller.none "Clavier et souris"
      :activity.achievement "a débloqué {text}"
      :activity.screenshot "a partagé une capture d'écran : {text}"
      :activity.played "{text}"
      :activity.review "{text}"}}}
