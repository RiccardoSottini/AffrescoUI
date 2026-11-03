;;; cart.lisp --- Arcadia's cart and wishlist
;;;
;;; The cart: its games with their prices, the total against the wallet, Purchase, and a receipt after buying with an
;;; install button per game. The wishlist: ranked games with their price and Add to Cart, searched, sorted by rank,
;;; name, price or discount, and moved up or down. What buying does is in arcadia.lisp (purchase).

;;; What the cart and the wishlist keep

(defstate cart [volt])

(defstate wishlist [glasswing aurora hollowsong tinyfleet])

(defstate wishSort rank)

(defstate wishSortOpen false)

(defstate wishQuery "")

(defstate cartAdded nil)

(defstate cartDialog false)

(defstate receipt [])

(defcomputed cartGames "The games in the cart." (filter (fn [it] (contains state.cart it.id)) data.games))

(defcomputed cartTotal "What the cart costs, after the discounts."
  (/ (round (sum computed.cartGames (fn [it] (* it.price (- 100 it.discount))))) 100))

(defcomputed wishGames "The wishlist's games whose title or one of whose tags holds its search."
  (filter (fn [it]
            (and (contains state.wishlist it.id)
                 (or (= state.wishQuery "")
                     (contains (lower-case it.title) (lower-case state.wishQuery))
                     (some (fn [it] (contains (lower-case it) (lower-case state.wishQuery))) it.tags))))
          data.games))

(defcomputed wishSorted "The wishlist's games in the order chosen: by rank, name, price or discount."
  (cond (= state.wishSort "name") (sort-by (fn [it] it.title) computed.wishGames)
        (= state.wishSort "price") (sort-by (fn [it] (* it.price (- 100 it.discount))) computed.wishGames)
        (= state.wishSort "discount") (sort-by (fn [it] it.discount) > computed.wishGames)
        :else (sort-by (fn [it] (indexOf state.wishlist it.id)) computed.wishGames)))

;;; The cart

(defcomponent storeCart
  "The cart: the games and their prices, the estimated total from the wallet, and a receipt after buying."
  {}
  `(column#store-cart-page :height ~(+ 140 (cart-height))
     :anchors {:left 0 :right 0}
     (label :font title
       :text ~(upper-case (t "store.yourCart"))
       :textColor @color.bright
       :height 60
       :anchors {:left 0 :right 0})
     (row :height ~(cart-height)
       :anchors {:left 0 :right 0}
       :gap 16
       :alignItems start
       ~(cart-games)
       ~(cart-summary))))

(defview cart-games "The receipt after buying, or that the cart is empty, and the games in the cart." []
  `(column :grow 1 :height ~(cart-height) :gap 6
     ~(receipt-card)
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
     ~(repeat [g computed.cartGames :key g.id] (cart-item g))))

(defview cart-item "A game in the cart: its banner, which opens its page, its name, what to do with it, its price." [g]
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
       ~(cart-item-actions g))
     (node :use priceTag :with {:g ~g})
     (panel :color transparent :width 8 :height 1)))

(defview cart-item-actions "Remove a game from the cart, or move it to the wishlist." [g]
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

(defview cart-summary "The total, the taxes note, what the wallet will have left, Purchase and Continue shopping." []
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

(defview receipt-card "After buying: thanks, each game bought with a button to install it, the way to the library." []
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

;;; The wishlist

(defcomponent storeWishlist
  "The wishlist: ranked games with their price and Add to Cart, searched, sorted, and moved up or down."
  {}
  `(column#store-wishlist-page :height ~(wishlist-height)
     :anchors {:left 0 :right 0}
     (row :height 60 :anchors {:left 0 :right 0} :alignItems center :gap 12
       (image :file ~(str "../assets/art/avatars/" state.user.avatar ".svg") :width 40 :height 40 :radius 2)
       (label :font title
         :text ~(upper-case (t "store.whoseWishlist" {:name state.user.name}))
         :textColor @color.bright
         :grow 1
         :height 40))
     ~(wish-tools)
     (panel :color transparent :height 8 :width 1)
     (label :when ~(= (count computed.wishSorted) 0)
       :text ~(t "store.wishEmpty")
       :height 60
       :anchors {:left 0 :right 0}
       :align center
       :textColor @color.muted)
     (column#wish-list :height ~(* (count computed.wishSorted) 128) :anchors {:left 0 :right 0} :gap 8
       ~(repeat [g computed.wishSorted :index i :key g.id] (wish-item g i)))))

(defview wish-tools "The wishlist's search, and the order it is sorted in." []
  `(row :height 52 :anchors {:left 0 :right 0} :alignItems center :gap 10
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
     ~(wish-sort)))

(defview wish-sort "The wishlist's order, and its menu: by rank, name, price or discount." []
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

(defview wish-item "A game of the wishlist: its rank, its banner, what it is, and its price with Add to Cart." [g i]
  `(row :id ~(str "wish-" g.id)
     :height 120
     :anchors {:left 0 :right 0}
     :gap 14
     :alignItems center
     :color @store.card
     :radius 2
     ~(wish-rank g i)
     (image :file ~(str "../assets/art/" g.id "/hero.svg")
       :fit cover
       :width 230
       :height 100
       :cursor hand
       :onClick ~(handler openStore g.id))
     ~(wish-details g)
     ~(wish-buy g)
     (panel :color transparent :width 4 :height 1)))

(defview wish-rank "A game's rank and, while ranked and not searched, the arrows that move it up or down." [g i]
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
       :onClick ~(handler wishUp i))
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
       :onClick ~(handler wishDown i))))

(defview wish-details "A wished game's name (which opens its page), reviews, release, tags, and when it was added." [g]
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
       :text ~(t "store.added" {:date (t "store.wishedDate")})
       :height 18
       :anchors {:left 0 :right 0}
       :textColor @color.dim)))

(defview wish-buy "A wished game's price, Add to Cart (In Cart opens the cart), and Remove from the wishlist." [g]
  `(column :width 290 :height 100 :gap 8
     (panel :color transparent :height 16 :width 1)
     (row :height 36 :anchors {:left 0 :right 0} :gap 4 :alignItems center
       (panel :color transparent :grow 1 :height 1)
       (node :use priceTag :with {:g ~g})
       (node :use ctaButton
         :with {:title ~(if (contains state.cart g.id) (t "store.inCart") (t "store.addToCart"))}
         :onClick ~(handler cartOrAdd g.id)))
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
