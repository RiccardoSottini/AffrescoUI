;;; profile.lisp --- the profile page: who you are on Arcadia, what you play and who with
;;;
;;; The banner with the avatar, level and status; numbers; a showcase of favourites; the games played lately with
;;; their hours and achievements; and friends.

;;; What it shows

(defcomputed profileRecent "The four games played last."
  (slice (sort-by (fn [it] it.lastPlayed) > (filter (fn [it] (some? it.lastPlayed)) computed.owned)) 0 4))

(deftokens profile-colours "The profile banner's colours, from top to bottom."
  {:profile.bannerStart {:dark "#3b2346" :light "#eadcf2"} :profile.bannerEnd {:dark "#211a28" :light "#f6f2f7"}})

;;; The page

(defcomponent profilePage "The profile: the banner, the numbers, then the showcase and recent games beside the friends."
  {}
  `(node#profile-page :fill true :color @color.page
     (column#profile-scroll :fill true :scroll vertical
       ~(profile-banner)
       ~(profile-stats)
       (panel :height 1 :anchors {:left 40 :right 40} :color @color.border)
       (row :anchors {:left 40 :right 40} :gap 32 :height ~(max 560 (+ 120 (* 96 (count computed.profileRecent))))
         ~(profile-games)
         ~(profile-friends)))))

(defview profile-banner "The banner: the avatar, the name, since when and the status, and the level in a ring." []
  `(row#profile-banner :height 196
     :anchors {:left 0 :right 0}
     :gap 24
     :alignItems center
     :gradient {:kind linear :angle 0 :stops [@profile.bannerStart @profile.bannerEnd]}
     (panel :color transparent :width 16 :height 1)
     (node :use avatar :with {:avatar ~state.user.avatar :presence ~state.user.status :size 132})
     (column :grow 1 :height 84 :gap 6 :alignSelf center
       (label#profile-name :font logo :text ~state.user.name :textColor @color.bright :anchors {:left 0 :right 0} :height 58 :role heading)
       (label :anchors {:left 0 :right 0}
         :height 20
         :textColor @color.muted
         :text ~(t "profile.sinceStatus"
                   {:since (t "profile.since" {:year (text state.user.since)}) :status (t (str "presence." state.user.status))})))
     (column#profile-level :width 140 :height 120 :gap 6 :alignItems center :alignSelf center
       (label :font small :text ~(t "profile.level") :textColor @color.muted :fitText true)
       (label :font nav
         :text ~state.user.level
         :width 56
         :height 56
         :radius 28
         :align center
         :borderWidth 3
         :borderColor @color.accent
         :textColor @color.bright
         :label ~(t "profile.levelIs" {:n state.user.level})))
     (panel :color transparent :width 32 :height 1)))

(defcomponent profileStat "A number with its name, on the profile."
  {:value "" :title "" :id ""}
  `(column :id ~id :width 120 :height 56 :gap 2
     (label :font nav :text ~value :textColor @color.bright :anchors {:left 0 :right 0} :height 26)
     (label :font small :text ~title :textColor @color.muted :anchors {:left 0 :right 0} :height 16)))

(defview profile-stats "The numbers: games, hours played, achievements and friends." []
  `(row#profile-stats :height 88 :anchors {:left 40 :right 40} :gap 24 :alignItems center
     (node :use profileStat :with {:id stat-games :value ~(count computed.owned) :title ~(t "profile.games")})
     (node :use profileStat
       :with {:id stat-hours :value ~(formatNumber (sum computed.owned (fn [it] it.hours)) 0 0) :title ~(t "profile.hours")})
     (node :use profileStat
       :with {:id stat-achievements
              :value ~(sum computed.owned (fn [it] (or-else it.achievements.0 0)))
              :title ~(t "profile.achievements")})
     (node :use profileStat :with {:id stat-friends :value ~(count data.friends) :title ~(t "profile.friends")})))

;;; Games

(defview profile-games "The showcase of favourite games, then the games played lately." []
  `(column :grow 1 :gap 12 :alignSelf stretch
     (panel :color transparent :width 1 :height 12)
     (label :style caps :height 18 :anchors {:left 0 :right 0} :role heading :text ~(t "profile.showcase"))
     (row#profile-showcase :height 180 :gap 14 :anchors {:left 0 :right 0}
       ~(repeat [g (slice (filter (fn [it] it.favourite) computed.owned) 0 5) :key g.id]
          `(node :use capsule :with {:game ~g :width 116})))
     (label :style caps :height 18 :anchors {:left 0 :right 0} :role heading :text ~(t "profile.recent"))
     ~(repeat [g computed.profileRecent :key g.id]
        (recent-game g))))

(defview recent-game "A game played lately: its banner, name and hours, and how many of its achievements are won." [g]
  `(row :id ~(str "profile-recent-" g.id)
     :height 84
     :anchors {:left 0 :right 0}
     :style card
     :gap 14
     :alignItems center
     :focusable true
     :role link
     :cursor hand
     :label ~g.title
     :hover {:color @color.raised}
     :onClick ~(handler openGame g.id)
     (panel :color transparent :width 4 :height 1)
     (image :file ~(str "../assets/art/" g.id "/hero.svg") :width 150 :height 64 :fit cover :radius 3)
     (column :grow 1 :height 40 :gap 4 :alignSelf center
       (label :font strong :text ~g.title :textColor @color.bright :anchors {:left 0 :right 0} :height 18)
       (label :font small :anchors {:left 0 :right 0} :height 16 :textColor @color.muted :text ~(t "game.hoursTotal" {:n g.hours})))
     ~(achievements g)
     (panel :color transparent :width 16 :height 1)))

(defview achievements "A game's achievements: how many are won, over a bar." [g]
  `(column :when ~(some? g.achievements) :width 180 :height 28 :gap 6 :alignSelf center
     (label :font small
       :anchors {:left 0 :right 0}
       :height 16
       :textColor @color.muted
       :text ~(t "profile.achieved" {:got g.achievements.0 :total g.achievements.1}))
     (node :height 6 :radius 3 :anchors {:left 0 :right 0} :color @color.raised
       (node :anchors {:left 0 :top 0 :bottom 0}
         :radius 3
         :width ~(str (text (floor (/ (* 100 g.achievements.0) (max 1 g.achievements.1)))) "%")
         :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}))))

;;; Friends

(defview profile-friends "The friends, those online first: eight, each opening their chat." []
  `(column#profile-friends :width 260 :gap 8 :alignSelf stretch
     (panel :color transparent :width 1 :height 16)
     (label :style caps :height 18 :anchors {:left 0 :right 0} :role heading :text ~(t "profile.friendsCount" {:n (count data.friends)}))
     ~(repeat [f (slice (sort-by (fn [it] (if (= it.presence "offline") 1 0)) data.friends) 0 8) :key f.id]
        (profile-friend f))))

(defview profile-friend "A friend on the profile: the avatar and the name, in their presence's colour." [f]
  `(row :height 40
     :anchors {:left 0 :right 0}
     :gap 10
     :alignItems center
     :focusable true
     :role link
     :cursor hand
     :label ~f.name
     :hover {:color @color.raised}
     :onClick ~(handler openChat f.id)
     (node :use avatar :with {:avatar ~f.avatar :presence ~f.presence :size 30})
     (label :let {:p ~f.presence} :grow 1 :height 18 :text ~f.name :textColor ~(presence-colour p))))
