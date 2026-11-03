;;; library.lisp --- Arcadia's library: the game list on the left, and the home
;;;
;;; The list has home, a filter, search and collections that fold, each game in its play-state colour. The home has
;;; what's new, recent games by day, a shelf per collection and every game sorted. A game's page is in game.lisp.

;;; What the library keeps

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

;;; The games listed

(defmacro listed
  ;; A macro, not a function: the list it writes stays an expression, worked out at its speed.
  "The owned games the list's filters and search let through."
  []
  `(filter (fn [it] (and (or (not state.readyOnly) it.installed)
                         (or (not state.favOnly) it.favourite)
                         (or (empty? state.query) (not= (fuzzy state.query it.title) nil))))
           computed.owned))

(defcomputed libList "The games of the list: filtered, then by name, or the one played last first."
  (if state.recentFirst
    (sort-by (fn [it] (or-else it.lastPlayed 0)) > (listed))
    (sort-by (fn [it] (lower-case it.title)) (listed))))

(defcomputed collected "Every game id some collection holds."
  (split (join "," (map (fn [it] (join "," it.games)) state.collections)) ","))

(defcomputed sections "The list's sections: the favourites, the collections, and the games in none of them."
  (concat [{:id "favourites"
            :title (t "lib.favourites")
            :ids (map (fn [it] it.id) (filter (fn [it] it.favourite) computed.owned))}]
          state.collections
          [{:id "uncategorised"
            :title (t "lib.uncategorised")
            :ids (map (fn [it] it.id) (filter (fn [it] (and (not it.favourite) (not (contains computed.collected it.id)))) computed.owned))}]))

(defcomputed recent "The ten games played last."
  (slice (sort-by (fn [it] it.lastPlayed) > (filter (fn [it] (some? it.lastPlayed)) computed.owned)) 0 10))

(defmacro played-between
  ;; A macro: the list stays an expression of `it`, the game.
  "The recent games last played between `from` and `to` milliseconds ago."
  [from to]
  `(filter (fn [it] (and (>= (- state.today it.lastPlayed) ~from) (< (- state.today it.lastPlayed) ~to))) computed.recent))

(defcomputed recentGroups "The recent games by when they were played: today, yesterday, this week, last week, earlier."
  (filter (fn [it] (> (count it.games) 0))
          [{:label (t "time.today") :games (filter (fn [it] (< (- state.today it.lastPlayed) 86400000)) computed.recent)}
           {:label (t "time.yesterday") :games (played-between 86400000 172800000)}
           {:label (t "time.thisWeek") :games (played-between 172800000 604800000)}
           {:label (t "time.lastWeek") :games (played-between 604800000 1209600000)}
           {:label (t "time.earlier") :games (filter (fn [it] (>= (- state.today it.lastPlayed) 1209600000)) computed.recent)}]))

(defcomputed allGames "Every owned game, in the order Settings chose: by name, by hours, or the one played last first."
  (cond (= settings.library.sortBy "playtime") (sort-by (fn [it] it.hours) > computed.owned)
        (= settings.library.sortBy "recent") (sort-by (fn [it] (or-else it.lastPlayed 0)) > computed.owned)
        :else (sort-by (fn [it] (lower-case it.title)) computed.owned)))

;;; The library's looks

(defstyle caps "Small capitals, for a heading." (label :font caps :textColor @color.muted :fitText true))

(defstyle iconButton "A quiet icon button."
  (label :text ""
    :fitText true
    :padding 7
    :radius 3
    :cursor hand
    :focusable true
    :role button
    :textColor @color.muted
    :hover {:color @color.hover :textColor @color.bright}))

(defstyle sectionLink "A link to a section, plain text that brightens under the pointer."
  (label :fitText true :padding 10 :cursor hand :focusable true :role link :textColor @color.text :hover {:textColor @color.bright}))

(defstyle card "A panel with round corners." {:color @color.panel :radius 12})

;;; The library page

(defcomponent libraryPage "The library: the game list on the left, the home or a game's page on the right."
  {}
  `(row :fill true
     (node#library-sidebar :use librarySidebar)
     (node#library-main :grow 1 :anchors {:top 0 :bottom 0} :gradient {:kind linear :stops [@color.main @color.page]}
       (node#library-home :when ~(= computed.here.page "home") :use libraryHome)
       (node#game-page :when ~(and (= computed.here.page "game") (some? computed.game)) :use gamePage)
       (node#hub-page :when ~(and (= computed.here.page "hub") (some? computed.game)) :use hubPage))))

;;; The game list

(defcomponent librarySidebar "Home, the games filter with recent-first and ready-to-play, search, and the games by collection."
  {}
  `(column :width 272 :anchors {:top 0 :bottom 0} :color @color.side
     ~(home-row)
     ~(list-tools)
     ~(search-row)
     (column#library-list :grow 1 :anchors {:left 0 :right 0} :scroll vertical :arrows vertical :role list :label ~(t "lib.games")
       ~(repeat [section computed.sections :key section.id]
          (list-section section)))))

(defmacro any-open
  ;; A macro: the test stays an expression.
  "Whether any section of the list is unfolded."
  []
  `(some (fn [it] (not (or-else (get state.collapsed it.id) false))) computed.sections))

(defview home-row "Home, lit on the home, and the button that folds or unfolds every section." []
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
       :icon ~(if (any-open) "list-collapse" "list")
       :tooltip ~(t (if (any-open) "lib.collapseAll" "lib.expandAll"))
       :onClick toggleCollections)))

(defview list-tools "The list's title, and its two quick filters: the one played last first, and ready to play." []
  `(row :height 36 :anchors {:left 0 :right 0} :alignItems center
     (label#library-kind :font caps :text ~(t "lib.games") :icon chevron-down :textColor @color.muted :grow 1 :padding 14 :height 36)
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

(defview search-row "The search, and the filter menu beside it, lit while a filter is on." []
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
                :items [{:title ~(t "lib.readyOnly") :checked ~state.readyOnly :run toggleFilter :with {:key readyOnly}}
                        {:title ~(t "lib.favOnly") :checked ~state.favOnly :run toggleFilter :with {:key favOnly}}
                        {:title ~(t "lib.recentFirst") :checked ~state.recentFirst :run toggleFilter :with {:key recentFirst}}
                        {:separator true}
                        {:title ~(t "lib.clearFilters") :run clearFilters}]}))
     (panel :width 6 :height 1 :color transparent)))

(defview list-section "A section of the list: its caps title with how many it holds, which folds it, and its games." [section]
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
       :onClick ~(handler foldSection section.id open))
     ~(repeat [g games :key (+ section.id g.id)]
        `(node :id ~(str "row-" section.id "-" g.id) :when ~open :use gameRow :with {:game ~g}))))

(defcomponent gameRow "A game in the list: its cover as an icon and its name in its play-state colour, with what it is doing."
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
     (image :file ~(str "../assets/art/" (or-else game.art game.id) "/cover.svg") :width 18 :height 18 :fit cover :radius 2)
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

;;; The home

(defmacro grid-rows
  ;; A macro: the count stays an expression of the home's width.
  "How many rows the grid of every game takes, at the home's width."
  []
  `(ceil (/ (count computed.owned) (max 1 (floor (/ (- (or-else (get-in nodes ["home-scroll" :width]) 1008) 42) 164))))))

(defcomponent libraryHome "The library's home: what's new, recent games grouped by day, a shelf per collection and every game."
  {}
  `(column#home-scroll :fill true :scroll vertical
     (column#home-content :anchors {:left 28 :right 28}
       :gap 6
       :height ~(+ 412
                   (* 286 (count (filter (fn [it] (and (not= it.id "uncategorised") (> (count (or-else it.ids it.games)) 0))) computed.sections)))
                   (* 239 (grid-rows)))
       (panel :height 10 :width 1 :color transparent)
       ~(news-header)
       (row#news-shelf :when ~state.showNews :height 222 :anchors {:left 0 :right 0} :gap 14 :scroll horizontal :arrows horizontal
         ~(repeat [n data.news :index i]
            (news-card n i)))
       (node#recent-header :use shelfHeader :with {:title ~(t "lib.recent")})
       (row#recent-shelf :height 262 :anchors {:left 0 :right 0} :gap 18 :scroll horizontal
         ~(repeat [group computed.recentGroups :index gi]
            (recent-group group gi)))
       ~(repeat [section (slice computed.sections 0 (- (count computed.sections) 1)) :key section.id]
          (section-shelf section))
       (node#all-header :use shelfHeader :with {:title ~(t "lib.allGames") :count ~(count computed.owned)}
         (label :font caps :text ~(t "lib.sortBy") :textColor @color.muted :fitText true)
         ~(sort-button))
       ~(all-games)
       (panel :height 24 :width 1 :color transparent))))

(defcomponent shelfHeader "A shelf's caps title, with how many it holds; what is put in it goes on the right."
  {:title "" :count nil}
  `(row :height 34 :anchors {:left 0 :right 0} :alignItems center :gap 8
     (label :font caps :text ~(upper-case title) :textColor @color.text :fitText true)
     (label :when ~(some? count) :font caps :text ~(str "(" count ")") :textColor @color.muted :fitText true)
     (panel :grow 1 :height 1 :color transparent)
     (node :slot true)))

(defview news-header "What's new: its title, and the menu that hides it." []
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
                :items [{:title ~(t "lib.showNews") :checked ~state.showNews :run toggleFilter :with {:key showNews}}]}))))

(defview news-card "A piece of news: the game's banner, its kind and age, its title, and the game." [n i]
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
     (image :file ~(str "../assets/art/" n.game "/hero.svg") :anchors {:left 0 :right 0} :height 120 :fit cover :radius {:topLeft 8 :topRight 8})
     (label :font caps
       :height 26
       :anchors {:left 0 :right 0}
       :padding 10
       :runs [{:text ~(upper-case (t (str "news." n.kind))) :color @color.link}
              {:text ~(str " · " (t "time.daysAgo" {:n n.ago})) :color @color.muted}])
     (label :font strong :text ~n.title :textColor @color.bright :wrap true :anchors {:left 10 :right 10} :height 40)
     (label :font small :text ~g.title :textColor @color.muted :anchors {:left 10 :right 10} :height 18)))

(defview recent-group "Games played on a day, under its name; the very last one played is wide." [group gi]
  `(column :gap 8 :height 256 :width ~(if (= gi 0) (+ 345 (* (- (count group.games) 1) 164)) (- (* (count group.games) 164) 14))
     (label :font small :text ~group.label :textColor @color.muted :height 18 :anchors {:left 0 :right 0})
     (row :gap 14 :height 225 :anchors {:left 0 :right 0} :arrows horizontal
       ~(repeat [g group.games :index i]
          `(stack :width ~(if (and (= gi 0) (= i 0)) 345 150) :height 225
             (node :id ~(str "recent-big-" g.id) :when ~(and (= gi 0) (= i 0)) :use bigCapsule :with {:game ~g})
             (node :id ~(str "recent-" g.id) :when ~(not (and (= gi 0) (= i 0))) :use capsule :with {:game ~g}))))))

(defview section-shelf "A shelf for a section of the list: its title and its games' covers, in a row that scrolls." [section]
  `(column :let {:games ~(filter (fn [it] (contains (or-else section.ids section.games) it.id)) computed.owned)}
     :anchors {:left 0 :right 0}
     :height 280
     :gap 6
     :when ~(> (count games) 0)
     (node :use shelfHeader :with {:title ~section.title :count ~(count games)})
     (row :height 236 :anchors {:left 0 :right 0} :gap 14 :scroll horizontal :arrows horizontal
       ~(repeat [g games]
          `(node :id ~(str "shelf-" section.id "-" g.id) :use capsule :with {:game ~g})))))

(defview sort-button "The order of every game, and its menu: by name, by hours or by the one played last." []
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
              :items [{:title ~(t "sort.name") :checked ~(= settings.library.sortBy "name") :run setSort :with {:by name}}
                      {:title ~(t "sort.playtime") :checked ~(= settings.library.sortBy "playtime") :run setSort :with {:by playtime}}
                      {:title ~(t "sort.recent") :checked ~(= settings.library.sortBy "recent") :run setSort :with {:by recent}}]})))

(defview all-games "Every owned game's cover, in rows that fill the home's width." []
  `(row#all-grid :wrap true :gap 14 :anchors {:left 0 :right 0} :height ~(* (grid-rows) 239)
     ~(repeat [g computed.allGames :key g.id]
        `(node :id ~(str "all-" g.id) :use capsule :with {:game ~g}))))

;;; Covers

(defcomponent capsule "A game's cover, 2:3, that lifts and glows under the pointer; dimmed when the game isn't installed."
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
     (node :fill true :radius 12 :color transparent :gradient {:kind linear :stops [{:at 0.35 :color "#00000000"} {:at 1 :color "#000000d0"}]})
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
