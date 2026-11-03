;;; store.lisp --- Arcadia's store: the store bar and search, a featured carousel whose picture follows the screenshot under the pointer, special offers, categories, tabbed lists with a preview, a page per game (gallery, reviews, buying, requirements), the cart and the wishlist.

;;; Definitions

(defstate cart [volt])

(defstate wishlist [glasswing aurora hollowsong tinyfleet])

(defstate followed [])

(defstate storeQuery "")

(defstate storeFeature 0)

(defstate storeShot nil)

(defstate storeList new)

(defstate storeHover nil)

(defstate storeCatsOpen false)

(defstate shotIndex 0)

(defstate reqTab windows)

(defstate wishSort rank)

(defstate wishSortOpen false)

(defstate wishQuery "")

(defstate cartAdded nil)

(defstate cartDialog false)

(defstate receipt [])

(defstate votes {})

(defstate reviews
  [{:author "MossyGolem"
    :avatar frog
    :products 142
    :reviews 12
    :up true
    :hours 42.5
    :posted "3 October"
    :text "Plays beautifully and respects my time. Forty hours in, I am still finding corners I missed, and the soundtrack is on loop at work."}
   {:author quietfern
    :avatar owl
    :products 37
    :reviews 3
    :up true
    :hours 11.2
    :posted "28 September"
    :text "Short sessions, big smile. Runs well on my old laptop, and the controller support is spot on."}
   {:author "Pixel_Baron"
    :avatar knight
    :products 611
    :reviews 88
    :up false
    :hours 3.4
    :posted "21 September"
    :text "Lovely to look at, but the middle chapters drag. I will come back after a few patches."}])

(defcomputed storeView (if (not= state.storeQuery "") "results" computed.here.page))

(defcomputed price (map (fn [it] (/ (round (* it.price (- 100 it.discount))) 100)) data.games))

(defcomputed storeGames (filter (fn [it] (not it.local)) data.games))

(defcomputed featured
  (filter (fn [it] (contains ["aurora" "glasswing" "clockwork" "rivermill" "volt"] it.id)) data.games))

(defcomputed feature (get computed.featured state.storeFeature))

(defcomputed offers
  (slice (sort-by (fn [it] it.discount) > (filter (fn [it] (> it.discount 0)) computed.storeGames)) 0 6))

(defcomputed storeRows
  (cond (= state.storeList "top") (slice (sort-by (fn [it] it.reviewCount) > computed.storeGames) 0 8)
        (= state.storeList "specials") (slice (sort-by (fn [it] it.discount) > (filter (fn [it] (> it.discount 0)) computed.storeGames)) 0 8)
        :else (slice (reverse computed.storeGames) 0 8)))

(defcomputed storePreview
  (or-else (find (fn [it] (= it.id state.storeHover)) computed.storeRows) computed.storeRows.0))

(defcomputed results
  (filter (fn [it]
            (or (contains (lower-case it.title) (lower-case state.storeQuery))
                (= (lower-case it.genre) (lower-case state.storeQuery))
                (some (fn [it] (= (lower-case it) (lower-case state.storeQuery))) it.tags)))
          computed.storeGames))

(defcomputed cartGames (filter (fn [it] (contains state.cart it.id)) data.games))

(defcomputed cartTotal (/ (round (sum computed.cartGames (fn [it] (* it.price (- 100 it.discount))))) 100))

(defcomputed wishGames
  (filter (fn [it]
            (and (contains state.wishlist it.id)
                 (or (= state.wishQuery "")
                     (contains (lower-case it.title) (lower-case state.wishQuery))
                     (some (fn [it] (contains (lower-case it) (lower-case state.wishQuery))) it.tags))))
          data.games))

(defcomputed wishSorted
  (cond (= state.wishSort "name") (sort-by (fn [it] it.title) computed.wishGames)
        (= state.wishSort "price") (sort-by (fn [it] (* it.price (- 100 it.discount))) computed.wishGames)
        (= state.wishSort "discount") (sort-by (fn [it] it.discount) > computed.wishGames)
        :else (sort-by (fn [it] (indexOf state.wishlist it.id)) computed.wishGames)))

(defcomputed storeWidth (min 940 (- window.width 32)))

(defcomputed storeHeight
  (+ 86
     (cond (= computed.storeView "results") (+ 110 (* 74 (max 1 (count computed.results))))
           (= computed.storeView "app") 1670
           (= computed.storeView "cart") (+ 140 (max 250 (+ (* (count computed.cartGames) 106) 60) (+ (* (count state.receipt) 40) 160)))
           (= computed.storeView "wishlist") (+ 160 (* (max 1 (count computed.wishSorted)) 128))
           :else 1960)))

(defcomponent priceTag "A game's price: the discount and the old price struck through, or 'In library' when owned."
  {:g {} :big false}
  `(row :height ~(if big 40 32) :width ~(cond g.owned 86 (> g.discount 0) (if big 156 132) big 90 :else 76)
     (label :when ~g.owned
       :font small
       :text ~(upper-case (t "store.inLibrary"))
       :grow 1
       :height ~(if big 40 32)
       :align center
       :radius 2
       :color @store.chip
       :textColor @store.chipText)
     (label :when ~(and (not g.owned) (> g.discount 0))
       :font ~(if big "price" "button")
       :text ~(str "-" g.discount "%")
       :width ~(if big 66 56)
       :height ~(if big 40 32)
       :align center
       :color @color.badge
       :textColor @color.onBadge
       :radius {:topLeft 2 :bottomLeft 2})
     ~(price-tag-node g big)))

(defcomponent ctaButton "The green call to action of the store (Add to Cart, Purchase)."
  {:title "" :enabled true}
  `(label :text ~title
     :fitText true
     :padding 16
     :radius 2
     :font default
     :focusable true
     :role button
     :cursor hand
     :textColor ~(if enabled "@store.onCta" "@color.dim")
     :color ~(if enabled "transparent" "@color.raised")
     :gradient ~{:kind "linear"
                 :angle 90
                 :stops (if enabled ["@store.cta" "@store.ctaEnd"] ["@color.raised" "@color.raised"])}
     :hover {:textColor "#ffffff" :shadow {:blur 10 :color "#7ff0cf55"}}))

(defcomponent quietButton "The store's quiet button (wishlist, follow, player hub)."
  {:title "" :icon "" :on false}
  `(label :text ~title
     :icon ~icon
     :fitText true
     :padding 12
     :radius 2
     :focusable true
     :role button
     :cursor hand
     :selected ~on
     :color @store.chip
     :textColor @store.chipText
     :hover {:textColor "#ffffff" :gradient {:kind linear :angle 135 :stops ["#ffc46b" "#c9701a"]}}))

(defcomponent storeHeader "A store section's heading, in capitals, with an optional button at the end."
  {:title ""}
  `(row :height 46 :anchors {:left 0 :right 0} :alignItems end
     (label :font caps :text ~(upper-case title) :textColor @color.bright :fitText true :padding 8)
     (panel :color transparent :grow 1 :height 1)
     (node :slot true)))

(defcomponent storeTop
  "Wishlist and cart links, then the store bar: For you, Fresh, Categories, Rewards, News, Experiments, and the search field."
  {}
  `(column :height 86 :anchors {:left 0 :right 0}
     ~(store-top-row)
     (row#store-bar :height 40
       :anchors {:left 0 :right 0}
       :alignItems center
       :gap 4
       :radius 20
       :color @color.side
       :arrows horizontal
       (panel :width 1 :height 1 :color transparent)
       ~(repeat [item ["yours" "noteworthy" "categories" "points" "news" "labs"]] (store-view item))
       (panel :color transparent :grow 1 :height 1)
       ~(store-search)
       (panel :color transparent :width 6 :height 1)
       (panel :width 1 :height 1 :color transparent))
     (panel :color transparent :height 10 :width 1)))

(defcomponent featured
  "The featured carousel: a big capsule whose picture follows the screenshot under the pointer, the arrows and the dots."
  {}
  `(node#featured :height 383
     :anchors {:left -46 :right -46}
     :children [~(featured-row)
                ~(featured-prev)
                ~(featured-next)
                (row :height 12 :anchors {:left 0 :right 0 :bottom 6} :gap 3 :justify center
                  ~(repeat [d (range 5)]
                     `(node :width 15
                        :height 9
                        :radius 2
                        :cursor hand
                        :color ~(if (= d state.storeFeature) "@store.dotOn" "@store.dot")
                        :role button
                        :label ~(t "store.slide" {:n (+ d 1)})
                        :onClick {:set state.storeFeature :to ~d})))]
     :color transparent))

(defcomponent offerCard "A special offer: the game's banner, when the offer ends and its price."
  {:g {}}
  `(node :width ~(/ (- computed.storeWidth 22) 3)
     :height 196
     :color @store.card
     :radius 2
     :focusable true
     :role button
     :cursor hand
     :label ~g.title
     :shadow {:y 2 :blur 6 :color "#00000050"}
     :hover {:color @store.cardHover :shadow {:y 4 :blur 14 :color "#f5a52450"}}
     :onClick ~(handler openStore g.id)
     (image :file ~(str "../assets/art/" g.id "/hero.svg")
       :fit cover
       :height 143
       :anchors {:left 0 :right 0 :top 0}
       :radius {:topLeft 2 :topRight 2})
     (label :text ~(t (if (>= g.discount 50) "store.weeklong" "store.midweek"))
       :fitText true
       :font caps
       :textColor @store.text
       :anchors {:left 10 :bottom 28})
     (label :text ~(t "store.endsOn" {:date "2 Nov"})
       :fitText true
       :font small
       :textColor @color.muted
       :anchors {:left 10 :bottom 10})
     (node :use priceTag :with {:g ~g} :anchors {:right 8 :bottom 10})))

(defcomponent categoryTile "A category to browse: a cover under a coloured wash and the category's name."
  {:genre "" :game "" :tint ""}
  `(node :width ~(/ (- computed.storeWidth 36) 4)
     :height 170
     :radius 4
     :color "#000000"
     :focusable true
     :role button
     :cursor hand
     :label ~(t (str "genre." genre))
     :hover {:scale 1.03 :shadow {:blur 16 :color "#f5a52450"}}
     :onClick ~(handler storeSearch genre)
     (image :file ~(str "../assets/art/" game "/hero.svg") :fill true :fit cover :radius 4)
     (node :fill true
       :radius 4
       :color transparent
       :gradient {:kind linear :stops [{:at 0.2 :color "#00000000"} {:at 1 :color ~tint}]})
     (label :font heading
       :text ~(upper-case (t (str "genre." genre)))
       :textColor "#ffffff"
       :height 30
       :align center
       :anchors {:left 0 :right 0 :bottom 14})))

(defcomponent storeRow
  "A game in a store list: its banner, title, tags and price; it lights up when it is the one previewed."
  {:g {} :lit false}
  `(row :height 69
     :anchors {:left 0 :right 0}
     :alignItems center
     :gap 12
     :radius 2
     :color ~(if lit "@store.preview" "@store.card")
     :focusable true
     :role button
     :cursor hand
     :label ~g.title
     :onClick ~(handler openStore g.id)
     (image :file ~(str "../assets/art/" g.id "/hero.svg")
       :fit cover
       :width 184
       :height 69
       :radius {:topLeft 2 :bottomLeft 2})
     (column :grow 1 :height 46
       (label :text ~g.title :height 22 :anchors {:left 0 :right 0} :textColor ~(if lit "@store.ink" "@store.text"))
       (label :font small
         :height 18
         :anchors {:left 0 :right 0}
         :icon monitor
         :text ~(str "  " (join ", " (slice g.tags 0 3)))
         :textColor ~(if lit "@store.inkSoft" "@color.muted")))
     (node :use priceTag :with {:g ~g})
     (panel :color transparent :width 4 :height 1)))

(defcomponent storeHome
  "The store's front page: featured, special offers, categories and the tabbed lists with a preview."
  {}
  `(column#store-home :height 1960 :anchors {:left 0 :right 0}
     (node :use storeHeader :with {:title ~(t "store.featured")})
     (node :use featured)
     (node :use storeHeader :with {:title ~(t "store.offers")}
       (node :use quietButton :with {:title ~(t "store.browseMore")} :onClick ~(handler storeSearch "")))
     (row#offers :wrap true :gap 11 :height 410 :anchors {:left 0 :right 0}
       ~(repeat [g computed.offers :key g.id] `(node :id ~(str "offer-" g.id) :use offerCard :with {:g ~g})))
     (panel :color transparent :height 14 :width 1)
     (node :use storeHeader :with {:title ~(t "store.categories")})
     (row#categories :gap 12 :height 170 :anchors {:left 0 :right 0}
       (node :use categoryTile :with {:genre "Racing" :game aurora :tint "#d8357ae0"})
       (node :use categoryTile :with {:genre "Strategy" :game clockwork :tint "#2f6fd8e0"})
       (node :use categoryTile :with {:genre "Simulation" :game saltmarsh :tint "#2aa37ae0"})
       (node :use categoryTile :with {:genre "Shooter" :game glasswing :tint "#d8762fe0"}))
     (panel :color transparent :height 30 :width 1)
     ~(store-tabs)
     (row :height 600 :anchors {:left 0 :right 0} :gap 0 :alignItems start
       (column#store-list :grow 1 :height 600 :gap 5
         ~(repeat [g computed.storeRows :key g.id]
            `(node :id ~(str "store-row-" g.id)
               :use storeRow
               :with {:g ~g :lit ~(= computed.storePreview.id g.id)}
               :onHover {:set state.storeHover :to ~g.id})))
       (node#store-preview :width 308
         :height 592
         :color transparent
         :gradient {:kind linear :stops [@store.preview @store.previewEnd]}
         ~(store-preview-column)))))

(defcomponent storeResults "The storeResults component."
  {}
  `(column#store-results :height ~(+ 110 (* 74 (max 1 (count computed.results)))) :anchors {:left 0 :right 0} :gap 5
     (row :height 50 :anchors {:left 0 :right 0} :alignItems center :gap 10
       (label :text ~(t "store.results" {:n (count computed.results) :query state.storeQuery})
         :fitText true
         :textColor @store.text)
       (panel :color transparent :grow 1 :height 1)
       (node :use quietButton
         :with {:title ~(t "store.clear") :icon circle-x}
         :onClick {:set state.storeQuery :to ""}))
     (label :when ~(= (count computed.results) 0)
       :text ~(t "store.noResults")
       :textColor @color.muted
       :height 40
       :anchors {:left 0 :right 0})
     ~(repeat [g computed.results :key g.id] `(node :id ~(str "result-" g.id) :use storeRow :with {:g ~g}))))

(defcomponent requirements "The requirements component."
  {:g {} :level ""}
  `(column :grow 1 :height 150 :gap 4
     (label :font caps
       :text ~(upper-case (t (str "req." level)))
       :textColor @store.text
       :height 22
       :anchors {:left 0 :right 0})
     ~(repeat [r ["os" "cpu" "memory" "gpu" "storage"]]
        `(label :font small
           :height 20
           :anchors {:left 0 :right 0}
           :runs [{:text ~(str (t (str "req." r)) ": ") :bold true :color @store.text}
                  {:text ~(if (= r "storage")
                            (t "req.storageValue" {:n (ceil (* g.sizeGb (if (= level "recommended") 1.5 1)))})
                            (t (str "req." state.reqTab "." r "." level)))
                   :color @color.muted}]))))

(defcomponent storeApp
  "A game's store page: the gallery and its strip, the header capsule with reviews and tags, buying, the description, requirements and reviews."
  {:g {}}
  `(column#store-app :height 1670 :anchors {:left 0 :right 0}
     ~(store-app-row g)
     (row :height 48 :anchors {:left 0 :right 0} :alignItems center
       (label#store-title :font title :text ~g.title :textColor @color.bright :grow 1 :height 40)
       (node :use quietButton :with {:title ~(t "game.section.hub")}))
     ~(store-top g)
     (panel :color transparent :height 16 :width 1)
     ~(store-queue g)
     (panel :color transparent :height 20 :width 1)
     (row :height 480 :anchors {:left 0 :right 0} :gap 16 :alignItems start
       ~(store-app-column-view g)
       (column :width 308 :height 480 :gap 12
         (column#features :height 196 :anchors {:left 0 :right 0} :color @store.card :gap 2
           (panel :color transparent :height 6 :width 1)
           ~(repeat [f
                     (filter (fn [it] it.on)
                             [{:id "single" :icon "user" :on true}
                              {:id "coop" :icon "users" :on (contains g.tags "Co-op")}
                              {:id "achievements" :icon "trophy" :on true}
                              {:id "controller" :icon "gamepad-2" :on (not= g.controller "none")}
                              {:id "cloud" :icon "cloud" :on true}
                              {:id "family" :icon "share-2" :on true}])]
              `(row :height 30 :anchors {:left 8 :right 8} :gap 8 :alignItems center :color "#ffffff0a"
                 (label :icon ~f.icon
                   :text ""
                   :width 30
                   :height 30
                   :align center
                   :textColor @store.chipText
                   :color "#00000033")
                 (label :font small
                   :text ~(if (= f.id "controller")
                            (t (str "controller." g.controller))
                            (t (str "store.feature." f.id)))
                   :grow 1
                   :textColor @store.chipText
                   :height 30
                   :align {:horizontal start :vertical center}))))
         ~(languages)))
     (column :height 34 :anchors {:left 0 :right 0}
       (panel :color transparent :height 10 :width 1)
       (label :font caps
         :text ~(upper-case (t "store.reviews"))
         :textColor @color.bright
         :height 20
         :anchors {:left 0 :right 0})
       (panel :height 1
         :anchors {:left 0 :right 0}
         :color transparent
         :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}))
     (panel :color transparent :height 10 :width 1)
     ~(review-summary g)
     (panel :color transparent :height 10 :width 1)
     (column#reviews :height 432 :anchors {:left 0 :right 0} :gap 10 ~(repeat [r state.reviews] (reviews-row-3 r)))))

(defview store-top "#store-top, in storeApp." [g]
  `(row#store-top :height 424 :anchors {:left 0 :right 0} :gap 16 :color @store.card
     (column#gallery :grow 1 :height 424 :gap 8
       (image :height ~(min 338 (round (* (- computed.storeWidth 340) 0.5625)))
         :anchors {:left 0 :right 0}
         :fit cover
         :color "#000000"
         :file ~(str "../assets/art/" g.id "/" (get ["hero" "shot1" "shot2" "shot3"] state.shotIndex) ".svg"))
       (row#gallery-strip :height 72 :anchors {:left 0 :right 0} :gap 4 :arrows horizontal
         ~(repeat [s (range 4)] (store-view-view g s))))
     ~(store-side g)
     (panel :color transparent :width 10 :height 1)))

(defview review-summary "#review-summary, in storeApp." [g]
  `(row#review-summary :height 56 :anchors {:left 0 :right 0} :gap 10 :alignItems center :color @store.card
     (panel :color transparent :width 4 :height 1)
     (label :text ~(t "store.overallReviews") :fitText true :font caps :textColor @store.text)
     (label :text ~(t (str "review." g.review)) :fitText true :font strong :textColor @store.chipText)
     (label :text ~(t "store.reviewCount" {:n (formatNumber g.reviewCount)})
       :fitText true
       :font small
       :textColor @color.muted)
     (panel :color transparent :grow 1 :height 1)
     (node :width 160 :height 8 :radius 4 :color @store.down
       (node :height 8
         :radius 4
         :color @store.chipText
         :width ~(str (text g.reviewPercent) "%")
         :anchors {:left 0 :top 0}))
     (label :text ~(str g.reviewPercent "%") :fitText true :font small :textColor @color.muted)
     (panel :color transparent :width 8 :height 1)))

(defcomponent storeCart
  "The cart: the games and their prices, the estimated total from the wallet, and a receipt after buying."
  {}
  `(column#store-cart-page :height ~(+ 140
                                       (max 250
                                            (+ (* (count computed.cartGames) 106) 60)
                                            (+ (* (count state.receipt) 40) 160)))
     :anchors {:left 0 :right 0}
     (label :font title
       :text ~(upper-case (t "store.yourCart"))
       :textColor @color.bright
       :height 60
       :anchors {:left 0 :right 0})
     (row :height ~(max 250 (+ (* (count computed.cartGames) 106) 60) (+ (* (count state.receipt) 40) 160))
       :anchors {:left 0 :right 0}
       :gap 16
       :alignItems start
       ~(store-cart-page-column)
       ~(cart-summary))))

(defcomponent storeWishlist
  "The wishlist: ranked games with their price and Add to Cart, sorted by rank, name, price or discount, and moved up or down."
  {}
  `(column#store-wishlist-page :height ~(+ 160 (* (max 1 (count computed.wishSorted)) 128))
     :anchors {:left 0 :right 0}
     (row :height 60 :anchors {:left 0 :right 0} :alignItems center :gap 12
       (image :file ~(str "../assets/art/avatars/" state.user.avatar ".svg") :width 40 :height 40 :radius 2)
       (label :font title
         :text ~(upper-case (t "store.whoseWishlist" {:name state.user.name}))
         :textColor @color.bright
         :grow 1
         :height 40))
     (row :height 52 :anchors {:left 0 :right 0} :alignItems center :gap 10
       (textarea#wish-search :width 300
         :height 32
         :padding 10
         :bind state.wishQuery
         :placeholder ~(t "store.wishSearch")
         :placeholderColor @color.dim
         :textColor @store.text
         :color @store.card
         :radius 3
         :label ~(t "store.wishSearch")
         :focused {:borderWidth 1 :borderColor @color.accent})
       (panel :color transparent :grow 1 :height 1)
       (label :text ~(t "store.sortBy") :fitText true :font small :textColor @color.muted)
       ~(wish-sort))
     (panel :color transparent :height 8 :width 1)
     (label :when ~(= (count computed.wishSorted) 0)
       :text ~(t "store.wishEmpty")
       :height 60
       :anchors {:left 0 :right 0}
       :align center
       :textColor @color.muted)
     (column#wish-list :height ~(* (count computed.wishSorted) 128) :anchors {:left 0 :right 0} :gap 8
       ~(repeat [g computed.wishSorted :index i :key g.id] (store-view-3 g i)))))

(defcomponent storePage
  "The store: the top links and the store bar, then the front page, search results, a game's page, the cart or the wishlist."
  {}
  `(node#store :fill true
     :color @color.page
     :gradient {:kind linear :stops [{:at 0 :color @store.glow} {:at 0.35 :color @color.page}]}
     (column#store-scroll :fill true :scroll vertical
       (column#store-content :height ~computed.storeHeight
         :anchors {:left ~(/ (- window.width computed.storeWidth) 2)
                   :right ~(/ (- window.width computed.storeWidth) 2)}
         (node :use storeTop)
         (node :when ~(= computed.storeView "home") :use storeHome)
         (node :when ~(= computed.storeView "results") :use storeResults)
         (node :when ~(and (= computed.storeView "app") (some? computed.game))
           :use storeApp
           :with {:g ~computed.game})
         (node :when ~(= computed.storeView "cart") :use storeCart)
         (node :when ~(= computed.storeView "wishlist") :use storeWishlist)))
     (node :use dialog
       :with {:open state.cartDialog
              :title ~(t "store.addedTitle")
              :message ~(t "store.addedText"
                           {:title (or-else (:title (find (fn [it] (= it.id state.cartAdded)) data.games)) "")})
              :buttons [{:title ~(t "store.viewCart") :run showCart} {:title ~(t "store.keepShopping")}]})))

(deftokens store-colours "The store's colours."
  {:store.glow {:dark "#3f2f4c" :light "#f0e3d2"}
   :store.card {:dark "#0000003d" :light "#ffffffd0"}
   :store.cardHover {:dark "#ffffff14" :light "#ffffff"}
   :store.text {:dark "#e2d8e6" :light "#2a2030"}
   :store.label {:dark "#76687f" :light "#7a6b84"}
   :store.chip {:dark "#ffc46b33" :light "#a658001f"}
   :store.chipText {:dark "#ffc46b" :light "#94500a"}
   :store.priceBox {:dark "#000000a0" :light "#efe6f1"}
   :store.oldPrice {:dark "#8d7f96" :light "#85778e"}
   :store.final {:dark "#7ff0cf" :light "#137a62"}
   :store.price {:dark "#ffffff" :light "#150f1a"}
   :store.preview {:dark "#c6b1d4" :light "#e5d8ec"}
   :store.previewEnd {:dark "#a68fb8" :light "#d2c0dc"}
   :store.ink {:dark "#140f18" :light "#140f18"}
   :store.inkSoft {:dark "#4a3758" :light "#4a3758"}
   :store.cta {:dark "#2fbf98" :light "#23a684"}
   :store.ctaEnd {:dark "#1d8a6f" :light "#177a62"}
   :store.onCta {:dark "#e0fff5" :light "#f0fffa"}
   :store.navA {:dark "#5a3a6e" :light "#5a3a6e"}
   :store.navB {:dark "#6f4683" :light "#6f4683"}
   :store.navC {:dark "#2a1638" :light "#3a2050"}
   :store.field {:dark "#4d3560" :light "#45305a"}
   :store.dot {:dark "#ffffff26" :light "#2a203026"}
   :store.dotOn {:dark "#ffffff70" :light "#2a203080"}
   :store.down {:dark "#a34c25" :light "#b4532a"}})

(defpart store "What store adds."
  :fonts {:title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [icons]}
          :price {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24 :bold true :fallback [icons]}})

;;; Views

(defview price-tag-node "#price-tag-node." [g big]
  `(node :when ~(not g.owned)
     :grow 1
     :height ~(if big 40 32)
     :color @store.priceBox
     :radius ~(if (> g.discount 0) {:topRight 2 :bottomRight 2} 2)
     (label :when ~(> g.discount 0)
       :font small
       :text ~(formatCurrency g.price "EUR")
       :textColor @store.oldPrice
       :fitText true
       :anchors {:right 6 :top ~(if big 3 1)}
       (panel :height 1 :anchors {:left 0 :right 0 :top 8} :color @store.oldPrice))
     (label :font ~(if big "strong" "default")
       :fitText true
       :text ~(formatCurrency (/ (round (* g.price (- 100 g.discount))) 100) "EUR")
       :textColor ~(if (> g.discount 0) "@store.final" "@store.price")
       :anchors ~(if (> g.discount 0) {:right 6 :bottom (if big 4 1)} {:right 8 :top (if big 11 7)}))))

(defview store-top-row "#store-top-row." []
  `(row :height 40 :anchors {:left 0 :right 0} :alignItems center :gap 3
     (panel :color transparent :grow 1 :height 1)
     (label#store-wishlist :font small
       :fitText true
       :padding 14
       :text ~(str (upper-case (t "store.wishlist")) " (" (count state.wishlist) ")")
       :color @store.chip
       :textColor @store.text
       :radius 2
       :focusable true
       :role button
       :cursor hand
       :hover {:textColor "#ffffff" :color "#ffc46b66"}
       :onClick showWishlist)
     (label#store-cart :when ~(> (count state.cart) 0)
       :font small
       :fitText true
       :padding 14
       :text ~(str (upper-case (t "store.cart")) " (" (count state.cart) ")")
       :radius 2
       :color transparent
       :gradient {:kind linear :angle 90 :stops [@store.cta @store.ctaEnd]}
       :textColor @store.onCta
       :focusable true
       :role button
       :cursor hand
       :hover {:textColor "#ffffff"}
       :onClick showCart)))

(defview store-view "#." [item]
  `(label :id ~(str "store-bar-" item)
     :let {:later ~(contains ["points" "labs"] item)}
     :fitText true
     :padding 13
     :height 30
     :radius 15
     :text ~(t (str "store.bar." item))
     :textColor ~(if later "@color.dim" "@color.text")
     :disabled ~later
     :tooltip ~(if later (t "store.later") "")
     :focusable true
     :role button
     :cursor hand
     :hover {:color @color.hover :textColor @color.bright}
     :isOpen ~(and (= item "categories") state.storeCatsOpen)
     :onClick [{:if ~(= item "categories")
                :then [{:toggle state.storeCatsOpen}]
                :else [{:if ~(= item "news")
                        :then [~(handler libraryHome) {:after 50 :do [{:scrollTo "#news-header"}]}]
                        :else [~(handler storeHome)
                               {:if ~(= item "noteworthy")
                                :then [{:set state.storeList :to new} {:after 50 :do [{:scrollTo "#store-tabs"}]}]}]}]}]
     (node :when ~(= item "categories")
       :use menu
       :with {:open state.storeCatsOpen
              :width 200
              :items ~(map (fn [it] {:title (t (str "genre." it)) :run "storeSearch" :with {:query it}})
                           ["Action"
                            "Adventure"
                            "Horror"
                            "Platformer"
                            "Puzzle"
                            "Racing"
                            "RPG"
                            "Shooter"
                            "Simulation"
                            "Strategy"])})))

(defview store-search "#store-search." []
  `(textarea#store-search :width 200
     :height 26
     :padding 30
     :bind state.storeQuery
     :placeholder ~(t "store.search")
     :placeholderColor @color.dim
     :textColor @color.text
     :color @color.field
     :radius 13
     :label ~(t "store.search")
     :borderWidth 1
     :borderColor @color.border
     :focused {:borderColor @color.accent}
     (label :icon search
       :text ""
       :width 24
       :height 22
       :align center
       :radius 11
       :textColor @color.muted
       :anchors {:right 2 :top 2}
       :color transparent)))

(defview featured-side-column "#featured-side-column." []
  `(column :anchors {:left 10 :right 10 :top 8 :bottom 10} :gap 6
     (label :font title :text ~computed.feature.title :textColor "#ffffff" :height 38 :anchors {:left 0 :right 0})
     (row :wrap true :gap 4 :height 176 :anchors {:left 0 :right 0}
       ~(repeat [n [1 2 3 4]]
          `(node :width 150
             :height 86
             :cursor hand
             :color "#000000"
             :onHover {:set state.storeShot :to ~n}
             :onLeave {:set state.storeShot :to nil}
             (image :fill true
               :fit cover
               :file ~(str "../assets/art/" computed.feature.id "/" (if (< n 4) (str "shot" n) "hero") ".svg"))
             (node :fill true :color ~(if (= state.storeShot n) "transparent" "#00000040")))))
     (label :font heading
       :text ~(t "store.nowAvailable")
       :textColor "#ffffff"
       :height 26
       :anchors {:left 0 :right 0})
     (row :height 24 :anchors {:left 0 :right 0} :gap 4 :alignItems center
       (label :text ~(t "store.topSeller")
         :fitText true
         :font small
         :padding 7
         :radius 2
         :color "#ffffff26"
         :textColor "#ffffff")
       ~(repeat [tag (slice computed.feature.tags 0 3)]
          `(label :font small :text ~tag :fitText true :padding 7 :radius 2 :color "#ffffff14" :textColor "#e2d8e6")))
     (panel :color transparent :height 1 :grow 1)
     (row :height 32 :anchors {:left 0 :right 0} :alignItems center
       (label :text "" :fitText true :icon monitor :textColor "#e2d8e6")
       (panel :color transparent :grow 1 :height 1)
       (node :use priceTag :with {:g ~computed.feature}))))

(defview featured-row "#featured-row." []
  `(row :height 353 :anchors {:left 46 :right 46} :shadow {:y 0 :blur 10 :color "#00000080"}
     (node#featured-capsule :grow 1
       :height 353
       :focusable true
       :role button
       :cursor hand
       :label ~computed.feature.title
       :onClick ~(handler openStore computed.feature.id)
       :children [(image :fill true
                    :fit cover
                    :file ~(str "../assets/art/"
                                computed.feature.id
                                "/"
                                (cond (nil? state.storeShot) "hero"
                                      (< state.storeShot 4) (str "shot" state.storeShot)
                                      :else "hero")
                                ".svg"))]
       :color transparent)
     (node#featured-side :width 324
       :height 353
       :color transparent
       :gradient {:kind linear :stops ["#1a1220" "#2a1d33"]}
       ~(featured-side-column))))

(defview featured-prev "#featured-prev." []
  `(label#featured-prev :icon chevron-left
     :mirror true
     :text ""
     :font nav
     :width 34
     :height 66
     :align center
     :anchors {:left 52 :top 143}
     :radius 3
     :color "#00000033"
     :textColor "#ffffff"
     :focusable true
     :role button
     :cursor hand
     :label ~(t "store.previous")
     :hover {:color "#ffffff26"}
     :onClick {:set state.storeFeature :to ~(mod (+ state.storeFeature 4) 5)}))

(defview featured-next "#featured-next." []
  `(label#featured-next :icon chevron-right
     :mirror true
     :text ""
     :font nav
     :width 34
     :height 66
     :align center
     :anchors {:right 376 :top 143}
     :radius 3
     :color "#00000033"
     :textColor "#ffffff"
     :focusable true
     :role button
     :cursor hand
     :label ~(t "store.next")
     :hover {:color "#ffffff26"}
     :onClick {:set state.storeFeature :to ~(mod (+ state.storeFeature 1) 5)}))

(defview store-tabs "#store-tabs." []
  `(row#store-tabs :height 34 :anchors {:left 0 :right 0} :arrows horizontal :role tablist
     ~(repeat [tab ["new" "top" "specials"]]
        `(label :id ~(str "store-tab-" tab)
           :fitText true
           :padding 14
           :height 34
           :text ~(t (str "store.list." tab))
           :radius {:topLeft 3 :topRight 3}
           :role tab
           :focusable true
           :cursor hand
           :selected ~(= state.storeList tab)
           :color ~(if (= state.storeList tab) "@store.card" "transparent")
           :textColor ~(if (= state.storeList tab) "@color.bright" "@store.chipText")
           :hover {:textColor @color.bright}
           :onClick [{:set state.storeList :to ~tab} {:set state.storeHover :to nil}]))))

(defview store-preview-column "#store-preview-column." []
  `(column :anchors {:left 14 :right 14 :top 12 :bottom 12} :gap 6
     (label :font heading
       :text ~computed.storePreview.title
       :textColor @store.ink
       :height 26
       :anchors {:left 0 :right 0})
     (row :height 20 :anchors {:left 0 :right 0} :gap 6
       (label :text ~(t "store.overall") :fitText true :font small :textColor @store.inkSoft)
       (label :text ~(t (str "review." computed.storePreview.review)) :fitText true :font small :textColor "#5a2f70")
       (label :text ~(str "(" (formatNumber computed.storePreview.reviewCount) ")")
         :fitText true
         :font small
         :textColor @store.inkSoft))
     (row :height 24 :anchors {:left 0 :right 0} :gap 4
       ~(repeat [tag (slice computed.storePreview.tags 0 4)]
          `(label :font small
             :text ~tag
             :fitText true
             :padding 6
             :radius 2
             :color "#ffffff55"
             :textColor @store.inkSoft)))
     ~(repeat [n [1 2 3]]
        `(image :height 158
           :anchors {:left 0 :right 0}
           :fit cover
           :file ~(str "../assets/art/" computed.storePreview.id "/shot" n ".svg")))))

(defview store-app-row "#store-app-row." [g]
  `(row :height 22 :anchors {:left 0 :right 0} :gap 6 :alignItems center
     (label :text ~(t "store.allGames")
       :fitText true
       :font small
       :textColor @color.muted
       :cursor hand
       :role link
       :focusable true
       :hover {:textColor @color.bright}
       :onClick storeHome)
     (label :text "" :fitText true :icon chevron-right :mirror true :font small :textColor @color.dim)
     (label :text ~(t (str "genre." g.genre))
       :fitText true
       :font small
       :textColor @color.muted
       :cursor hand
       :role link
       :focusable true
       :hover {:textColor @color.bright}
       :onClick ~(handler storeSearch g.genre))
     (label :text "" :fitText true :icon chevron-right :mirror true :font small :textColor @color.dim)
     (label :text ~g.title :fitText true :font small :textColor @color.muted)))

(defview store-view-view "#." [g s]
  `(node :id ~(str "shot-" s)
     :width 116
     :height 65
     :focusable true
     :role button
     :cursor hand
     :label ~(t "store.screenshot" {:n (+ s 1)})
     :selected ~(= state.shotIndex s)
     :onClick {:set state.shotIndex :to ~s}
     :children [(image :fill true
                  :fit cover
                  :file ~(str "../assets/art/" g.id "/" (get ["hero" "shot1" "shot2" "shot3"] s) ".svg"))
                {:fill true
                 :color ~(if (= state.shotIndex s) "transparent" "#00000055")
                 :borderWidth ~(if (= state.shotIndex s) 2 0)
                 :borderColor "#ffffff"}]
     :color transparent))

(defview store-side "#store-side." [g]
  `(column#store-side :width 314 :height 424 :gap 4
     (image :height 151 :anchors {:left 0 :right 0} :fit cover :file ~(str "../assets/art/" g.id "/hero.svg"))
     (panel :color transparent :height 4 :width 1)
     (label :font small :text ~g.blurb :wrap true :height 40 :anchors {:left 0 :right 0} :textColor @store.text)
     (row :height 20 :anchors {:left 0 :right 0}
       (label :font small
         :text ~(upper-case (t "store.recentReviews"))
         :width 110
         :textColor @store.label
         :height 20
         :align {:horizontal start :vertical center})
       (label :font small
         :text ~(t (str "review." g.review))
         :grow 1
         :textColor @store.chipText
         :height 20
         :align {:horizontal start :vertical center}))
     (row :height 20 :anchors {:left 0 :right 0}
       (label :font small
         :text ~(upper-case (t "store.allReviews"))
         :width 110
         :textColor @store.label
         :height 20
         :align {:horizontal start :vertical center})
       (label :font small
         :text ~(str (t (str "review." g.review)) " (" (formatNumber g.reviewCount) ")")
         :grow 1
         :textColor @store.chipText
         :height 20
         :align {:horizontal start :vertical center}))
     (row :height 20 :anchors {:left 0 :right 0}
       (label :font small
         :text ~(upper-case (t "game.release"))
         :width 110
         :textColor @store.label
         :height 20
         :align {:horizontal start :vertical center})
       (label :font small
         :text ~g.release
         :grow 1
         :textColor @store.text
         :height 20
         :align {:horizontal start :vertical center}))
     (panel :color transparent :height 4 :width 1)
     (row :height 20 :anchors {:left 0 :right 0}
       (label :font small
         :text ~(upper-case (t "game.developer"))
         :width 110
         :textColor @store.label
         :height 20
         :align {:horizontal start :vertical center})
       (label :font small
         :text ~g.developer
         :grow 1
         :textColor @store.chipText
         :height 20
         :align {:horizontal start :vertical center}))
     (row :height 20 :anchors {:left 0 :right 0}
       (label :font small
         :text ~(upper-case (t "game.publisher"))
         :width 110
         :textColor @store.label
         :height 20
         :align {:horizontal start :vertical center})
       (label :font small
         :text ~g.publisher
         :grow 1
         :textColor @store.chipText
         :height 20
         :align {:horizontal start :vertical center}))
     (panel :color transparent :height 8 :width 1)
     (label :font small :text ~(t "store.userTags") :height 18 :anchors {:left 0 :right 0} :textColor @store.label)
     (row :wrap true :gap 3 :height 26 :anchors {:left 0 :right 0}
       ~(repeat [tag g.tags]
          `(label :font small
             :text ~tag
             :fitText true
             :padding 7
             :radius 2
             :color @store.chip
             :textColor @store.chipText
             :cursor hand
             :hover {:textColor "#ffffff"}
             :onClick ~(handler storeSearch tag)))
       (label :text "+" :fitText true :font small :padding 7 :radius 2 :color @store.chip :textColor @store.chipText))))

(defview store-queue "#store-queue." [g]
  `(row#store-queue :height 44 :anchors {:left 0 :right 0} :gap 6 :alignItems center :color @store.card
     (panel :color transparent :width 4 :height 1)
     (node#store-wish :use quietButton
       :with {:title ~(if (contains state.wishlist g.id) (t "store.onWishlist") (t "store.addWishlist"))
              :icon ~(if (contains state.wishlist g.id) "check" "")
              :on ~(contains state.wishlist g.id)}
       :onClick ~(handler toggleWishlist g.id))
     (node#store-follow :use quietButton
       :with {:title ~(if (contains state.followed g.id) (t "store.following") (t "store.follow"))
              :icon ~(if (contains state.followed g.id) "check" "")
              :on ~(contains state.followed g.id)}
       :onClick ~(handler toggleFollow g.id))
     (node :use quietButton :with {:title ~(t "store.ignore")})
     (panel :color transparent :grow 1 :height 1)
     (node :use quietButton :with {:title ~(t "store.queue") :icon list})
     (panel :color transparent :width 4 :height 1)))

(defview purchase-row "#purchase-row." [g]
  `(row :height 44
     :anchors {:right 14 :bottom 12}
     :width ~(cond g.owned 118 (> g.discount 0) 262 :else 200)
     :color "#000000"
     :radius 3
     :alignItems center
     :gap 6
     (panel :color transparent :grow 1 :height 1)
     (node :when ~(not g.owned) :use priceTag :with {:g ~g :big false})
     (node#add-to-cart :when ~(not g.owned)
       :use ctaButton
       :with {:title ~(if (contains state.cart g.id) (t "store.inCart") (t "store.addToCart"))}
       :onClick [{:if ~(contains state.cart g.id) :then [~(handler showCart)] :else [~(handler addToCart g.id)]}])
     (node :when ~g.owned :use ctaButton :with {:title ~(t "store.playNow")} :onClick ~(handler openGame g.id))
     (panel :color transparent :width 2 :height 1)))

(defview purchase-box "#purchase." [g]
  `(node#purchase :height 104
     :anchors {:left 0 :right 0}
     :radius 4
     :color transparent
     :gradient {:kind linear :angle 120 :stops ["#5e4b6c80" "#4a385a66"]}
     (label :when ~(not g.owned)
       :font heading
       :text ~(t "store.buy" {:title g.title})
       :textColor "#ffffff"
       :fitText true
       :anchors {:left 16 :top 14})
     (label :when ~g.owned
       :font heading
       :text ~(t "store.owned" {:title g.title})
       :textColor "#ffffff"
       :fitText true
       :anchors {:left 16 :top 14})
     (label :when ~(and (not g.owned) (> g.discount 0))
       :font small
       :text ~(upper-case (t "store.promo" {:date "2 Nov"}))
       :textColor "#ffd9a0"
       :fitText true
       :anchors {:left 16 :top 42})
     (label :text "" :fitText true :icon monitor :textColor "#e2d8e6" :anchors {:right 16 :top 16})
     ~(purchase-row g)))

(defview store-app-column "#store-app-column." []
  `(column :height 34 :anchors {:left 0 :right 0}
     (panel :color transparent :height 10 :width 1)
     (label :font caps
       :text ~(upper-case (t "store.about"))
       :textColor @color.bright
       :height 20
       :anchors {:left 0 :right 0})
     (panel :height 1
       :anchors {:left 0 :right 0}
       :color transparent
       :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]})))

(defview store-app-column-view "#store-app-column-view." [g]
  `(column :grow 1 :height 480
     ~(purchase-box g)
     (panel :color transparent :height 6 :width 1)
     ~(store-app-column)
     (label :text ~(str g.blurb
                        " "
                        (t "store.aboutText" {:title g.title :genre (lower-case (t (str "genre." g.genre)))}))
       :wrap true
       :height 76
       :anchors {:left 0 :right 0}
       :textColor @store.text
       :padding 0)
     (column :height 34 :anchors {:left 0 :right 0}
       (panel :color transparent :height 10 :width 1)
       (label :font caps
         :text ~(upper-case (t "store.requirements"))
         :textColor @color.bright
         :height 20
         :anchors {:left 0 :right 0})
       (panel :height 1
         :anchors {:left 0 :right 0}
         :color transparent
         :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}))
     ~(req-tabs)
     (row :height 150 :anchors {:left 0 :right 0} :gap 20
       (node :use requirements :with {:g ~g :level minimum})
       (node :use requirements :with {:g ~g :level recommended}))))

(defview req-tabs "#req-tabs, in store-app-column-view." []
  `(row#req-tabs :height 40 :anchors {:left 0 :right 0} :gap 2 :alignItems center :arrows horizontal :role tablist
     ~(repeat [p ["windows" "macos" "linux"]]
        `(label :id ~(str "req-" p)
           :font small
           :text ~(upper-case (t (str "req.platform." p)))
           :fitText true
           :padding 12
           :radius 2
           :role tab
           :focusable true
           :cursor hand
           :selected ~(= state.reqTab p)
           :color ~(if (= state.reqTab p) "@store.chip" "transparent")
           :textColor ~(if (= state.reqTab p) "@color.bright" "@store.chipText")
           :onClick {:set state.reqTab :to ~p}))))

(defview languages "#languages." []
  `(column#languages :height 150 :anchors {:left 0 :right 0} :color @store.card
     (panel :color transparent :height 8 :width 1)
     (row :height 22 :anchors {:left 10 :right 10}
       (label :font caps
         :text ~(t "store.languages")
         :grow 1
         :textColor @store.text
         :height 22
         :align {:horizontal start :vertical center})
       ~(repeat [c ["interface" "audio" "subtitles"]]
          `(label :font small
             :text ~(t (str "store.lang." c))
             :width 62
             :align center
             :textColor @color.muted
             :height 22)))
     ~(repeat [l [["en" true true true] ["es" true false true] ["fr" true false true]]]
        `(row :height 26 :anchors {:left 10 :right 10}
           (label :font small
             :text ~(t (str "lang." l.0))
             :grow 1
             :textColor @store.text
             :height 26
             :align {:horizontal start :vertical center})
           ~(repeat [k [1 2 3]]
              `(label :icon ~(if (get l k) "check" "")
                 :text ""
                 :width 62
                 :align center
                 :textColor @color.ingame
                 :height 26))))))

(defview reviews-row "#reviews-row." [r]
  `(row :height 36 :anchors {:left 10 :right 6} :gap 8 :alignItems center
     (image :file ~(str "../assets/art/avatars/" r.avatar ".svg")
       :width 34
       :height 34
       :radius 2
       :borderWidth 1
       :borderColor "#6b5280")
     (label :font strong
       :text ~r.author
       :grow 1
       :textColor "#f0d9c0"
       :height 36
       :align {:horizontal start :vertical center})))

(defview reviews-row-view "#reviews-row-view." [r]
  `(row :height 40 :anchors {:left 0 :right 10} :gap 10 :alignItems center :color "#00000033"
     (label :icon ~(if r.up "thumbs-up" "thumbs-down")
       :text ""
       :font heading
       :width 40
       :height 40
       :align center
       :textColor ~(if r.up "@store.chipText" "@store.down")
       :color "#0000004d")
     (column :grow 1 :height 36
       (label :font heading
         :text ~(t (if r.up "store.recommended" "store.notRecommended"))
         :height 20
         :anchors {:left 0 :right 0}
         :textColor "#d6d7d8")
       (label :font small
         :text ~(t "store.onRecord" {:n (formatNumber r.hours 1 1)})
         :height 16
         :anchors {:left 0 :right 0}
         :textColor @color.muted))))

(defview reviews-row-2 "#reviews-row-2." [r]
  `(row :height 22 :anchors {:left 0 :right 0} :gap 4 :alignItems center
     (label :text ~(t "store.helpful") :fitText true :font small :textColor @color.dim)
     ~(repeat [b ["yes" "no" "funny"]]
        `(label :id ~(str "vote-" (slug r.author) "-" b)
           :let {:on ~(= (get state.votes (slug r.author)) b)}
           :font small
           :text ~(t (str "store.vote." b))
           :fitText true
           :padding 8
           :radius 2
           :color ~(if on "@color.select" "@store.chip")
           :textColor ~(if on "@color.bright" "@store.chipText")
           :focusable true
           :role button
           :selected ~on
           :cursor hand
           :hover {:textColor @color.bright}
           :onClick {:set ~(str "state.votes." (slug r.author)) :to ~(if on nil b)}))))

(defview reviews-row-3 "#reviews-row-3." [r]
  `(row :height 136 :anchors {:left 0 :right 0} :gap 0 :color @store.card
     (column :width 200 :height 136 :gap 4
       (panel :color transparent :height 10 :width 1)
       ~(reviews-row r)
       (label :font small
         :text ~(t "store.products" {:n r.products})
         :height 18
         :anchors {:left 10 :right 6}
         :textColor @color.muted)
       (label :font small
         :text ~(t "store.reviewsBy" {:n r.reviews})
         :height 18
         :anchors {:left 10 :right 6}
         :textColor @color.muted))
     (column :grow 1 :height 136 :gap 6
       (panel :color transparent :height 8 :width 1)
       ~(reviews-row-view r)
       (label :font small
         :text ~(upper-case (t "store.posted" {:date r.posted}))
         :height 16
         :anchors {:left 0 :right 0}
         :textColor @color.dim)
       (label :text ~r.text :wrap true :height 30 :anchors {:left 0 :right 14} :textColor @store.text)
       ~(reviews-row-2 r))))

(defview receipt "#receipt." []
  `(column#receipt :when ~(> (count state.receipt) 0)
     :height ~(+ (* (count state.receipt) 40) 130)
     :anchors {:left 0 :right 0}
     :color @store.card
     :gap 6
     (panel :color transparent :height 8 :width 1)
     (label :font heading
       :icon circle-check
       :text ~(str " " (t "store.thanks"))
       :height 30
       :anchors {:left 16 :right 16}
       :textColor @color.ingame)
     ~(repeat [id state.receipt]
        `(row :height 34 :anchors {:left 16 :right 16} :gap 10 :alignItems center
           (image :file ~(str "../assets/art/" id "/hero.svg") :fit cover :width 90 :height 34)
           (label :text ~(:title (find (fn [it] (= it.id id)) data.games))
             :grow 1
             :textColor @store.text
             :height 34
             :align {:horizontal start :vertical center})
           (node :use quietButton
             :with {:title ~(t "status.button.none") :icon download}
             :onClick ~(handler install id))))
     (row :height 40 :anchors {:left 16 :right 16} :alignItems center
       (node :use quietButton :with {:title ~(t "store.toLibrary")} :onClick showLibrary))))

(defview row "#row." [g]
  `(row :height 20 :anchors {:left 0 :right 0} :gap 14
     (label :text ~(t "store.remove")
       :fitText true
       :font small
       :textColor @color.muted
       :cursor hand
       :role button
       :focusable true
       :hover {:textColor @color.bright}
       :onClick ~(handler removeFromCart g.id))
     (label :text ~(t "store.moveToWishlist")
       :fitText true
       :font small
       :textColor @color.muted
       :cursor hand
       :role button
       :focusable true
       :hover {:textColor @color.bright}
       :onClick ~(handler toWishlist g.id))))

(defview store-view-2 "#." [g]
  `(row :id ~(str "cart-" g.id)
     :height 100
     :anchors {:left 0 :right 0}
     :gap 14
     :alignItems center
     :color @store.card
     :radius 2
     (panel :color transparent :width 2 :height 1)
     (image :file ~(str "../assets/art/" g.id "/hero.svg")
       :fit cover
       :width 184
       :height 69
       :cursor hand
       :onClick ~(handler openStore g.id))
     (column :grow 1 :height 69 :gap 4
       (label :font strong :text ~g.title :height 22 :anchors {:left 0 :right 0} :textColor @color.bright)
       (label :text "" :fitText true :icon monitor :font small :textColor @color.muted)
       ~(row g))
     (node :use priceTag :with {:g ~g})
     (panel :color transparent :width 8 :height 1)))

(defview store-cart-page-column "#store-cart-page-column." []
  `(column :grow 1
     :height ~(max 250 (+ (* (count computed.cartGames) 106) 60) (+ (* (count state.receipt) 40) 160))
     :gap 6
     ~(receipt)
     (column :when ~(and (= (count computed.cartGames) 0) (= (count state.receipt) 0))
       :height 120
       :anchors {:left 0 :right 0}
       :color @store.card
       (panel :color transparent :height 24 :width 1)
       (label :font heading
         :text ~(t "store.cartEmpty")
         :align center
         :height 30
         :anchors {:left 0 :right 0}
         :textColor @store.text)
       (row :height 40 :anchors {:left 0 :right 0} :justify center :alignItems center
         (node :use quietButton :with {:title ~(t "store.continue")} :onClick storeHome)))
     ~(repeat [g computed.cartGames :key g.id] (store-view-2 g))))

(defview cart-summary "#cart-summary." []
  `(column#cart-summary :width 308 :height 236 :color @store.card :radius 2 :gap 8
     (panel :color transparent :height 8 :width 1)
     (row :height 30 :anchors {:left 16 :right 16} :alignItems center
       (label :font heading
         :text ~(t "store.total")
         :grow 1
         :textColor @color.bright
         :height 30
         :align {:horizontal start :vertical center})
       (label :font heading :text ~(formatCurrency computed.cartTotal "EUR") :fitText true :textColor @color.bright))
     (label :font small
       :text ~(t "store.tax")
       :wrap true
       :height 32
       :anchors {:left 16 :right 16}
       :textColor @color.muted)
     (label :font small
       :text ~(t "store.walletLeft" {:amount (formatCurrency state.user.wallet "EUR")})
       :height 18
       :anchors {:left 16 :right 16}
       :textColor ~(if (> computed.cartTotal state.user.wallet) "@store.down" "@color.muted"))
     (row :height 44 :anchors {:left 16 :right 16} :alignItems center
       (node#purchase-button :use ctaButton
         :grow 1
         :align center
         :with {:title ~(t "store.purchase")
                :enabled ~(and (> (count state.cart) 0) (<= computed.cartTotal state.user.wallet))}
         :onClick purchase))
     (row :height 34 :anchors {:left 16 :right 16} :alignItems center
       (node :use quietButton :with {:title ~(t "store.continue")} :grow 1 :align center :onClick storeHome))))

(defview wish-sort "#wish-sort." []
  `(label#wish-sort :font small
     :fitText true
     :padding 12
     :radius 2
     :color @store.chip
     :text ~(str (t (str "store.wishSort." state.wishSort)) " ▾")
     :textColor @store.chipText
     :focusable true
     :role button
     :cursor hand
     :isOpen ~state.wishSortOpen
     :onClick {:toggle state.wishSortOpen}
     (node :use menu
       :with {:open state.wishSortOpen
              :width 180
              :items ~(map (fn [it]
                             {:title (t (str "store.wishSort." it))
                              :checked (= state.wishSort it)
                              :run "setWishSort"
                              :with {:by it}})
                           ["rank" "name" "price" "discount"])})))

(defview store-column "#store-column." [g i]
  `(column :width 34 :height 100
     (label :font strong
       :text ~(if (= state.wishSort "rank") (text (+ (indexOf state.wishlist g.id) 1)) "")
       :height 32
       :anchors {:left 0 :right 0}
       :align center
       :textColor @color.muted)
     (label :when ~(and (= state.wishSort "rank") (= state.wishQuery ""))
       :icon chevron-up
       :text ""
       :height 30
       :anchors {:left 0 :right 0}
       :align center
       :textColor ~(if (> i 0) "@store.chipText" "@color.dim")
       :role button
       :focusable true
       :cursor hand
       :label ~(t "store.moveUp")
       :hover {:textColor "#ffffff"}
       :onClick {:if ~(> i 0) :then [~(handler wishMove i (- i 1))]})
     (label :when ~(and (= state.wishSort "rank") (= state.wishQuery ""))
       :icon chevron-down
       :text ""
       :height 30
       :anchors {:left 0 :right 0}
       :align center
       :textColor ~(if (< i (- (count state.wishlist) 1)) "@store.chipText" "@color.dim")
       :role button
       :focusable true
       :cursor hand
       :label ~(t "store.moveDown")
       :hover {:textColor "#ffffff"}
       :onClick {:if ~(< i (- (count state.wishlist) 1)) :then [~(handler wishMove i (+ i 1))]})))

(defview store-column-view "#store-column-view." [g]
  `(column :grow 1 :height 100 :gap 4
     (label :font heading
       :text ~g.title
       :height 26
       :anchors {:left 0 :right 0}
       :textColor @color.bright
       :cursor hand
       :onClick ~(handler openStore g.id))
     (row :height 18 :anchors {:left 0 :right 0} :gap 6
       (label :text ~(upper-case (t "store.overall")) :fitText true :font small :textColor @store.label)
       (label :text ~(t (str "review." g.review)) :fitText true :font small :textColor @store.chipText)
       (panel :color transparent :width 10 :height 1)
       (label :text ~(upper-case (t "game.release")) :fitText true :font small :textColor @store.label)
       (label :text ~g.release :fitText true :font small :textColor @store.text))
     (row :height 24 :anchors {:left 0 :right 0} :gap 4
       ~(repeat [tag (slice g.tags 0 4)]
          `(label :font small
             :text ~tag
             :fitText true
             :padding 7
             :radius 2
             :color @store.chip
             :textColor @store.chipText)))
     (label :font small
       :text ~(t "store.added" {:date "12 Aug"})
       :height 18
       :anchors {:left 0 :right 0}
       :textColor @color.dim)))

(defview store-column-2 "#store-column-2." [g]
  `(column :width 290 :height 100 :gap 8
     (panel :color transparent :height 16 :width 1)
     (row :height 36 :anchors {:left 0 :right 0} :gap 4 :alignItems center
       (panel :color transparent :grow 1 :height 1)
       (node :use priceTag :with {:g ~g})
       (node :use ctaButton
         :with {:title ~(if (contains state.cart g.id) (t "store.inCart") (t "store.addToCart"))}
         :onClick [{:if ~(contains state.cart g.id) :then [~(handler showCart)] :else [~(handler addToCart g.id)]}]))
     (row :height 18 :anchors {:left 0 :right 0}
       (panel :color transparent :grow 1 :height 1)
       (label :text ~(t "store.remove")
         :fitText true
         :font small
         :textColor @color.dim
         :cursor hand
         :role button
         :focusable true
         :hover {:textColor @color.bright}
         :onClick ~(handler toggleWishlist g.id)))))

(defview store-view-3 "#." [g i]
  `(row :id ~(str "wish-" g.id)
     :height 120
     :anchors {:left 0 :right 0}
     :gap 14
     :alignItems center
     :color @store.card
     :radius 2
     ~(store-column g i)
     (image :file ~(str "../assets/art/" g.id "/hero.svg")
       :fit cover
       :width 230
       :height 100
       :cursor hand
       :onClick ~(handler openStore g.id))
     ~(store-column-view g)
     ~(store-column-2 g)
     (panel :color transparent :width 4 :height 1)))
