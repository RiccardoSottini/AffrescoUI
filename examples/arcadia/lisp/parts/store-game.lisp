;;; store-game.lisp --- a game's page in the store
;;;
;;; The path back to the store; the gallery, whose strip chooses the picture shown, beside the game's summary; the
;;; wishlist and follow buttons; buying; what the game is, its features, languages and system requirements; and the
;;; players' reviews, each of which can be voted on. Every review is invented.

;;; What the page keeps

(defstate shotIndex 0)

(defstate reqTab windows)

(defstate followed [])

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

;;; The page

(defcomponent storeApp
  "A game's store page: the gallery, its summary, buying, what it is, its requirements and its reviews."
  {:g {}}
  `(column#store-app :height 1670 :anchors {:left 0 :right 0}
     ~(breadcrumbs g)
     (row :height 48 :anchors {:left 0 :right 0} :alignItems center
       (label#store-title :font title :text ~g.title :textColor @color.bright :grow 1 :height 40)
       (node :use quietButton :with {:title ~(t "game.section.hub")}))
     ~(showcase g)
     (panel :color transparent :height 16 :width 1)
     ~(wish-and-follow g)
     (panel :color transparent :height 20 :width 1)
     (row :height 480 :anchors {:left 0 :right 0} :gap 16 :alignItems start
       ~(about-game g)
       (column :width 308 :height 480 :gap 12 ~(features g) ~(languages)))
     ~(section-heading "store.reviews")
     (panel :color transparent :height 10 :width 1)
     ~(review-summary g)
     (panel :color transparent :height 10 :width 1)
     (column#reviews :height 432 :anchors {:left 0 :right 0} :gap 10 ~(repeat [r state.reviews] (review r)))))

(defview breadcrumbs "The way back: All games, then the game's genre (its games), then the game's name." [g]
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

(defview section-heading "A heading of the page, in capitals, over a line that fades out to the right." [key]
  `(column :height 34 :anchors {:left 0 :right 0}
     (panel :color transparent :height 10 :width 1)
     (label :font caps :text ~(upper-case (t key)) :textColor @color.bright :height 20 :anchors {:left 0 :right 0})
     (panel :height 1
       :anchors {:left 0 :right 0}
       :color transparent
       :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]})))

;;; The gallery and the summary

(defview showcase "The gallery, the picture chosen over a strip of four, beside the game's summary." [g]
  `(row#store-top :height 424 :anchors {:left 0 :right 0} :gap 16 :color @store.card
     (column#gallery :grow 1 :height 424 :gap 8
       (image :height ~(min 338 (round (* (- computed.storeWidth 340) 0.5625)))
         :anchors {:left 0 :right 0}
         :fit cover
         :color "#000000"
         :file ~(str "../assets/art/" g.id "/" (get ["hero" "shot1" "shot2" "shot3"] state.shotIndex) ".svg"))
       (row#gallery-strip :height 72 :anchors {:left 0 :right 0} :gap 4 :arrows horizontal
         ~(repeat [s (range 4)] (gallery-shot g s))))
     ~(game-summary g)
     (panel :color transparent :width 10 :height 1)))

(defview gallery-shot "A picture of the strip, dimmed unless it is the one shown, which shows it." [g s]
  `(node :id ~(str "shot-" s)
     :width 116
     :height 65
     :focusable true
     :role button
     :cursor hand
     :label ~(t "store.screenshot" {:n (+ s 1)})
     :selected ~(= state.shotIndex s)
     :onClick ~(handler showShot s)
     :color transparent
     (image :fill true
       :fit cover
       :file ~(str "../assets/art/" g.id "/" (get ["hero" "shot1" "shot2" "shot3"] s) ".svg"))
     (node :fill true
       :color ~(if (= state.shotIndex s) "transparent" "#00000055")
       :borderWidth ~(if (= state.shotIndex s) 2 0)
       :borderColor "#ffffff")))

(defstyle factName "The name of a fact of the summary, in capitals, at the start of its row."
  (label :font small :width 110 :textColor @store.label :height 20 :align {:horizontal start :vertical center}))

(defstyle factValue "A fact of the summary, after its name."
  (label :font small :grow 1 :height 20 :align {:horizontal start :vertical center}))

(defview game-summary "The game's banner and blurb; its reviews, release, developer and publisher; its tags." [g]
  `(column#store-side :width 314 :height 424 :gap 4
     (image :height 151 :anchors {:left 0 :right 0} :fit cover :file ~(str "../assets/art/" g.id "/hero.svg"))
     (panel :color transparent :height 4 :width 1)
     (label :font small :text ~g.blurb :wrap true :height 40 :anchors {:left 0 :right 0} :textColor @store.text)
     (row :height 20 :anchors {:left 0 :right 0}
       (label :style factName :text ~(upper-case (t "store.recentReviews")))
       (label :style factValue :text ~(t (str "review." g.review)) :textColor @store.chipText))
     (row :height 20 :anchors {:left 0 :right 0}
       (label :style factName :text ~(upper-case (t "store.allReviews")))
       (label :style factValue
         :text ~(str (t (str "review." g.review)) " (" (formatNumber g.reviewCount) ")")
         :textColor @store.chipText))
     (row :height 20 :anchors {:left 0 :right 0}
       (label :style factName :text ~(upper-case (t "game.release")))
       (label :style factValue :text ~g.release :textColor @store.text))
     (panel :color transparent :height 4 :width 1)
     (row :height 20 :anchors {:left 0 :right 0}
       (label :style factName :text ~(upper-case (t "game.developer")))
       (label :style factValue :text ~g.developer :textColor @store.chipText))
     (row :height 20 :anchors {:left 0 :right 0}
       (label :style factName :text ~(upper-case (t "game.publisher")))
       (label :style factValue :text ~g.publisher :textColor @store.chipText))
     (panel :color transparent :height 8 :width 1)
     (label :font small :text ~(t "store.userTags") :height 18 :anchors {:left 0 :right 0} :textColor @store.label)
     ~(user-tags g)))

(defview user-tags "The game's tags, each of which searches the store for it, and a + to add one." [g]
  `(row :wrap true :gap 3 :height 26 :anchors {:left 0 :right 0}
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
     (label :text "+" :fitText true :font small :padding 7 :radius 2 :color @store.chip :textColor @store.chipText)))

;;; Wishing, following and buying

(defview wish-and-follow "Add to (or on) the wishlist, follow (or following), ignore, and the discovery queue." [g]
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

(defview purchase-box "Buy the game, with when its promotion ends, or, once owned, that it is in the library." [g]
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
       :text ~(upper-case (t "store.promo" {:date (t "store.offerDate")}))
       :textColor "#ffd9a0"
       :fitText true
       :anchors {:left 16 :top 42})
     (label :text "" :fitText true :icon monitor :textColor "#e2d8e6" :anchors {:right 16 :top 16})
     ~(buy-row g)))

(defview buy-row "The price and Add to Cart (In Cart, which opens the cart), or Play Now for a game owned." [g]
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
       :onClick ~(handler cartOrAdd g.id))
     (node :when ~g.owned :use ctaButton :with {:title ~(t "store.playNow")} :onClick ~(handler openGame g.id))
     (panel :color transparent :width 2 :height 1)))

;;; About the game

(defview about-game "Buying, what the game is, and its system requirements by platform." [g]
  `(column :grow 1 :height 480
     ~(purchase-box g)
     (panel :color transparent :height 6 :width 1)
     ~(section-heading "store.about")
     (label :text ~(str g.blurb
                        " "
                        (t "store.aboutText" {:title g.title :genre (lower-case (t (str "genre." g.genre)))}))
       :wrap true
       :height 76
       :anchors {:left 0 :right 0}
       :textColor @store.text
       :padding 0)
     ~(section-heading "store.requirements")
     ~(req-tabs)
     (row :height 150 :anchors {:left 0 :right 0} :gap 20
       (node :use requirements :with {:g ~g :level minimum})
       (node :use requirements :with {:g ~g :level recommended}))))

(defview features "What the game offers: single player, co-op, achievements, controllers, cloud saves, sharing." [g]
  `(column#features :height 196 :anchors {:left 0 :right 0} :color @store.card :gap 2
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
             :text ~(if (= f.id "controller") (t (str "controller." g.controller)) (t (str "store.feature." f.id)))
             :grow 1
             :textColor @store.chipText
             :height 30
             :align {:horizontal start :vertical center})))))

(defview languages "The languages the game speaks: a tick for its interface, its audio and its subtitles in each."
  []
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

;;; System requirements

(defview req-tabs "The requirements' platforms: Windows, macOS and Linux." []
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
           :onClick ~(handler showRequirements p)))))

(defcomponent requirements
  "The minimum or recommended system for the platform chosen: system, processor, memory, graphics and storage."
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

;;; Reviews

(defview review-summary "The reviews' overall verdict, how many there are, and the share that recommend it." [g]
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

(defview review "A player's review: who wrote it; the verdict, the date, the text and the votes." [r]
  `(row :height 136 :anchors {:left 0 :right 0} :gap 0 :color @store.card
     (column :width 200 :height 136 :gap 4
       (panel :color transparent :height 10 :width 1)
       ~(reviewer r)
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
       ~(verdict r)
       (label :font small
         :text ~(upper-case (t "store.posted" {:date r.posted}))
         :height 16
         :anchors {:left 0 :right 0}
         :textColor @color.dim)
       (label :text ~r.text :wrap true :height 30 :anchors {:left 0 :right 14} :textColor @store.text)
       ~(helpful-votes r))))

(defview reviewer "The reviewer's avatar and name." [r]
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

(defview verdict "A thumb up or down, recommended or not, and the reviewer's hours in the game." [r]
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

(defview helpful-votes "Was this review helpful? Yes, no or funny, the vote given lit; a vote again takes it back."
  [r]
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
           :onClick ~(handler voteReview r.author b)))))
