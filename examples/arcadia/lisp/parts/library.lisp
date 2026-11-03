;;; library.lisp --- Arcadia's library: the game list on the left (home, a filter, search, collections that fold, each game in its play-state colour), the home (what's new, recent games by day, a shelf per collection and every game sorted) and a page per game (the banner with its logo, play and the stats, the sections, the activity and the side panels).

;;; Definitions

(defstate selected starfall)

(defstate query "")

(defstate readyOnly false)

(defstate recentFirst false)

(defstate favOnly false)

(defstate filterOpen false)

(defstate newsOpen false)

(defstate showNews true)

(defstate note {:open false :id nil :text ""})

(defstate collapsed {})

(defstate running nil)

(defstate playOpen false)

(defstate sortOpen false)

(defstate infoOpen false)

(defstate post "")

(defstate collections
  [{:id coop :title "Co-op Nights" :games [ember pebble neon]}
   {:id cozy :title "Cozy Sundays" :games [tidebreaker mossheart driftwood lumen]}])

(defcomputed libList
  "{{sort(filter(computed.owned, (!state.readyOnly || it.installed) && (!state.favOnly || it.favourite) && (empty(state.query) || fuzzy(state.query, it.title) != null)), state.recentFirst ? it.lastPlayed ?? 0 : lower(it.title), state.recentFirst ? 'desc' : 'asc')}}")

(defcomputed collected (split (join "," (map (fn [it] (join "," it.games)) state.collections)) ","))

(defcomputed sections
  (concat [{:id "favourites"
            :title (t "lib.favourites")
            :ids (map (fn [it] it.id) (filter (fn [it] it.favourite) computed.owned))}]
          state.collections
          [{:id "uncategorised"
            :title (t "lib.uncategorised")
            :ids (map (fn [it] it.id)
                      (filter (fn [it] (and (not it.favourite) (not (contains computed.collected it.id))))
                              computed.owned))}]))

(defcomputed recent
  (slice (sort-by (fn [it] it.lastPlayed) > (filter (fn [it] (some? it.lastPlayed)) computed.owned)) 0 10))

(defcomputed recentGroups
  (filter (fn [it] (> (count it.games) 0))
          [{:label (t "time.today")
            :games (filter (fn [it] (< (- state.today it.lastPlayed) 86400000)) computed.recent)}
           {:label (t "time.yesterday")
            :games (filter (fn [it]
                             (and (>= (- state.today it.lastPlayed) 86400000)
                                  (< (- state.today it.lastPlayed) 172800000)))
                           computed.recent)}
           {:label (t "time.thisWeek")
            :games (filter (fn [it]
                             (and (>= (- state.today it.lastPlayed) 172800000)
                                  (< (- state.today it.lastPlayed) 604800000)))
                           computed.recent)}
           {:label (t "time.lastWeek")
            :games (filter (fn [it]
                             (and (>= (- state.today it.lastPlayed) 604800000)
                                  (< (- state.today it.lastPlayed) 1209600000)))
                           computed.recent)}
           {:label (t "time.earlier")
            :games (filter (fn [it] (>= (- state.today it.lastPlayed) 1209600000)) computed.recent)}]))

(defcomputed allGames
  (cond (= settings.library.sortBy "playtime") (sort-by (fn [it] it.hours) > computed.owned)
        (= settings.library.sortBy "recent") (sort-by (fn [it] (or-else it.lastPlayed 0)) > computed.owned)
        :else (sort-by (fn [it] (lower-case it.title)) computed.owned)))

(defcomputed lastPlayed
  (cond (nil? computed.game.lastPlayed) (t "time.never")
        (< (- state.today computed.game.lastPlayed) 86400000) (t "time.today")
        (< (- state.today computed.game.lastPlayed) 172800000) (t "time.yesterday")
        :else (formatDate computed.game.lastPlayed "d MMM" "utc")))

(defstyle caps (label :font caps :textColor @color.muted :fitText true))

(defstyle iconButton
  (label :text ""
    :fitText true
    :padding 7
    :radius 3
    :cursor hand
    :focusable true
    :role button
    :textColor @color.muted
    :hover {:color @color.hover :textColor @color.bright}))

(defstyle sectionLink
  (label :fitText true
    :padding 10
    :cursor hand
    :focusable true
    :role link
    :textColor @color.text
    :hover {:textColor @color.bright}))

(defstyle card {:color @color.panel :radius 12})

(defcomponent noteDialog "A note about a game, kept with it in the library."
  {}
  `(node :use dialog
     :with {:open state.note.open
            :width 460
            :height 240
            :title ~(t "note.title")
            :message ~(t "note.help")
            :buttons [{:title ~(t "dialog.save") :run saveNote :default true} {:title ~(t "dialog.cancel")}]}
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
       :label ~(t "note.title")
       :autofocus true)))

(defcomponent libraryPage "The library: the game list on the left, the home or a game's page on the right."
  {}
  `(row :fill true
     (node#library-sidebar :use librarySidebar)
     (node#library-main :grow 1
       :anchors {:top 0 :bottom 0}
       :gradient {:kind linear :stops [@color.main @color.page]}
       (node#library-home :when ~(= computed.here.page "home") :use libraryHome)
       (node#game-page :when ~(and (= computed.here.page "game") (some? computed.game)) :use gamePage)
       (node#hub-page :when ~(and (= computed.here.page "hub") (some? computed.game)) :use hubPage))))

(defcomponent librarySidebar
  "Home, the games filter with recent-first and ready-to-play, search, and the games by collection."
  {}
  `(column :width 272 :anchors {:top 0 :bottom 0} :color @color.side
     ~(library-sidebar-row)
     ~(library-sidebar-row-view)
     ~(library-sidebar-row-2)
     (column#library-list :grow 1
       :anchors {:left 0 :right 0}
       :scroll vertical
       :arrows vertical
       :role list
       :label ~(t "lib.games")
       ~(repeat [section computed.sections :key section.id] (library-list-column section)))))

(defcomponent gameRow
  "A game in the list: its cover as an icon and its name in its play-state colour, with what it is doing."
  {:game {}}
  `(row :height 28
     :anchors {:left 0 :right 0}
     :gap 8
     :alignItems center
     :focusable true
     :role listitem
     :cursor hand
     :label ~game.title
     :selected ~(and (= computed.here.page "game") (= computed.here.id game.id))
     :color ~(if (and (= computed.here.page "game") (= computed.here.id game.id)) "@color.select" "transparent")
     :hover {:color @color.hover}
     :onClick ~(handler openGame game.id)
     :onDoubleClick ~(handler primary game)
     :onContextMenu ~(handler openGameMenu game.id)
     (panel :width 18 :height 1 :color transparent)
     (image :file ~(str "../assets/art/" (or-else game.art game.id) "/cover.svg")
       :width 18
       :height 18
       :fit cover
       :radius 2)
     (label :grow 1
       :height 18
       :font small
       :runs [{:text ~game.title}
              {:text ~(cond (= game.status "running") (str " – " (t "status.running"))
                            (= game.status "update") (str " – " (t "status.update"))
                            (= game.status "downloading") (str " – " (t "status.downloading"))
                            :else "")}]
       :textColor ~(cond (= game.status "running") "@color.ingame"
                         (or (= game.status "update") (= game.status "downloading")) "@color.accent"
                         game.installed "@color.text"
                         :else "@color.dim"))))

(defcomponent capsule
  "A game's cover, 2:3, that lifts and glows under the pointer; dimmed when the game isn't installed."
  {:game {} :width 150}
  `(node :width ~width
     :height ~(* width 1.5)
     :radius 12
     :color @color.raised
     :focusable true
     :role button
     :label ~game.title
     :cursor hand
     :tooltip ~game.title
     :shadow {:y 2 :blur 6 :color "#00000060"}
     :hover {:scale 1.04 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
     :focused {:scale 1.04 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
     :onClick ~(handler openGame game.id)
     :onContextMenu ~(handler openGameMenu game.id)
     (image :file ~(str "../assets/art/" (or-else game.art game.id) "/cover.svg") :fill true :fit cover :radius 12)
     (node :when ~(not game.installed) :fill true :color "#100c1480" :radius 12)))

(defcomponent shelfHeader "The shelfHeader component."
  {:title "" :count nil}
  `(row :height 34 :anchors {:left 0 :right 0} :alignItems center :gap 8
     (label :font caps :text ~(upper-case title) :textColor @color.text :fitText true)
     (label :when ~(some? count) :font caps :text ~(str "(" count ")") :textColor @color.muted :fitText true)
     (panel :grow 1 :height 1 :color transparent)
     (node :slot true)))

(defcomponent libraryHome
  "The library's home: what's new, recent games grouped by day, a shelf per collection and every game."
  {}
  `(column#home-scroll :fill true :scroll vertical ~(home-content)))

(defcomponent bigCapsule "The latest game played, wide: its banner and logo, play time and a play button."
  {:game {}}
  `(node :width 345
     :height 225
     :radius 12
     :color @color.raised
     :focusable true
     :role button
     :cursor hand
     :label ~game.title
     :shadow {:y 2 :blur 6 :color "#00000060"}
     :hover {:scale 1.02 :shadow {:y 6 :blur 18 :color "#f5a52455"}}
     :onClick ~(handler openGame game.id)
     (image :file ~(str "../assets/art/" (or-else game.art game.id) "/hero.svg") :fill true :fit cover :radius 12)
     (node :fill true
       :radius 12
       :color transparent
       :gradient {:kind linear :stops [{:at 0.35 :color "#00000000"} {:at 1 :color "#000000d0"}]})
     (label :font heading :text ~game.title :textColor ~game.logo :anchors {:left 14 :bottom 40} :fitText true)
     (label :font small
       :anchors {:left 14 :bottom 18}
       :fitText true
       :textColor "#e2d8e6"
       :text ~(t "game.hoursTotal" {:n (formatNumber game.hours 0 1)}))
     (label :icon play
       :text ""
       :font button
       :width 40
       :height 40
       :align center
       :radius 999
       :anchors {:right 14 :bottom 14}
       :textColor "#ffffff"
       :gradient {:kind linear :angle 90 :stops [@color.play @color.playEnd]})))

(defcomponent gamePage
  "A game's page: the banner and logo, play and the stats, the sections, the activity and the side panels."
  {}
  `(column#game-scroll :fill true :scroll vertical :let {:g ~computed.game}
     (node#hero :height 310 :anchors {:left 0 :right 0}
       (image#hero-image :file ~(str "../assets/art/" (or-else g.art g.id) "/hero.svg") :fill true :fit cover)
       (node :fill true
         :color transparent
         :gradient {:kind linear :stops [{:at 0.55 :color "#00000000"} {:at 1 :color "#0d0a10c0"}]})
       (label :font logo :text ~g.title :textColor "#000000a0" :anchors {:left 42 :bottom 34} :fitText true)
       (label#details-title :font logo
         :text ~g.title
         :textColor ~g.logo
         :anchors {:left 40 :bottom 36}
         :fitText true
         :role heading))
     ~(play-bar)
     (row#game-sections :height 46 :anchors {:left 24 :right 24} :alignItems center :arrows horizontal
       ~(repeat [s (if g.local ["support"] ["store" "dlc" "hub" "discussions" "guides" "support"])]
          `(node :id ~(str "section-link-" s)
             :style sectionLink
             :text ~(t (str "game.section." s))
             :onClick [{:if ~(= s "store")
                        :then [~(handler openStore g.id)]
                        :else [{:if ~(= s "dlc")
                                :then [~(handler openProperties g.id "dlc")]
                                :else [{:if ~(= s "support")
                                        :then [~(handler showInfo "support")]
                                        :else [~(handler openHub g.id s)]}]}]}])))
     (panel :height 1 :anchors {:left 32 :right 32} :color @color.border)
     (row#game-columns :anchors {:left 32 :right 32}
       :gap 24
       :height ~(max 700
                     (+ 150
                        (* (count (filter (fn [it] (= it.game g.id)) data.activity)) 96)
                        (* (count (filter (fn [it] (= it.game g.id)) data.news)) 120)))
       ~(activity-view)
       ~(game-side))))

(defcomponent stat "The stat component."
  {:title "" :value "" :icon ""}
  `(column :height 40 :gap 4 :width ~(max 90 (+ (* (count value) 8) 24))
     (label :font caps :text ~(upper-case title) :textColor @color.muted :height 15 :anchors {:left 0 :right 0})
     (label :font small :icon ~icon :text ~value :textColor @color.bright :height 18 :anchors {:left 0 :right 0})))

(defcomponent sideCard "The sideCard component."
  {:title "" :height 100}
  `(node :anchors {:left 0 :right 0} :height ~height :style card
     (label :font caps :text ~(upper-case title) :textColor @color.text :anchors {:left 14 :top 14} :fitText true)
     (node :slot true)))

;;; Views

(defview library-sidebar-row "#library-sidebar-row." []
  `(row :height 40 :anchors {:left 0 :right 0} :color @color.sideTop :alignItems center
     (label#library-home-button :grow 1
       :height 40
       :font strong
       :icon layout-dashboard
       :text ~(t "lib.home")
       :padding 14
       :cursor hand
       :focusable true
       :role button
       :textColor ~(if (= computed.here.page "home") "@color.bright" "@color.text")
       :color ~(if (= computed.here.page "home") "@color.hover" "transparent")
       :hover {:color @color.hover}
       :onClick libraryHome)
     (node#library-collections :style iconButton
       :icon ~(if (some (fn [it] (not (or-else (get state.collapsed it.id) false))) computed.sections)
                "list-collapse"
                "list")
       :tooltip ~(t (if (some (fn [it] (not (or-else (get state.collapsed it.id) false))) computed.sections)
                      "lib.collapseAll"
                      "lib.expandAll"))
       :onClick toggleCollections)))

(defview library-sidebar-row-view "#library-sidebar-row-view." []
  `(row :height 36 :anchors {:left 0 :right 0} :alignItems center
     (label#library-kind :font caps
       :text ~(t "lib.games")
       :icon chevron-down
       :textColor @color.muted
       :grow 1
       :padding 14
       :height 36)
     (node#library-recent :style iconButton
       :icon clock-3
       :tooltip ~(t "lib.recentFirst")
       :textColor ~(if state.recentFirst "@color.accent" "@color.muted")
       :selected ~state.recentFirst
       :onClick {:toggle state.recentFirst})
     (node#library-ready :style iconButton
       :icon circle-play
       :tooltip ~(t "lib.readyOnly")
       :textColor ~(if state.readyOnly "@color.accent" "@color.muted")
       :selected ~state.readyOnly
       :onClick {:toggle state.readyOnly})
     (panel :width 6 :height 1 :color transparent)))

(defview library-sidebar-row-2 "#library-sidebar-row-2." []
  `(row :height 34 :anchors {:left 0 :right 0} :alignItems center
     (panel :width 10 :height 1 :color transparent)
     (textarea#library-search :grow 1
       :height 28
       :padding 8
       :bind state.query
       :placeholder ~(t "lib.search")
       :placeholderColor @color.dim
       :textColor @color.text
       :color @color.field
       :radius 3
       :label ~(t "lib.search")
       :focused {:borderWidth 1 :borderColor @color.accent}
       (label :icon search :text "" :textColor @color.dim :fitText true :anchors {:right 8 :top 6}))
     (node#library-filter :style iconButton
       :icon sliders-horizontal
       :tooltip ~(t "lib.advanced")
       :textColor ~(if (or state.readyOnly state.recentFirst state.favOnly) "@color.accent" "@color.muted")
       :isOpen ~state.filterOpen
       :onClick {:toggle state.filterOpen}
       (node :use menu
         :with {:open state.filterOpen
                :width 230
                :items [{:title ~(t "lib.readyOnly")
                         :checked ~state.readyOnly
                         :run toggleFilter
                         :with {:key readyOnly}}
                        {:title ~(t "lib.favOnly") :checked ~state.favOnly :run toggleFilter :with {:key favOnly}}
                        {:title ~(t "lib.recentFirst")
                         :checked ~state.recentFirst
                         :run toggleFilter
                         :with {:key recentFirst}}
                        {:separator true}
                        {:title ~(t "lib.clearFilters") :run clearFilters}]}))
     (panel :width 6 :height 1 :color transparent)))

(defview library-list-column "#library-list-column." [section]
  `(column :let {:games ~(filter (fn [it] (contains (or-else section.ids section.games) it.id)) computed.libList)
                 :open ~(not (or-else (get state.collapsed section.id) false))}
     :anchors {:left 0 :right 0}
     :height ~(+ 30 (if open (* (count games) 28) 0))
     :when ~(or (> (count games) 0) (empty? state.query))
     (label :id ~(str "section-" section.id)
       :font caps
       :height 30
       :anchors {:left 0 :right 0}
       :padding 12
       :icon ~(if open "chevron-down" "chevron-right")
       :mirror true
       :text ~(str (upper-case section.title) " (" (count games) ")")
       :textColor @color.muted
       :cursor hand
       :focusable true
       :role button
       :isOpen ~open
       :hover {:textColor @color.bright}
       :onClick {:set ~(str "state.collapsed." section.id) :to ~open})
     ~(repeat [g games :key (+ section.id g.id)]
        `(node :id ~(str "row-" section.id "-" g.id) :when ~open :use gameRow :with {:game ~g}))))

(defview library-view "#." [n i]
  `(column :id ~(str "news-" i)
     :let {:g ~(find (fn [it] (= it.id n.game)) data.games)}
     :width 232
     :height 216
     :style card
     :focusable true
     :role link
     :label ~n.title
     :cursor hand
     :hover {:color @color.hover :shadow {:y 4 :blur 14 :color "#00000080"}}
     :onClick ~(handler openGame n.game)
     (image :file ~(str "../assets/art/" n.game "/hero.svg")
       :anchors {:left 0 :right 0}
       :height 120
       :fit cover
       :radius {:topLeft 8 :topRight 8})
     (label :font caps
       :height 26
       :anchors {:left 0 :right 0}
       :padding 10
       :runs [{:text ~(upper-case (t (str "news." n.kind))) :color @color.link}
              {:text ~(str " · " (t "time.daysAgo" {:n n.ago})) :color @color.muted}])
     (label :font strong :text ~n.title :textColor @color.bright :wrap true :anchors {:left 10 :right 10} :height 40)
     (label :font small :text ~g.title :textColor @color.muted :anchors {:left 10 :right 10} :height 18)))

(defview recent-shelf-column "#recent-shelf-column." [group gi]
  `(column :gap 8
     :height 256
     :width ~(if (= gi 0) (+ 345 (* (- (count group.games) 1) 164)) (- (* (count group.games) 164) 14))
     (label :font small :text ~group.label :textColor @color.muted :height 18 :anchors {:left 0 :right 0})
     (row :gap 14 :height 225 :anchors {:left 0 :right 0} :arrows horizontal
       ~(repeat [g group.games :index i]
          `(stack :children [(node :id ~(str "recent-big-" g.id)
                               :when ~(and (= gi 0) (= i 0))
                               :use bigCapsule
                               :with {:game ~g})
                             (node :id ~(str "recent-" g.id)
                               :when ~(not (and (= gi 0) (= i 0)))
                               :use capsule
                               :with {:game ~g})]
             :width ~(if (and (= gi 0) (= i 0)) 345 150)
             :height 225)))))

(defview sort-button "#sort-button." []
  `(label#sort-button :font small
     :text ~(t (str "sort." settings.library.sortBy))
     :icon chevron-down
     :padding 8
     :fitText true
     :color @color.raised
     :radius 3
     :textColor @color.text
     :cursor hand
     :focusable true
     :role button
     :isOpen ~state.sortOpen
     :hover {:color @color.hover}
     :onClick {:toggle state.sortOpen}
     (node :use menu
       :with {:open state.sortOpen
              :width 180
              :placement below-end
              :items [{:title ~(t "sort.name")
                       :checked ~(= settings.library.sortBy "name")
                       :run setSort
                       :with {:by name}}
                      {:title ~(t "sort.playtime")
                       :checked ~(= settings.library.sortBy "playtime")
                       :run setSort
                       :with {:by playtime}}
                      {:title ~(t "sort.recent")
                       :checked ~(= settings.library.sortBy "recent")
                       :run setSort
                       :with {:by recent}}]})))

(defview home-content "#home-content." []
  `(column#home-content :anchors {:left 28 :right 28}
     :gap 6
     :height ~(+ 412
                 (* 286
                    (count (filter (fn [it]
                                     (and (not= it.id "uncategorised") (> (count (or-else it.ids it.games)) 0)))
                                   computed.sections)))
                 (* 239
                    (ceil (/ (count computed.owned)
                             (max 1 (floor (/ (- (or-else (get-in nodes ["home-scroll" :width]) 1008) 42) 164)))))))
     (panel :height 10 :width 1 :color transparent)
     ~(news-header)
     (row#news-shelf :when ~state.showNews
       :height 222
       :anchors {:left 0 :right 0}
       :gap 14
       :scroll horizontal
       :arrows horizontal
       ~(repeat [n data.news :index i] (library-view n i)))
     (node#recent-header :use shelfHeader :with {:title ~(t "lib.recent")})
     (row#recent-shelf :height 262 :anchors {:left 0 :right 0} :gap 18 :scroll horizontal
       ~(repeat [group computed.recentGroups :index gi] (recent-shelf-column group gi)))
     ~(repeat [section (slice computed.sections 0 (- (count computed.sections) 1)) :key section.id]
        `(column :let {:games ~(filter (fn [it] (contains (or-else section.ids section.games) it.id))
                                       computed.owned)}
           :anchors {:left 0 :right 0}
           :height 280
           :gap 6
           :when ~(> (count games) 0)
           (node :use shelfHeader :with {:title ~section.title :count ~(count games)})
           (row :height 236 :anchors {:left 0 :right 0} :gap 14 :scroll horizontal :arrows horizontal
             ~(repeat [g games] `(node :id ~(str "shelf-" section.id "-" g.id) :use capsule :with {:game ~g})))))
     (node#all-header :use shelfHeader :with {:title ~(t "lib.allGames") :count ~(count computed.owned)}
       (label :font caps :text ~(t "lib.sortBy") :textColor @color.muted :fitText true)
       ~(sort-button))
     ~(all-grid)
     (panel :height 24 :width 1 :color transparent)))

(defview all-grid "#all-grid, in home-content." []
  `(row#all-grid :wrap true
     :gap 14
     :anchors {:left 0 :right 0}
     :height ~(* (ceil (/ (count computed.owned)
                          (max 1 (floor (/ (- (or-else (get-in nodes ["home-scroll" :width]) 1008) 42) 164)))))
                 239)
     ~(repeat [g computed.allGames :key g.id] `(node :id ~(str "all-" g.id) :use capsule :with {:game ~g}))))

(defview news-header "#news-header, in home-content." []
  `(node#news-header :use shelfHeader :with {:title ~(t "lib.whatsNew")}
     (node#news-settings :style iconButton
       :icon settings
       :tooltip ~(t "lib.whatsNewSettings")
       :isOpen ~state.newsOpen
       :onClick {:toggle state.newsOpen}
       (node :use menu
         :with {:open state.newsOpen
                :width 220
                :placement below-end
                :items [{:title ~(t "lib.showNews")
                         :checked ~state.showNews
                         :run toggleFilter
                         :with {:key showNews}}]}))))

(defview play-button "#play-button." []
  `(label#play-button :grow 1
     :height 48
     :font button
     :align center
     :icon ~(cond (= g.status "ready") "play"
                  (= g.status "running") "square"
                  (= g.status "downloading") "pause"
                  :else "download")
     :text ~(t (str "status.button." g.status))
     :textColor @color.onAccent
     :focusable true
     :role button
     :cursor hand
     :radius {:topLeft 24 :bottomLeft 24}
     :color ~(if (= g.status "ready") "@color.play" "@color.warm")
     :hover {:color ~(if (= g.status "ready") "@color.playEnd" "@color.warmEnd")}
     :onClick ~(handler primary g)))

(defview play-more "#play-more." []
  `(label#play-more :width 34
     :height 48
     :icon chevron-down
     :text ""
     :align center
     :textColor @color.onAccent
     :focusable true
     :role button
     :label ~(t "game.launchOptions")
     :cursor hand
     :color ~(if (= g.status "ready") "@color.play" "@color.warm")
     :hover {:color ~(if (= g.status "ready") "@color.playEnd" "@color.warmEnd")}
     :radius {:topRight 24 :bottomRight 24}
     :borderWidth 1
     :borderColor @color.shade
     :isOpen ~state.playOpen
     :onClick {:toggle state.playOpen}
     (node :use menu
       :with {:open state.playOpen
              :width 240
              :items [{:title ~(t "game.play" {:title g.title})
                       :enabled ~(= g.status "ready")
                       :run play
                       :with {:id ~g.id}}
                      {:title ~(t "game.playSafe")
                       :enabled ~(= g.status "ready")
                       :run play
                       :with {:id ~g.id :safe true}}
                      {:separator true}
                      {:title ~(t "game.launchOptions") :run openProperties :with {:id ~g.id :tab general}}]})))

(defview info-card "#info-card." []
  `(column#info-card :overlay {:placement below-end :open state.infoOpen :dismiss outside}
     :width 340
     :height 196
     :color @kit.surface
     :radius 4
     :gap 6
     :role dialog
     :label ~g.title
     :shadow {:y 8 :blur 24 :color "#00000090"}
     (label :font strong
       :text ~g.title
       :textColor @color.bright
       :anchors {:left 16 :right 16}
       :height 34
       :align {:vertical end})
     (label#details-blurb :text ~g.blurb :textColor @color.text :wrap true :anchors {:left 16 :right 16} :height 60)
     (label :font small
       :text ~(str (t "game.developer") ": " g.developer)
       :textColor @color.muted
       :anchors {:left 16 :right 16}
       :height 18)
     (label :font small
       :text ~(str (t "game.publisher") ": " g.publisher)
       :textColor @color.muted
       :anchors {:left 16 :right 16}
       :height 18)
     (label :font small
       :text ~(str (t "game.release") ": " g.release)
       :textColor @color.muted
       :anchors {:left 16 :right 16}
       :height 18)))

(defview play-bar "#play-bar." []
  `(row#play-bar :height 80 :anchors {:left 0 :right 0} :alignItems center :gap 26 :color @color.shade
     (panel :width 6 :height 1 :color transparent)
     (row#play-group :height 48 :width ~(if (= g.status "ready") 196 210) :radius 24 ~(play-button) ~(play-more))
     (node#stat-last :use stat :with {:title ~(t "game.lastPlayed") :value ~computed.lastPlayed})
     (node#stat-time :use stat
       :with {:title ~(t "game.playTime") :value ~(t "game.hours" {:n (formatNumber g.hours 0 1)})})
     ~(stat-achievements)
     (node#stat-cloud :use stat
       :with {:title ~(t "game.cloud") :value ~(t (if g.installed "game.cloudOk" "game.cloudNone")) :icon cloud})
     (panel :grow 1 :height 1 :color transparent)
     (node#game-settings :style iconButton
       :font heading
       :icon settings
       :tooltip ~(t "game.manage")
       :label ~(t "game.manage")
       :onClick ~(handler openGameMenu g.id))
     (node#game-info :style iconButton
       :font heading
       :icon info
       :tooltip ~(t "game.info")
       :isOpen ~state.infoOpen
       :onClick {:toggle state.infoOpen}
       ~(info-card))
     (node#game-favourite :style iconButton
       :font heading
       :icon star
       :textColor ~(if g.favourite "#ffc82c" "@color.muted")
       :selected ~g.favourite
       :tooltip ~(t (if g.favourite "game.unfavourite" "game.favourite"))
       :onClick ~(handler toggleFavourite g.id))
     (panel :width 14 :height 1 :color transparent)))

(defview stat-achievements "#stat-achievements, in play-bar." []
  `(column#stat-achievements :width 128 :height 40 :gap 4
     (label :font caps
       :text ~(upper-case (t "game.achievements"))
       :textColor @color.muted
       :height 15
       :anchors {:left 0 :right 0})
     (row :height 18 :gap 8 :alignItems center :anchors {:left 0 :right 0}
       (label :font small :text ~(str g.achievements.0 "/" g.achievements.1) :textColor @color.bright :fitText true)
       (node#achievement-track :grow 1 :height 6 :radius 3 :color @color.raised
         (node :anchors {:left 0 :top 0 :bottom 0}
           :radius 3
           :width ~(str (text (floor (/ (* 100 (or-else g.achievements.0 0)) (max 1 (or-else g.achievements.1 0)))))
                        "%")
           :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]})))))

(defview activity-column "#activity-column." [a]
  `(column :grow 1 :gap 4 :anchors {:top 14 :bottom 10}
     (label :anchors {:left 0 :right 0}
       :height 18
       :runs [{:text ~(str f.name " ") :color @color.online :bold true}
              {:text ~(t (str "activity." a.kind) {:text a.text}) :color @color.text}])
     (label :when ~(some? a.detail)
       :font small
       :text ~a.detail
       :textColor @color.muted
       :anchors {:left 0 :right 0}
       :height 16)
     (image :when ~(= a.kind "screenshot")
       :file ~(str "../assets/art/" (or-else g.art g.id) "/shot" a.shot ".svg")
       :width 160
       :height 90
       :fit cover
       :radius 2)
     (label :font small
       :text ~(t "time.daysAgo" {:n a.ago})
       :textColor @color.dim
       :anchors {:left 0 :right 0}
       :height 16)))

(defview activity-row "#activity-row." [a]
  `(row :let {:f ~(find (fn [it] (= it.id a.who)) data.friends)}
     :height ~(if (= a.kind "screenshot") 168 84)
     :anchors {:left 0 :right 0}
     :style card
     :gap 12
     (image :file ~(str "../assets/art/avatars/" f.avatar ".svg")
       :width 36
       :height 36
       :anchors {:top 14}
       :fit cover
       :radius 2
       :borderWidth 1
       :borderColor ~(cond (= f.presence "ingame") "@color.ingame"
                           (= f.presence "offline") "@color.offline"
                           :else "@color.online"))
     ~(activity-column a)))

(defview activity-view "#activity." []
  `(column#activity :grow 1 :gap 12 :anchors {:top 16 :bottom 0}
     (label :font caps
       :text ~(upper-case (t "game.activity"))
       :textColor @color.text
       :height 22
       :anchors {:left 0 :right 0})
     ~(post-box)
     ~(repeat [n (filter (fn [it] (= it.game g.id)) data.news) :index i]
        `(row :height 108 :anchors {:left 0 :right 0} :style card :gap 14 :alignItems center
           (image :file ~(str "../assets/art/" (or-else g.art g.id) "/hero.svg")
             :width 180
             :height 108
             :fit cover
             :radius {:topLeft 3 :bottomLeft 3})
           (column :grow 1 :height 84 :gap 4
             (label :font caps
               :anchors {:left 0 :right 0}
               :height 16
               :runs [{:text ~(upper-case (t (str "news." n.kind))) :color @color.link}
                      {:text ~(str " · " (t "time.daysAgo" {:n n.ago})) :color @color.muted}])
             (label :font strong :text ~n.title :textColor @color.bright :anchors {:left 0 :right 0} :height 20)
             (label :font small
               :text ~n.text
               :textColor @color.muted
               :wrap true
               :anchors {:left 0 :right 12}
               :height 36))))
     ~(repeat [a (filter (fn [it] (= it.game g.id)) data.activity) :index i] (activity-row a))
     (label :when ~(= (+ (count (filter (fn [it] (= it.game g.id)) data.activity))
                         (count (filter (fn [it] (= it.game g.id)) data.news)))
                      0)
       :text ~(t "game.noActivity")
       :textColor @color.muted
       :height 40
       :anchors {:left 0 :right 0})))

(defview post-box "#post-box, in activity-view." []
  `(textarea#post-box :height 44
     :anchors {:left 0 :right 0}
     :padding 12
     :bind state.post
     :placeholder ~(t "game.say")
     :placeholderColor @color.dim
     :textColor @color.text
     :color @color.panel
     :radius 3
     :label ~(t "game.say")
     :focused {:borderWidth 1 :borderColor @color.accent}))

(defview friends-card-column "#friends-card-column." []
  `(column :let {:who ~(filter (fn [it]
                                 (or (= it.game g.id)
                                     (and (> g.hours 40) (or (= it.presence "online") (= it.avatar "owl")))))
                               data.friends)}
     :anchors {:left 14 :right 14 :top 40}
     :height 70
     :gap 8
     (label :font small
       :text ~(t "game.friendsCount" {:n (count who)})
       :textColor @color.muted
       :anchors {:left 0 :right 0}
       :height 16)
     (row :gap 6 :height 36 :anchors {:left 0 :right 0}
       ~(repeat [f (slice who 0 7)]
          `(image :file ~(str "../assets/art/avatars/" f.avatar ".svg")
             :width 34
             :height 34
             :fit cover
             :radius 2
             :tooltip ~f.name
             :borderWidth 1
             :borderColor ~(if (= f.game g.id) "@color.ingame" "@color.online"))))))

(defview achievements-card-row "#achievements-card-row." []
  `(row :gap 6 :height 44 :anchors {:left 0 :right 0}
     ~(repeat [icon ["trophy" "flame" "gem" "crown" "swords"] :index k]
        `(label :icon ~icon
           :text ""
           :font heading
           :width 42
           :height 44
           :align center
           :radius 3
           :textColor ~(if (< k 4) g.logo "@color.dim")
           :gradient {:kind linear :stops [@color.hover @color.raised]}))
     (label :font strong
       :text ~(str "+" (max 0 (- g.achievements.1 5)))
       :width 44
       :height 44
       :align center
       :textColor @color.muted
       :color @color.raised
       :radius 3)))

(defview achievements-card "#achievements-card." []
  `(node#achievements-card :use sideCard :with {:title ~(t "game.achievements") :height 150}
     (column :anchors {:left 14 :right 14 :top 40} :height 100 :gap 8
       (label :font small
         :textColor @color.muted
         :anchors {:left 0 :right 0}
         :height 16
         :text ~(t "game.unlocked"
                   {:got g.achievements.0
                    :total g.achievements.1
                    :percent (floor (* (/ g.achievements.0 (max 1 g.achievements.1)) 100))}))
       (node#achievement-track-side :height 6 :radius 3 :color @color.raised :anchors {:left 0 :right 0}
         (node :anchors {:left 0 :top 0 :bottom 0}
           :radius 3
           :width ~(str (text (floor (/ (* 100 (or-else g.achievements.0 0)) (max 1 (or-else g.achievements.1 0)))))
                        "%")
           :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}))
       ~(achievements-card-row))))

(defview game-side "#game-side." []
  `(column#game-side :width 312 :gap 14 :anchors {:top 16 :bottom 0}
     (node#friends-card :use sideCard :with {:title ~(t "game.friendsWhoPlay") :height 118} ~(friends-card-column))
     ~(achievements-card)
     (node#screenshots-card :use sideCard :with {:title ~(t "game.screenshots") :height 132}
       (row :gap 6 :anchors {:left 14 :right 14 :top 40} :height 80
         ~(repeat [k [1 2 3]]
            `(image :file ~(str "../assets/art/" (or-else g.art g.id) "/shot" k ".svg")
               :grow 1
               :height 80
               :fit cover
               :radius 2))))
     (node#controller-card :use sideCard :with {:title ~(t "game.controller") :height 74}
       (label :icon gamepad-2
         :text ~(t (str "controller." g.controller))
         :textColor @color.text
         :anchors {:left 14 :right 14 :top 42}
         :height 20))
     (node#notes-card :use sideCard :with {:title ~(t "game.notes") :height 74}
       (label#write-note :icon pencil
         :text ~(if (empty? g.note) (t "game.writeNote") g.note)
         :onClick ~(handler editNote g.id)
         :textColor @color.link
         :cursor hand
         :focusable true
         :role button
         :anchors {:left 14 :top 42}
         :width 200
         :height 20
         :hover {:textColor @color.bright}))))
