;;; rewards.lisp --- the store bar's Rewards and Experiments pages
;;;
;;; Rewards is a points shop: every euro spent in the store earns 100 points (purchase, in arcadia.lisp), and the
;;; points buy profile items: avatar frames, profile backgrounds, badges and chat stickers. Experiments holds ideas the
;;; person can try and turn off: a discovery queue that shows the store one game at a time, and a switch that hides
;;; the games the person owns from the store's lists. Every item and its picture is invented.

(ns arcadia.parts.rewards)

;;; What the rewards keep

(defstate points 4820)

(defstate rewardsOwned [gem-badge])

(defstate rewardKind all)

(defstate rewardItems
  [{:id ember-frame :kind frame :icon flame :cost 1500 :a "#ff8a3d" :b "#7a1f3d"}
   {:id crown-frame :kind frame :icon crown :cost 3000 :a "#ffd36b" :b "#8a5a00"}
   {:id nebula-bg :kind background :icon moon :cost 2000 :a "#8a5aa8" :b "#140f18"}
   {:id comet-bg :kind background :icon rocket :cost 2500 :a "#2fbf98" :b "#1b3a5a"}
   {:id gem-badge :kind badge :icon gem :cost 800 :a "#7fc8ff" :b "#2a3a8a"}
   {:id award-badge :kind badge :icon award :cost 1200 :a "#f5a524" :b "#5a2a00"}
   {:id party-sticker :kind sticker :icon party-popper :cost 300 :a "#ffe36b" :b "#c97a1a"}
   {:id tune-sticker :kind sticker :icon music :cost 400 :a "#ff7fbf" :b "#5a1f6e"}])

(defcomputed rewardsShown "The rewards of the kind chosen, or all of them."
  (filter #(or (= state.rewardKind "all") (= (:kind %) state.rewardKind)) state.rewardItems))

(defcomputed rewardRows "How many rows of four cards the rewards shown take."
  (ceil (/ (count computed.rewardsShown) 4)))

(defcomputed rewardWidth "The width of a reward's card: four to a row of the store's width."
  (/ (- computed.storeWidth 42) 4))

;;; What the experiments keep

(defstate labsQueue false)

(defstate labsAt 0)

(defstate labsSkipped [])

(defstate labsHideOwned false)

(defcomputed labsGames "The discovery queue: the store's games the person doesn't own and hasn't passed on."
  (filter #(not (or (:owned %) (has state.labsSkipped (:id %)))) computed.storeGames))

(defcomputed labsGame "The game the discovery queue shows, or nil once it has shown them all."
  (get computed.labsGames state.labsAt))

;;; The pages' measures

(defmacro rewards-height
  ;; A macro: the height stays an expression, for the page and for the store around it.
  "The height of the Rewards page: its heading, its kinds, and a row of four cards at a time."
  []
  `(+ 150 (* 254 computed.rewardRows)))

(defmacro labs-height
  ;; A macro: the height stays an expression, for the page and for the store around it.
  "The height of the Experiments page: its heading and its two experiments, the queue's taller while it runs."
  []
  `(+ 70 14 (if state.labsQueue 420 124) 14 104))

(defpart rewards "The big icon on a reward's picture."
  :fonts {:rewardArt {:file "../../shared/fonts/Roboto-Regular.ttf" :size 44 :fallback [rewardIcons]}
          :rewardIcons {:file "../../shared/fonts/Lucide.ttf" :size 44}})

;;; Rewards

(defcomponent storeRewards
  "The Rewards page: the points the person has, how they are earned, the kinds of item, and the items to redeem."
  {}
  `(column#store-rewards-page :height ~(rewards-height) :anchors {:left 0 :right 0}
     (row :height 60 :anchors {:left 0 :right 0} :alignItems center :gap 12
       (label :font title :grow 1 :height 40 :text ~(upper-case (t "rewards.title")) :textColor @color.bright)
       (label#rewards-points :font strong
         :fitText true
         :padding 14
         :height 34
         :radius 17
         :icon star
         :text ~(t "rewards.points" {:n state.points})
         :color @store.chip
         :textColor @store.chipText))
     (label :font small :height 22 :anchors {:left 0 :right 0} :text ~(t "rewards.earn") :textColor @color.muted)
     ~(reward-kinds)
     (row#rewards-grid :wrap true
       :gap 14
       :anchors {:left 0 :right 0}
       :height ~(* 254 computed.rewardRows)
       :role list
       :label ~(t "rewards.title")
       ~(repeat [r computed.rewardsShown :key r.id] (reward-card r)))))

(defview reward-kinds "The kinds of item, all of them first, the chosen one lit." []
  `(row#reward-kinds :height 52 :anchors {:left 0 :right 0} :gap 6 :alignItems center :role tablist :arrows horizontal
     ~(repeat [k ["all" "frame" "background" "badge" "sticker"]]
        `(node :id ~(str "reward-kind-" k)
           :style pillButton
           :role tab
           :selected ~(= state.rewardKind k)
           :color ~(if (= state.rewardKind k) "@color.select" "transparent")
           :textColor ~(if (= state.rewardKind k) "@color.bright" "@color.text")
           :text ~(t (str "rewards.kind." k))
           :onClick ~(handler show-reward-kind k)))))

(defview reward-card "A reward: its picture, its name and kind, its cost, and Redeem, or Owned once it is the person's."
  [r]
  `(column :id ~(str "reward-" r.id)
     :let {:mine ~(has state.rewardsOwned r.id)}
     :role listitem
     :label ~(t (str "rewards.item." r.id))
     :width ~computed.rewardWidth
     :height 240
     :color @store.card
     :radius 6
     :gap 8
     (node :anchors {:left 0 :right 0}
       :height 130
       :radius {:topLeft 6 :topRight 6}
       :gradient {:kind linear :angle 135 :stops [~r.a ~r.b]}
       (label :font rewardArt :icon ~r.icon :text "" :fill true :align center :textColor "#ffffffe0"))
     (label :font strong
       :height 20
       :anchors {:left 12 :right 12}
       :text ~(t (str "rewards.item." r.id))
       :textColor @store.text)
     (label :font small
       :height 16
       :anchors {:left 12 :right 12}
       :text ~(t (str "rewards.type." r.kind))
       :textColor @color.muted)
     (row :height 44 :anchors {:left 12 :right 12} :alignItems center :gap 8
       (label :font strong :fitText true :icon star :text ~(formatNumber r.cost) :textColor @store.chipText)
       (panel :color transparent :grow 1 :height 1)
       (label :when ~mine
         :font small
         :fitText true
         :padding 12
         :height 30
         :radius 2
         :icon check
         :text ~(t "rewards.owned")
         :color @color.raised
         :textColor @color.muted)
       (node :when ~(not mine)
         :id ~(str "redeem-" r.id)
         :use ctaButton
         :height 30
         :with {:title ~(t "rewards.redeem") :enabled ~(>= state.points r.cost)}
         :tooltip ~(if (< state.points r.cost) (t "rewards.short" {:n (- r.cost state.points)}) "")
         :onClick ~(handler redeemReward r.id)))))

;;; Experiments

(defcomponent storeLabs
  "The Experiments page: ideas to try, each with what it does and the way to turn it on or off."
  {}
  `(column#store-labs-page :height ~(labs-height) :anchors {:left 0 :right 0} :gap 14
     (column :height 70 :anchors {:left 0 :right 0}
       (label :font title :height 44 :anchors {:left 0 :right 0} :text ~(upper-case (t "labs.title")) :textColor @color.bright)
       (label :font small :height 22 :anchors {:left 0 :right 0} :text ~(t "labs.about") :textColor @color.muted))
     ~(labs-queue)
     ~(labs-hide-owned)))

(defview labs-queue "The discovery queue: off, a button to start it; on, one game at a time with what to do with it." []
  `(column#labs-queue :height ~(if state.labsQueue 420 124)
     :anchors {:left 0 :right 0}
     :color @store.card
     :radius 6
     :gap 0
     (panel :height 26 :anchors {:left 0 :right 0} :color transparent)
     (row :height 72 :anchors {:left 20 :right 20} :gap 14 :alignItems center
       (label :font heading :icon compass :text "" :fitText true :textColor @color.accent)
       (column :grow 1 :height 44
         (label :font strong :height 22 :anchors {:left 0 :right 0} :text ~(t "labs.queue") :textColor @store.text)
         (label :font small :height 20 :anchors {:left 0 :right 0} :text ~(t "labs.queue.about") :textColor @color.muted))
       (node#labs-queue-toggle :use quietButton
         :with {:title ~(t (if state.labsQueue "labs.stop" "labs.start"))
                :icon ~(if state.labsQueue "square" "play")
                :on ~state.labsQueue}
         :onClick toggleDiscoveryQueue))
     (node :when ~state.labsQueue :anchors {:left 20 :right 20} :height 300 :color transparent
       ~(labs-queue-game)
       (label#labs-queue-done :when ~(nil? computed.labsGame)
         :fill true
         :align center
         :text ~(t "labs.queue.done")
         :textColor @color.muted))))

(defview labs-queue-game "The queue's game: its picture, its name, genre and tags, its price, and what to do with it." []
  `(row :when ~(some? computed.labsGame) :let {:g ~computed.labsGame} :fill true :gap 20
     (image#labs-queue-art :file ~(str "../assets/art/" g.id "/hero.svg")
       :width 480
       :height 270
       :radius 4
       :fit cover
       :cursor hand
       :label ~g.title
       :onClick ~(handler openStore g.id))
     (column :grow 1 :height 270 :gap 8
       (label :font small
         :height 18
         :anchors {:left 0 :right 0}
         :text ~(t "labs.queue.of" {:n (inc state.labsAt) :total (count computed.labsGames)})
         :textColor @color.muted)
       (label#labs-queue-title :font title :height 34 :anchors {:left 0 :right 0} :text ~g.title :textColor @color.bright)
       (label :font small :height 18 :anchors {:left 0 :right 0} :text ~(t (str "genre." g.genre)) :textColor @color.muted)
       (row :height 24 :anchors {:left 0 :right 0} :gap 4
         ~(repeat [tag (slice g.tags 0 3)]
            `(label :font small :text ~tag :fitText true :padding 7 :radius 2 :color @store.chip :textColor @store.chipText)))
       (panel :color transparent :grow 1 :width 1)
       (node :use priceTag :with {:g ~g})
       (row :height 36 :anchors {:left 0 :right 0} :gap 6 :alignItems center
         (node#labs-queue-wish :use quietButton
           :with {:title ~(t (if (has state.wishlist g.id) "store.onWishlist" "store.addWishlist"))
                  :icon heart
                  :on ~(has state.wishlist g.id)}
           :onClick ~(handler toggleWishlist g.id))
         (node#labs-queue-skip :use quietButton
           :with {:title ~(t "labs.queue.skip") :icon thumbs-down}
           :onClick ~(handler skipQueueGame g.id))
         (panel :color transparent :grow 1 :height 1)
         (node#labs-queue-next :use ctaButton :with {:title ~(t "labs.queue.next")} :onClick nextQueueGame)))))

(defview labs-hide-owned "The switch that hides the games the person owns from the store's lists." []
  `(column#labs-hide-owned :height 104 :anchors {:left 0 :right 0} :color @store.card :radius 6
     (panel :height 16 :anchors {:left 0 :right 0} :color transparent)
     (row :height 72 :anchors {:left 20 :right 20} :gap 14 :alignItems center
       (label :font heading :icon eye-off :text "" :fitText true :textColor @color.accent)
       (column :grow 1 :height 44
         (label :font strong :height 22 :anchors {:left 0 :right 0} :text ~(t "labs.hideOwned") :textColor @store.text)
         (label :font small
           :height 20
           :anchors {:left 0 :right 0}
           :text ~(t "labs.hideOwned.about")
           :textColor @color.muted))
       (toggle#labs-hide-owned-toggle :width 40
         :height 22
         :label ~(t "labs.hideOwned")
         :checked ~state.labsHideOwned
         :activeColor @color.accent
         :onChange toggleHideOwned))))
