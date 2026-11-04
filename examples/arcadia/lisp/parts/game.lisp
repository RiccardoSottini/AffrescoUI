;;; game.lisp --- a game's page in the library
;;;
;;; The banner with the game's logo; the play bar with Play (or Install, Update...) and the stats; the links to its
;;; sections; what friends did in it and its news; and the side panels: friends who play it, achievements,
;;; screenshots, the controller, and a note of your own.

;;; What the page shows

(defcomputed lastPlayed "When the game on the page was last played: today, yesterday, a date, or never."
  (cond (nil? computed.game.lastPlayed) (t "time.never")
        (< (- state.today computed.game.lastPlayed) 86400000) (t "time.today")
        (< (- state.today computed.game.lastPlayed) 172800000) (t "time.yesterday")
        :else (formatDate computed.game.lastPlayed "d MMM" "utc")))

(defmacro of-game
  ;; A macro: the list stays an expression, for a repeat and a count.
  "The records of a list (data.news, data.activity) about the game on the page."
  [records]
  `(filter (fn [it] (= it.game g.id)) ~records))

;;; The page

(defcomponent gamePage
  "A game's page: the banner and logo, play and the stats, the sections, the activity and the side panels."
  {}
  `(column#game-scroll :fill true :scroll vertical :let {:g ~computed.game}
     ~(hero)
     ~(play-bar)
     ~(section-links)
     (panel :height 1 :anchors {:left 32 :right 32} :color @color.border)
     (row#game-columns :anchors {:left 32 :right 32}
       :gap 24
       :height ~(max 700 (+ 150 (* (count (of-game data.activity)) 96) (* (count (of-game data.news)) 120)))
       ~(activity)
       ~(side-panels))))

(defview hero "The banner, darker at the bottom, with the game's name as its logo over a shadow." []
  `(node#hero :height 310 :anchors {:left 0 :right 0}
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
       :role heading)))

(defview section-links
  "The links to the game's sections: the store page, extra content, the hub, discussions, guides, support."
  []
  `(row#game-sections :height 46 :anchors {:left 24 :right 24} :alignItems center :arrows horizontal
     ~(repeat [s (if g.local ["support"] ["store" "dlc" "hub" "discussions" "guides" "support"])]
        `(node :id ~(str "section-link-" s)
           :style sectionLink
           :text ~(t (str "game.section." s))
           :onClick ~(handler openSection g.id s)))))

;;; The play bar

(defview play-bar
  "Play (with its menu), when it was last played, the hours, the achievements, cloud saves, and the buttons."
  []
  `(row#play-bar :height 80 :anchors {:left 0 :right 0} :alignItems center :gap 26 :color @color.shade
     (panel :width 6 :height 1 :color transparent)
     (row#play-group :height 48 :width ~(if (= g.status "ready") 196 210) :radius 24 ~(play-button) ~(play-more))
     (node#stat-last :use stat :with {:title ~(t "game.lastPlayed") :value ~computed.lastPlayed})
     (node#stat-time :use stat
       :with {:title ~(t "game.playTime") :value ~(t "game.hours" {:n (formatNumber g.hours 0 1)})})
     ~(achievements-stat)
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

(defview play-button "Play, Stop, Install or Update, green when the game is ready." []
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

(defview play-more "The arrow beside Play, and its menu: play, play in safe mode, or the launch options." []
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

(defcomponent stat "A figure of the play bar: a caps title over a value, as wide as the value."
  {:title "" :value "" :icon ""}
  `(column :height 40 :gap 4 :width ~(max 90 (+ (* (count value) 8) 24))
     (label :font caps :text ~(upper-case title) :textColor @color.muted :height 15 :anchors {:left 0 :right 0})
     (label :font small :icon ~icon :text ~value :textColor @color.bright :height 18 :anchors {:left 0 :right 0})))

(defmacro achieved-width
  ;; A macro: the width stays an expression of the game.
  "How much of a bar the game's won achievements fill, as a percentage."
  []
  `(str (text (floor (/ (* 100 (or-else g.achievements.0 0)) (max 1 (or-else g.achievements.1 0))))) "%"))

(defview achievements-stat "The achievements in the play bar: how many are won, over a bar." []
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
           :width ~(achieved-width)
           :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]})))))

(defview info-card "The game's card under the info button: its name, what it is, who made it and when." []
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

;;; Activity

(defview activity "What's happened in the game: a box to post in, its news, and what friends did in it." []
  `(column#activity :grow 1 :gap 12 :anchors {:top 16 :bottom 0}
     (label :font caps
       :text ~(upper-case (t "game.activity"))
       :textColor @color.text
       :height 22
       :anchors {:left 0 :right 0})
     (textarea#post-box :height 44
       :anchors {:left 0 :right 0}
       :padding 12
       :bind state.post
       :placeholder ~(t "game.say")
       :placeholderColor @color.dim
       :textColor @color.text
       :color @color.panel
       :radius 3
       :label ~(t "game.say")
       :focused {:borderWidth 1 :borderColor @color.accent})
     ~(repeat [n (of-game data.news) :index i] (game-news n))
     ~(repeat [a (of-game data.activity) :index i] (friend-activity a))
     (label :when ~(= (+ (count (of-game data.activity)) (count (of-game data.news))) 0)
       :text ~(t "game.noActivity")
       :textColor @color.muted
       :height 40
       :anchors {:left 0 :right 0})))

(defview game-news "A piece of the game's news: its banner, kind and age, title and text." [n]
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
       (label :font small :text ~n.text :textColor @color.muted :wrap true :anchors {:left 0 :right 12} :height 36))))

(defview friend-activity
  "What a friend did in the game: their avatar, what it was, and a screenshot if they took one."
  [a]
  `(row :let {:f ~(find (fn [it] (= it.id a.who)) data.friends)}
     :height ~(if (= a.kind "screenshot") 168 84)
     :anchors {:left 0 :right 0}
     :style card
     :gap 12
     ~(friend-avatar)
     (column :grow 1 :gap 4 :anchors {:top 14 :bottom 10}
       (label :anchors {:left 0 :right 0}
         :height 18
         :runs [{:text ~(str f.name "\u00a0") :color @color.online :bold true}
                ; a space that doesn't break
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
         :height 16))))

(defview friend-avatar "The friend's avatar, its border showing whether they are in a game, online or offline." []
  `(image :file ~(str "../assets/art/avatars/" f.avatar ".svg")
     :width 36
     :height 36
     :anchors {:top 14}
     :fit cover
     :radius 2
     :borderWidth 1
     :borderColor ~(cond (= f.presence "ingame") "@color.ingame"
                         (= f.presence "offline") "@color.offline"
                         :else "@color.online")))

;;; The side panels

(defcomponent sideCard "A panel of the page's side: a caps title, and what is put in it."
  {:title "" :height 100}
  `(node :anchors {:left 0 :right 0} :height ~height :style card
     (label :font caps :text ~(upper-case title) :textColor @color.text :anchors {:left 14 :top 14} :fitText true)
     (node :slot true)))

(defview side-panels
  "The side panels: friends who play it, achievements, screenshots, the controller and your note."
  []
  `(column#game-side :width 312 :gap 14 :anchors {:top 16 :bottom 0}
     (node#friends-card :use sideCard :with {:title ~(t "game.friendsWhoPlay") :height 118} ~(friends-who-play))
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

(defview friends-who-play "How many friends play the game, and their avatars, lime for those playing it now." []
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

(defview achievements-card "The achievements: how many are won, a bar, and the latest five with how many more." []
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
           :width ~(achieved-width)
           :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}))
       (row :gap 6 :height 44 :anchors {:left 0 :right 0}
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
           :radius 3)))))

;;; Your note

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
