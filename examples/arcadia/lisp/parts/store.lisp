;;; store.lisp --- Arcadia's store: its bar, its front page and its search results
;;;
;;; The wishlist and cart links over the store bar and its search; the front page with a featured carousel whose
;;; picture follows the screenshot under the pointer, special offers, categories, and tabbed lists with a preview. The
;;; parts every store page uses (prices, buttons, headings) are here too. A game's store page is in store-game.lisp;
;;; the cart and the wishlist are in cart.lisp.

;;; What the store keeps

(defstate storeQuery "")

(defstate storeFeature 0)

(defstate storeShot nil)

(defstate storeList new)

(defstate storeHover nil)

(defstate storeCatsOpen false)

;;; What the store lists

(defcomputed storeView "The store page shown: the search results while there is a search, else the page gone to."
  (if (not= state.storeQuery "") "results" computed.here.page))

(defcomputed price "Every game's price after its discount, in the order of the games."
  (map (fn [it] (/ (round (* it.price (- 100 it.discount))) 100)) data.games))

(defcomputed storeGames "The games the store sells: every game but those added from this computer."
  (filter (fn [it] (not it.local)) data.games))

(defcomputed featured "The games of the featured carousel."
  (filter (fn [it] (contains ["aurora" "glasswing" "clockwork" "rivermill" "volt"] it.id)) data.games))

(defcomputed feature "The featured game the carousel shows." (get computed.featured state.storeFeature))

(defcomputed offers "The special offers: the six games with the biggest discounts."
  (slice (sort-by (fn [it] it.discount) > (filter (fn [it] (> it.discount 0)) computed.storeGames)) 0 6))

(defcomputed storeRows "The eight games of the list's tab: the newest, the most reviewed, or the biggest discounts."
  (cond (= state.storeList "top") (slice (sort-by (fn [it] it.reviewCount) > computed.storeGames) 0 8)
        (= state.storeList "specials") (slice (sort-by (fn [it] it.discount) > (filter (fn [it] (> it.discount 0)) computed.storeGames)) 0 8)
        :else (slice (reverse computed.storeGames) 0 8)))

(defcomputed storePreview "The game previewed beside the list: the one under the pointer, else the first."
  (or-else (find (fn [it] (= it.id state.storeHover)) computed.storeRows) computed.storeRows.0))

(defcomputed results "The games whose title holds the search, or whose genre or one of whose tags is the search."
  (filter (fn [it]
            (or (contains (lower-case it.title) (lower-case state.storeQuery))
                (= (lower-case it.genre) (lower-case state.storeQuery))
                (some (fn [it] (= (lower-case it) (lower-case state.storeQuery))) it.tags)))
          computed.storeGames))

;;; The store's measures

(defmacro results-height
  ;; A macro: the height stays an expression, for the page and for the store around it.
  "The height of the search results: a heading, and a row per game found (at least one)."
  []
  `(+ 110 (* 74 (max 1 (count computed.results)))))

(defmacro cart-height
  ;; A macro: the height stays an expression, for the cart's page, its list and the store around it.
  "The height of the cart's games and summary: the games, or the receipt, or room for the summary."
  []
  `(max 250 (+ (* (count computed.cartGames) 106) 60) (+ (* (count state.receipt) 40) 160)))

(defmacro wishlist-height
  ;; A macro: the height stays an expression, for the page and for the store around it.
  "The height of the wishlist's page: its heading and search, and a row per game (at least one)."
  []
  `(+ 160 (* (max 1 (count computed.wishSorted)) 128)))

(defcomputed storeWidth "The width of the store's pages: 940, or the window's less a margin when it is narrower."
  (min 940 (- window.width 32)))

(defcomputed storeHeight "The height of the store's content: the bar, and the page shown."
  (+ 86
     (cond (= computed.storeView "results") (results-height)
           (= computed.storeView "app") 1670
           (= computed.storeView "cart") (+ 140 (cart-height))
           (= computed.storeView "wishlist") (wishlist-height)
           :else 1960)))

;;; The store's looks

(deftokens store-colours
  "The store's colours: cards, chips, prices, the list's preview, the green buttons, the dots."
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

(defpart store "The store's two bigger fonts: a page's title and a big price."
  :fonts {:title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [icons]}
          :price {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24 :bold true :fallback [icons]}})

;;; Prices, buttons and headings

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
     ~(price-box g big)))

(defview price-box "A price not owned yet: the old one struck through over the new one, or just the price." [g big]
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

;;; The page

(defcomponent storePage
  "The store: the links and the bar, then the front page, the results, a game's page, the cart or the wishlist."
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

;;; The store bar

(defcomponent storeTop
  "Wishlist and cart links over the store bar: For you, Fresh, Categories, Rewards, News, Experiments, search."
  {}
  `(column :height 86 :anchors {:left 0 :right 0}
     ~(store-links)
     (row#store-bar :height 40
       :anchors {:left 0 :right 0}
       :alignItems center
       :gap 4
       :radius 20
       :color @color.side
       :arrows horizontal
       (panel :width 1 :height 1 :color transparent)
       ~(repeat [item ["yours" "noteworthy" "categories" "points" "news" "labs"]] (bar-link item))
       (panel :color transparent :grow 1 :height 1)
       ~(store-search)
       (panel :color transparent :width 6 :height 1)
       (panel :width 1 :height 1 :color transparent))
     (panel :color transparent :height 10 :width 1)))

(defview store-links "The wishlist and, when it holds a game, the cart, each with how many games it holds." []
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

(defview bar-link "A store bar link: Categories opens a menu of genres; Rewards and Experiments are for later."
  [item]
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
     :onClick ~(handler openStoreBar item)
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

(defview store-search "The store's search field, round, with a magnifier at its end." []
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

;;; The front page

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
     ~(store-lists)))

;;; The featured carousel

(defcomponent featured
  "The featured carousel: a big capsule whose picture follows the screenshot pointed at, arrows and dots."
  {}
  `(node#featured :height 383 :anchors {:left -46 :right -46} :color transparent
     ~(featured-slide)
     ~(featured-prev)
     ~(featured-next)
     ~(featured-dots)))

(defview featured-slide "The featured game's big picture, which opens its page, and its details beside it." []
  `(row :height 353 :anchors {:left 46 :right 46} :shadow {:y 0 :blur 10 :color "#00000080"}
     (node#featured-capsule :grow 1
       :height 353
       :focusable true
       :role button
       :cursor hand
       :label ~computed.feature.title
       :onClick ~(handler openStore computed.feature.id)
       :color transparent
       (image :fill true
         :fit cover
         :file ~(str "../assets/art/"
                     computed.feature.id
                     "/"
                     (cond (nil? state.storeShot) "hero"
                           (< state.storeShot 4) (str "shot" state.storeShot)
                           :else "hero")
                     ".svg")))
     (node#featured-side :width 324
       :height 353
       :color transparent
       :gradient {:kind linear :stops ["#1a1220" "#2a1d33"]}
       ~(featured-details))))

(defview featured-details "The featured game's name, its screenshots, why it is featured, its tags and its price."
  []
  `(column :anchors {:left 10 :right 10 :top 8 :bottom 10} :gap 6
     (label :font title :text ~computed.feature.title :textColor "#ffffff" :height 38 :anchors {:left 0 :right 0})
     ~(featured-shots)
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

(defview featured-shots "Four of the featured game's pictures; the one under the pointer shows in the big picture."
  []
  `(row :wrap true :gap 4 :height 176 :anchors {:left 0 :right 0}
     ~(repeat [n [1 2 3 4]]
        `(node :width 150
           :height 86
           :cursor hand
           :color "#000000"
           :onHover ~(handler showFeatureShot n)
           :onLeave {:set state.storeShot :to nil}
           (image :fill true
             :fit cover
             :file ~(str "../assets/art/" computed.feature.id "/" (if (< n 4) (str "shot" n) "hero") ".svg"))
           (node :fill true :color ~(if (= state.storeShot n) "transparent" "#00000040"))))))

(defview featured-prev "The arrow that shows the featured game before, the last after the first." []
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
     :onClick previousFeature))

(defview featured-next "The arrow that shows the next featured game, the first after the last." []
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
     :onClick nextFeature))

(defview featured-dots "A dot per featured game, lit for the one shown, that shows its game." []
  `(row :height 12 :anchors {:left 0 :right 0 :bottom 6} :gap 3 :justify center
     ~(repeat [d (range 5)]
        `(node :width 15
           :height 9
           :radius 2
           :cursor hand
           :color ~(if (= d state.storeFeature) "@store.dotOn" "@store.dot")
           :role button
           :label ~(t "store.slide" {:n (+ d 1)})
           :onClick ~(handler showFeature d)))))

;;; Offers and categories

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
     (label :text ~(t "store.endsOn" {:date (t "store.offerDate")})
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

;;; The tabbed lists

(defview store-tabs "The lists' tabs: new and trending, top sellers, and specials." []
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
           :onClick ~(handler showStoreList tab)))))

(defview store-lists "The tab's games, the one under the pointer previewed beside them." []
  `(row :height 600 :anchors {:left 0 :right 0} :gap 0 :alignItems start
     (column#store-list :grow 1 :height 600 :gap 5
       ~(repeat [g computed.storeRows :key g.id]
          `(node :id ~(str "store-row-" g.id)
             :use storeRow
             :with {:g ~g :lit ~(= computed.storePreview.id g.id)}
             :onHover ~(handler previewGame g.id))))
     (node#store-preview :width 308
       :height 592
       :color transparent
       :gradient {:kind linear :stops [@store.preview @store.previewEnd]}
       ~(list-preview))))

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

(defview list-preview "The game previewed: its name, its reviews, its tags and three screenshots." []
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

;;; Search results

(defcomponent storeResults
  "The search results: how many games were found, a button that clears the search, the games."
  {}
  `(column#store-results :height ~(results-height) :anchors {:left 0 :right 0} :gap 5
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
