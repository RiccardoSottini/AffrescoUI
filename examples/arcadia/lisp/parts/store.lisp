{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Arcadia's store: the store bar and search, a featured carousel whose picture follows the screenshot under the pointer, special offers, categories, tabbed lists with a preview, a page per game (gallery, reviews, buying, requirements), the cart and the wishlist."
  :fonts {:title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [icons]}
    :price {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24 :bold true :fallback [icons]}}
  :tokens {:store.glow {:dark "#3f2f4c" :light "#f0e3d2"}
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
    :store.down {:dark "#a34c25" :light "#b4532a"}}
  :state {:cart [volt]
    :wishlist [glasswing aurora hollowsong tinyfleet]
    :followed []
    :storeQuery ""
    :storeFeature 0
    :storeShot nil
    :storeList new
    :storeHover nil
    :storeCatsOpen false
    :shotIndex 0
    :reqTab windows
    :wishSort rank
    :wishSortOpen false
    :wishQuery ""
    :cartAdded nil
    :cartDialog false
    :receipt []
    :votes {}
    :reviews [{:author MossyGolem
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
      {:author Pixel_Baron
        :avatar knight
        :products 611
        :reviews 88
        :up false
        :hours 3.4
        :posted "21 September"
        :text "Lovely to look at, but the middle chapters drag. I will come back after a few patches."}]}
  :computed {:storeView "{{state.storeQuery != '' ? 'results' : computed.here.page}}"
    :price "{{map(data.games, round(it.price * (100 - it.discount)) / 100)}}"
    :storeGames "{{filter(data.games, !it.local)}}"
    :featured "{{filter(data.games, contains(['aurora', 'glasswing', 'clockwork', 'rivermill', 'volt'], it.id))}}"
    :feature "{{computed.featured[state.storeFeature]}}"
    :offers "{{slice(sort(filter(computed.storeGames, it.discount > 0), it.discount, 'desc'), 0, 6)}}"
    :storeRows "{{state.storeList == 'top' ? slice(sort(computed.storeGames, it.reviewCount, 'desc'), 0, 8) : state.storeList == 'specials' ? slice(sort(filter(computed.storeGames, it.discount > 0), it.discount, 'desc'), 0, 8) : slice(reverse(computed.storeGames), 0, 8)}}"
    :storePreview "{{find(computed.storeRows, it.id == state.storeHover) ?? computed.storeRows[0]}}"
    :results "{{filter(computed.storeGames, contains(lower(it.title), lower(state.storeQuery)) || lower(it.genre) == lower(state.storeQuery) || some(it.tags, lower(it) == lower(state.storeQuery)))}}"
    :cartGames "{{filter(data.games, contains(state.cart, it.id))}}"
    :cartTotal "{{round(sum(computed.cartGames, it.price * (100 - it.discount))) / 100}}"
    :wishGames "{{filter(data.games, contains(state.wishlist, it.id) && (state.wishQuery == '' || contains(lower(it.title), lower(state.wishQuery)) || some(it.tags, contains(lower(it), lower(state.wishQuery)))))}}"
    :wishSorted "{{state.wishSort == 'name' ? sort(computed.wishGames, it.title) : state.wishSort == 'price' ? sort(computed.wishGames, it.price * (100 - it.discount)) : state.wishSort == 'discount' ? sort(computed.wishGames, it.discount, 'desc') : sort(computed.wishGames, indexOf(state.wishlist, it.id))}}"
    :storeWidth "{{min(940, window.width - 32)}}"
    :storeHeight "{{86 + (computed.storeView == 'results' ? 110 + 74 * max(1, length(computed.results)) : computed.storeView == 'app' ? 1670 : computed.storeView == 'cart' ? 140 + max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160) : computed.storeView == 'wishlist' ? 160 + max(1, length(computed.wishSorted)) * 128 : 1960)}}"}
  :actions {:storeHome {:title "Store home"
      :category Store
      :icon store
      :do [{:set state.cartDialog :to false}
        {:set state.storeQuery :to ""}
        {:run go :with {:tab store :page home :id nil}}]}
    :openStore {:title "Open the game's store page"
      :category Store
      :do [{:set state.cartDialog :to false}
        {:set state.storeQuery :to ""}
        {:set state.shotIndex :to 0}
        {:run go :with (node :tab store :page app #"{{id}}")}]}
    :showCart {:title Cart
      :category Store
      :icon shopping-cart
      :do [{:set state.cartDialog :to false}
        {:set state.storeQuery :to ""}
        {:set state.receipt :to []}
        {:run go :with {:tab store :page cart :id nil}}]}
    :showWishlist {:title Wishlist
      :category Store
      :icon heart
      :do [{:set state.cartDialog :to false}
        {:set state.storeQuery :to ""}
        {:run go :with {:tab store :page wishlist :id nil}}]}
    :storeSearch {:title "Search the store"
      :category Store
      :do [{:set state.storeQuery :to "{{query}}"} {:set state.storeCatsOpen :to false}]}
    :addToCart {:title "Add to cart"
      :category Store
      :icon shopping-cart
      :description "Adds the game to the cart and confirms it in a dialog."
      :do [{:if "{{!contains(state.cart, id)}}" :then [{:push state.cart :value "{{id}}"}]}
        {:set state.cartAdded :to "{{id}}"}
        {:set state.cartDialog :to true}]}
    :removeFromCart {:title "Remove from cart"
      :category Store
      :do [{:set state.cart :to "{{filter(state.cart, it != id)}}"}]}
    :toWishlist {:title "Move to the wishlist"
      :category Store
      :do [{:set state.cart :to "{{filter(state.cart, it != id)}}"}
        {:if "{{!contains(state.wishlist, id)}}" :then [{:push state.wishlist :value "{{id}}"}]}]}
    :toggleWishlist {:title "Add to or remove from the wishlist"
      :category Store
      :icon heart
      :do [{:set state.wishlist
          :to "{{contains(state.wishlist, id) ? filter(state.wishlist, it != id) : concat(state.wishlist, [id])}}"}]}
    :toggleFollow {:title "Follow or unfollow"
      :category Store
      :do [{:set state.followed
          :to "{{contains(state.followed, id) ? filter(state.followed, it != id) : concat(state.followed, [id])}}"}]}
    :wishMove {:title "Move a wishlist game up or down"
      :category Store
      :description "Swaps the games at ranks i and j."
      :do [{:set state.wishlist
          :to "{{map(state.wishlist, index == i ? state.wishlist[j] : index == j ? state.wishlist[i] : it)}}"}]}
    :setWishSort {:title "Sort the wishlist" :category Store :do [{:set state.wishSort :to "{{by}}"}]}
    :purchase {:title "Purchase for myself"
      :category Store
      :icon credit-card
      :enablement "{{length(state.cart) > 0 && computed.cartTotal <= state.user.wallet}}"
      :description "Pays from the wallet: the games join the library, not installed, and leave the wishlist."
      :do [(node :update games #"{{state.cart}}" :fields {:owned true})
        {:set state.user.wallet :to "{{round((state.user.wallet - computed.cartTotal) * 100) / 100}}"}
        {:set state.wishlist :to "{{filter(state.wishlist, !contains(state.cart, it))}}"}
        {:set state.receipt :to "{{state.cart}}"}
        {:set state.cart :to []}]}}
  :components {:priceTag {:note "A game's price: the discount and the old price struck through, or 'In library' when owned."
      :params {:g {} :big false}
      :node {:type row
        :height "{{big ? 40 : 32}}"
        :width "{{g.owned ? 86 : g.discount > 0 ? (big ? 156 : 132) : (big ? 90 : 76)}}"
        :children [{:when "{{g.owned}}"
            :type label
            :font small
            :text "{{upper(t('store.inLibrary'))}}"
            :grow 1
            :height "{{big ? 40 : 32}}"
            :align center
            :radius 2
            :color @store.chip
            :textColor @store.chipText}
          {:when "{{!g.owned && g.discount > 0}}"
            :type label
            :font "{{big ? 'price' : 'button'}}"
            :text "-{{g.discount}}%"
            :width "{{big ? 66 : 56}}"
            :height "{{big ? 40 : 32}}"
            :align center
            :color @color.badge
            :textColor @color.onBadge
            :radius {:topLeft 2 :bottomLeft 2}}
          {:when "{{!g.owned}}"
            :grow 1
            :height "{{big ? 40 : 32}}"
            :color @store.priceBox
            :radius "{{g.discount > 0 ? {topRight: 2, bottomRight: 2} : 2}}"
            :children [{:when "{{g.discount > 0}}"
                :type label
                :font small
                :text "{{formatCurrency(g.price, 'EUR')}}"
                :textColor @store.oldPrice
                :fitText true
                :anchors {:right 6 :top "{{big ? 3 : 1}}"}
                :children [{:type panel :height 1 :anchors {:left 0 :right 0 :top 8} :color @store.oldPrice}]}
              {:type label
                :font "{{big ? 'strong' : 'default'}}"
                :fitText true
                :text "{{formatCurrency(round(g.price * (100 - g.discount)) / 100, 'EUR')}}"
                :textColor "{{g.discount > 0 ? '@store.final' : '@store.price'}}"
                :anchors "{{g.discount > 0 ? {right: 6, bottom: big ? 4 : 1} : {right: 8, top: big ? 11 : 7}}}"}]}]}}
    :ctaButton {:note "The green call to action of the store (Add to Cart, Purchase)."
      :params {:title "" :enabled true}
      :node {:type label
        :text "{{title}}"
        :fitText true
        :padding 16
        :radius 2
        :font default
        :focusable true
        :role button
        :cursor hand
        :textColor "{{enabled ? '@store.onCta' : '@color.dim'}}"
        :color "{{enabled ? 'transparent' : '@color.raised'}}"
        :gradient "{{ {kind: 'linear', angle: 90, stops: enabled ? ['@store.cta', '@store.ctaEnd'] : ['@color.raised', '@color.raised']} }}"
        :hover {:textColor "#ffffff" :shadow {:blur 10 :color "#7ff0cf55"}}}}
    :quietButton {:note "The store's quiet button (wishlist, follow, player hub)."
      :params {:title "" :icon "" :on false}
      :node {:type label
        :text "{{title}}"
        :icon "{{icon}}"
        :fitText true
        :padding 12
        :radius 2
        :focusable true
        :role button
        :cursor hand
        :selected "{{on}}"
        :color @store.chip
        :textColor @store.chipText
        :hover {:textColor "#ffffff" :gradient {:kind linear :angle 135 :stops ["#ffc46b" "#c9701a"]}}}}
    :storeHeader {:note "A store section's heading, in capitals, with an optional button at the end."
      :params {:title ""}
      :node {:type row
        :height 46
        :anchors {:left 0 :right 0}
        :alignItems end
        :children [{:type label :font caps :text "{{upper(title)}}" :textColor @color.bright :fitText true :padding 8}
          {:type panel :color transparent :grow 1 :height 1}
          {:slot true}]}}
    :storeTop {:note "Wishlist and cart links, then the store bar: For you, Fresh, Categories, Rewards, News, Experiments, and the search field."
      :params {}
      :node {:type column
        :height 86
        :anchors {:left 0 :right 0}
        :children [{:type row
            :height 40
            :anchors {:left 0 :right 0}
            :alignItems center
            :gap 3
            :children [{:type panel :color transparent :grow 1 :height 1}
              (node #store-wishlist
                :type label
                :font small
                :fitText true
                :padding 14
                :text "{{upper(t('store.wishlist'))}} ({{length(state.wishlist)}})"
                :color @store.chip
                :textColor @store.text
                :radius 2
                :focusable true
                :role button
                :cursor hand
                :hover {:textColor "#ffffff" :color "#ffc46b66"}
                :onClick showWishlist)
              (node :when "{{length(state.cart) > 0}}"
                #store-cart
                :type label
                :font small
                :fitText true
                :padding 14
                :text "{{upper(t('store.cart'))}} ({{length(state.cart)}})"
                :radius 2
                :color transparent
                :gradient {:kind linear :angle 90 :stops [@store.cta @store.ctaEnd]}
                :textColor @store.onCta
                :focusable true
                :role button
                :cursor hand
                :hover {:textColor "#ffffff"}
                :onClick showCart)]}
          (node #store-bar
            :type row
            :height 40
            :anchors {:left 0 :right 0}
            :alignItems center
            :gap 4
            :radius 20
            :color @color.side
            :arrows horizontal
            {:type panel :width 1 :height 1 :color transparent}
            (node :repeat {:for item :in "{{['yours', 'noteworthy', 'categories', 'points', 'news', 'labs']}}"}
              :let {:later "{{contains(['points', 'labs'], item)}}"}
              #"{{'store-bar-' + item}}"
              :type label
              :fitText true
              :padding 13
              :height 30
              :radius 15
              :text "{{t('store.bar.' + item)}}"
              :textColor "{{later ? '@color.dim' : '@color.text'}}"
              :disabled "{{later}}"
              :tooltip "{{later ? t('store.later') : ''}}"
              :focusable true
              :role button
              :cursor hand
              :hover {:color @color.hover :textColor @color.bright}
              :isOpen "{{item == 'categories' && state.storeCatsOpen}}"
              :onClick [{:if "{{item == 'categories'}}"
                  :then [{:toggle state.storeCatsOpen}]
                  :else [{:if "{{item == 'news'}}"
                      :then [{:run libraryHome} {:after 50 :do [{:scrollTo "#news-header"}]}]
                      :else [{:run storeHome}
                        {:if "{{item == 'noteworthy'}}"
                          :then [{:set state.storeList :to new} {:after 50 :do [{:scrollTo "#store-tabs"}]}]}]}]}]
              {:when "{{item == 'categories'}}"
                :use menu
                :with {:open state.storeCatsOpen
                  :width 200
                  :items "{{map(['Action', 'Adventure', 'Horror', 'Platformer', 'Puzzle', 'Racing', 'RPG', 'Shooter', 'Simulation', 'Strategy'], { title: t('genre.' + it), run: 'storeSearch', with: { query: it } })}}"}})
            {:type panel :color transparent :grow 1 :height 1}
            (node #store-search
              :type textarea
              :width 200
              :height 26
              :padding 30
              :bind state.storeQuery
              :placeholder "{{t('store.search')}}"
              :placeholderColor @color.dim
              :textColor @color.text
              :color @color.field
              :radius 13
              :label "{{t('store.search')}}"
              :borderWidth 1
              :borderColor @color.border
              :focused {:borderColor @color.accent}
              {:type label
                :icon search
                :text ""
                :width 24
                :height 22
                :align center
                :radius 11
                :textColor @color.muted
                :anchors {:right 2 :top 2}
                :color transparent})
            {:type panel :color transparent :width 6 :height 1}
            {:type panel :width 1 :height 1 :color transparent})
          {:type panel :color transparent :height 10 :width 1}]}}
    :featured {:note "The featured carousel: a big capsule whose picture follows the screenshot under the pointer, the arrows and the dots."
      :params {}
      :node (node #featured
        :height 383
        :anchors {:left -46 :right -46}
        :children [{:type row
            :height 353
            :anchors {:left 46 :right 46}
            :shadow {:y 0 :blur 10 :color "#00000080"}
            :children [(node #featured-capsule
                :grow 1
                :height 353
                :focusable true
                :role button
                :cursor hand
                :label "{{computed.feature.title}}"
                :onClick {:run openStore :with (node #"{{computed.feature.id}}")}
                :children [{:type image
                    :fill true
                    :fit cover
                    :file "{{'../assets/art/' + computed.feature.id + '/' + (state.storeShot == null ? 'hero' : state.storeShot < 4 ? 'shot' + state.storeShot : 'hero') + '.svg'}}"}]
                :color transparent)
              (node #featured-side
                :width 324
                :height 353
                :color transparent
                :gradient {:kind linear :stops ["#1a1220" "#2a1d33"]}
                {:type column
                  :anchors {:left 10 :right 10 :top 8 :bottom 10}
                  :gap 6
                  :children [{:type label
                      :font title
                      :text "{{computed.feature.title}}"
                      :textColor "#ffffff"
                      :height 38
                      :anchors {:left 0 :right 0}}
                    {:type row
                      :wrap true
                      :gap 4
                      :height 176
                      :anchors {:left 0 :right 0}
                      :children [{:repeat {:for n :in "{{[1, 2, 3, 4]}}"}
                          :width 150
                          :height 86
                          :cursor hand
                          :color "#000000"
                          :onHover {:set state.storeShot :to "{{n}}"}
                          :onLeave {:set state.storeShot :to nil}
                          :children [{:type image
                              :fill true
                              :fit cover
                              :file "{{'../assets/art/' + computed.feature.id + '/' + (n < 4 ? 'shot' + n : 'hero') + '.svg'}}"}
                            {:fill true :color "{{state.storeShot == n ? 'transparent' : '#00000040'}}"}]}]}
                    {:type label
                      :font heading
                      :text "{{t('store.nowAvailable')}}"
                      :textColor "#ffffff"
                      :height 26
                      :anchors {:left 0 :right 0}}
                    {:type row
                      :height 24
                      :anchors {:left 0 :right 0}
                      :gap 4
                      :alignItems center
                      :children [{:type label
                          :text "{{t('store.topSeller')}}"
                          :fitText true
                          :font small
                          :padding 7
                          :radius 2
                          :color "#ffffff26"
                          :textColor "#ffffff"}
                        {:repeat {:for tag :in "{{slice(computed.feature.tags, 0, 3)}}"}
                          :type label
                          :font small
                          :text "{{tag}}"
                          :fitText true
                          :padding 7
                          :radius 2
                          :color "#ffffff14"
                          :textColor "#e2d8e6"}]}
                    {:type panel :color transparent :height 1 :grow 1}
                    {:type row
                      :height 32
                      :anchors {:left 0 :right 0}
                      :alignItems center
                      :children [{:type label :text "" :fitText true :icon monitor :textColor "#e2d8e6"}
                        {:type panel :color transparent :grow 1 :height 1}
                        {:use priceTag :with {:g "{{computed.feature}}"}}]}]})]}
          (node #featured-prev
            :type label
            :icon chevron-left
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
            :label "{{t('store.previous')}}"
            :hover {:color "#ffffff26"}
            :onClick {:set state.storeFeature :to "{{(state.storeFeature + 4) % 5}}"})
          (node #featured-next
            :type label
            :icon chevron-right
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
            :label "{{t('store.next')}}"
            :hover {:color "#ffffff26"}
            :onClick {:set state.storeFeature :to "{{(state.storeFeature + 1) % 5}}"})
          {:type row
            :height 12
            :anchors {:left 0 :right 0 :bottom 6}
            :gap 3
            :justify center
            :children [{:repeat {:for d :in "{{range(5)}}"}
                :width 15
                :height 9
                :radius 2
                :cursor hand
                :color "{{d == state.storeFeature ? '@store.dotOn' : '@store.dot'}}"
                :role button
                :label "{{t('store.slide', { n: d + 1 })}}"
                :onClick {:set state.storeFeature :to "{{d}}"}}]}]
        :color transparent)}
    :offerCard {:note "A special offer: the game's banner, when the offer ends and its price."
      :params {:g {}}
      :node {:width "{{(computed.storeWidth - 22) / 3}}"
        :height 196
        :color @store.card
        :radius 2
        :focusable true
        :role button
        :cursor hand
        :label "{{g.title}}"
        :shadow {:y 2 :blur 6 :color "#00000050"}
        :hover {:color @store.cardHover :shadow {:y 4 :blur 14 :color "#f5a52450"}}
        :onClick {:run openStore :with (node #"{{g.id}}")}
        :children [{:type image
            :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
            :fit cover
            :height 143
            :anchors {:left 0 :right 0 :top 0}
            :radius {:topLeft 2 :topRight 2}}
          {:type label
            :text "{{t(g.discount >= 50 ? 'store.weeklong' : 'store.midweek')}}"
            :fitText true
            :font caps
            :textColor @store.text
            :anchors {:left 10 :bottom 28}}
          {:type label
            :text "{{t('store.endsOn', { date: '2 Nov' })}}"
            :fitText true
            :font small
            :textColor @color.muted
            :anchors {:left 10 :bottom 10}}
          {:use priceTag :with {:g "{{g}}"} :anchors {:right 8 :bottom 10}}]}}
    :categoryTile {:note "A category to browse: a cover under a coloured wash and the category's name."
      :params {:genre "" :game "" :tint ""}
      :node {:width "{{(computed.storeWidth - 36) / 4}}"
        :height 170
        :radius 4
        :color "#000000"
        :focusable true
        :role button
        :cursor hand
        :label "{{t('genre.' + genre)}}"
        :hover {:scale 1.03 :shadow {:blur 16 :color "#f5a52450"}}
        :onClick {:run storeSearch :with {:query "{{genre}}"}}
        :children [{:type image :file "{{'../assets/art/' + game + '/hero.svg'}}" :fill true :fit cover :radius 4}
          {:fill true
            :radius 4
            :color transparent
            :gradient {:kind linear :stops [{:at 0.2 :color "#00000000"} {:at 1 :color "{{tint}}"}]}}
          {:type label
            :font heading
            :text "{{upper(t('genre.' + genre))}}"
            :textColor "#ffffff"
            :height 30
            :align center
            :anchors {:left 0 :right 0 :bottom 14}}]}}
    :storeRow {:note "A game in a store list: its banner, title, tags and price; it lights up when it is the one previewed."
      :params {:g {} :lit false}
      :node {:type row
        :height 69
        :anchors {:left 0 :right 0}
        :alignItems center
        :gap 12
        :radius 2
        :color "{{lit ? '@store.preview' : '@store.card'}}"
        :focusable true
        :role button
        :cursor hand
        :label "{{g.title}}"
        :onClick {:run openStore :with (node #"{{g.id}}")}
        :children [{:type image
            :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
            :fit cover
            :width 184
            :height 69
            :radius {:topLeft 2 :bottomLeft 2}}
          {:type column
            :grow 1
            :height 46
            :children [{:type label
                :text "{{g.title}}"
                :height 22
                :anchors {:left 0 :right 0}
                :textColor "{{lit ? '@store.ink' : '@store.text'}}"}
              {:type label
                :font small
                :height 18
                :anchors {:left 0 :right 0}
                :icon monitor
                :text "  {{join(slice(g.tags, 0, 3), ', ')}}"
                :textColor "{{lit ? '@store.inkSoft' : '@color.muted'}}"}]}
          {:use priceTag :with {:g "{{g}}"}}
          {:type panel :color transparent :width 4 :height 1}]}}
    :storeHome {:note "The store's front page: featured, special offers, categories and the tabbed lists with a preview."
      :params {}
      :node (node #store-home :type column :height 1960 :anchors {:left 0 :right 0}
        {:use storeHeader :with {:title "{{t('store.featured')}}"}}
        {:use featured}
        {:use storeHeader
          :with {:title "{{t('store.offers')}}"}
          :children [{:use quietButton
              :with {:title "{{t('store.browseMore')}}"}
              :onClick {:run storeSearch :with {:query "{{''}}"}}}]}
        (node #offers :type row :wrap true :gap 11 :height 410 :anchors {:left 0 :right 0}
          (node :repeat {:for g :in "{{computed.offers}}" :key "{{g.id}}"}
            :use offerCard
            #"{{'offer-' + g.id}}"
            :with {:g "{{g}}"}))
        {:type panel :color transparent :height 14 :width 1}
        {:use storeHeader :with {:title "{{t('store.categories')}}"}}
        (node #categories :type row :gap 12 :height 170 :anchors {:left 0 :right 0}
          {:use categoryTile :with {:genre Racing :game aurora :tint "#d8357ae0"}}
          {:use categoryTile :with {:genre Strategy :game clockwork :tint "#2f6fd8e0"}}
          {:use categoryTile :with {:genre Simulation :game saltmarsh :tint "#2aa37ae0"}}
          {:use categoryTile :with {:genre Shooter :game glasswing :tint "#d8762fe0"}})
        {:type panel :color transparent :height 30 :width 1}
        (node #store-tabs :type row :height 34 :anchors {:left 0 :right 0} :arrows horizontal :role tablist
          (node :repeat {:for tab :in "{{['new', 'top', 'specials']}}"}
            #"{{'store-tab-' + tab}}"
            :type label
            :fitText true
            :padding 14
            :height 34
            :text "{{t('store.list.' + tab)}}"
            :radius {:topLeft 3 :topRight 3}
            :role tab
            :focusable true
            :cursor hand
            :selected "{{state.storeList == tab}}"
            :color "{{state.storeList == tab ? '@store.card' : 'transparent'}}"
            :textColor "{{state.storeList == tab ? '@color.bright' : '@store.chipText'}}"
            :hover {:textColor @color.bright}
            :onClick [{:set state.storeList :to "{{tab}}"} {:set state.storeHover :to nil}]))
        {:type row
          :height 600
          :anchors {:left 0 :right 0}
          :gap 0
          :alignItems start
          :children [(node #store-list :type column :grow 1 :height 600 :gap 5
              (node :repeat {:for g :in "{{computed.storeRows}}" :key "{{g.id}}"}
                :use storeRow
                #"{{'store-row-' + g.id}}"
                :with {:g "{{g}}" :lit "{{computed.storePreview.id == g.id}}"}
                :onHover {:set state.storeHover :to "{{g.id}}"}))
            (node #store-preview
              :width 308
              :height 592
              :color transparent
              :gradient {:kind linear :stops [@store.preview @store.previewEnd]}
              {:type column
                :anchors {:left 14 :right 14 :top 12 :bottom 12}
                :gap 6
                :children [{:type label
                    :font heading
                    :text "{{computed.storePreview.title}}"
                    :textColor @store.ink
                    :height 26
                    :anchors {:left 0 :right 0}}
                  {:type row
                    :height 20
                    :anchors {:left 0 :right 0}
                    :gap 6
                    :children [{:type label
                        :text "{{t('store.overall')}}"
                        :fitText true
                        :font small
                        :textColor @store.inkSoft}
                      {:type label
                        :text "{{t('review.' + computed.storePreview.review)}}"
                        :fitText true
                        :font small
                        :textColor "#5a2f70"}
                      {:type label
                        :text "({{formatNumber(computed.storePreview.reviewCount)}})"
                        :fitText true
                        :font small
                        :textColor @store.inkSoft}]}
                  {:type row
                    :height 24
                    :anchors {:left 0 :right 0}
                    :gap 4
                    :children [{:repeat {:for tag :in "{{slice(computed.storePreview.tags, 0, 4)}}"}
                        :type label
                        :font small
                        :text "{{tag}}"
                        :fitText true
                        :padding 6
                        :radius 2
                        :color "#ffffff55"
                        :textColor @store.inkSoft}]}
                  {:repeat {:for n :in "{{[1, 2, 3]}}"}
                    :type image
                    :height 158
                    :anchors {:left 0 :right 0}
                    :fit cover
                    :file "{{'../assets/art/' + computed.storePreview.id + '/shot' + n + '.svg'}}"}]})]})}
    :storeResults {:params {}
      :node (node #store-results
        :type column
        :height "{{110 + 74 * max(1, length(computed.results))}}"
        :anchors {:left 0 :right 0}
        :gap 5
        {:type row
          :height 50
          :anchors {:left 0 :right 0}
          :alignItems center
          :gap 10
          :children [{:type label
              :text "{{t('store.results', { n: length(computed.results), query: state.storeQuery })}}"
              :fitText true
              :textColor @store.text}
            {:type panel :color transparent :grow 1 :height 1}
            {:use quietButton
              :with {:title "{{t('store.clear')}}" :icon circle-x}
              :onClick {:set state.storeQuery :to ""}}]}
        {:when "{{length(computed.results) == 0}}"
          :type label
          :text "{{t('store.noResults')}}"
          :textColor @color.muted
          :height 40
          :anchors {:left 0 :right 0}}
        (node :repeat {:for g :in "{{computed.results}}" :key "{{g.id}}"}
          :use storeRow
          #"{{'result-' + g.id}}"
          :with {:g "{{g}}"}))}
    :requirements {:params {:g {} :level ""}
      :node {:type column
        :grow 1
        :height 150
        :gap 4
        :children [{:type label
            :font caps
            :text "{{upper(t('req.' + level))}}"
            :textColor @store.text
            :height 22
            :anchors {:left 0 :right 0}}
          {:repeat {:for r :in "{{['os', 'cpu', 'memory', 'gpu', 'storage']}}"}
            :type label
            :font small
            :height 20
            :anchors {:left 0 :right 0}
            :runs [{:text "{{t('req.' + r) + ': '}}" :bold true :color @store.text}
              {:text "{{r == 'storage' ? t('req.storageValue', { n: ceil(g.sizeGb * (level == 'recommended' ? 1.5 : 1)) }) : t('req.' + state.reqTab + '.' + r + '.' + level)}}"
                :color @color.muted}]}]}}
    :storeApp {:note "A game's store page: the gallery and its strip, the header capsule with reviews and tags, buying, the description, requirements and reviews."
      :params {:g {}}
      :node (node #store-app :type column :height 1670 :anchors {:left 0 :right 0}
        {:type row
          :height 22
          :anchors {:left 0 :right 0}
          :gap 6
          :alignItems center
          :children [{:type label
              :text "{{t('store.allGames')}}"
              :fitText true
              :font small
              :textColor @color.muted
              :cursor hand
              :role link
              :focusable true
              :hover {:textColor @color.bright}
              :onClick storeHome}
            {:type label :text "" :fitText true :icon chevron-right :mirror true :font small :textColor @color.dim}
            {:type label
              :text "{{t('genre.' + g.genre)}}"
              :fitText true
              :font small
              :textColor @color.muted
              :cursor hand
              :role link
              :focusable true
              :hover {:textColor @color.bright}
              :onClick {:run storeSearch :with {:query "{{g.genre}}"}}}
            {:type label :text "" :fitText true :icon chevron-right :mirror true :font small :textColor @color.dim}
            {:type label :text "{{g.title}}" :fitText true :font small :textColor @color.muted}]}
        {:type row
          :height 48
          :anchors {:left 0 :right 0}
          :alignItems center
          :children [(node #store-title
              :type label
              :font title
              :text "{{g.title}}"
              :textColor @color.bright
              :grow 1
              :height 40)
            {:use quietButton :with {:title "{{t('game.section.hub')}}"}}]}
        (node #store-top :type row :height 424 :anchors {:left 0 :right 0} :gap 16 :color @store.card
          (node #gallery :type column :grow 1 :height 424 :gap 8
            {:type image
              :height "{{min(338, round((computed.storeWidth - 340) * 0.5625))}}"
              :anchors {:left 0 :right 0}
              :fit cover
              :color "#000000"
              :file "{{'../assets/art/' + g.id + '/' + ['hero', 'shot1', 'shot2', 'shot3'][state.shotIndex] + '.svg'}}"}
            (node #gallery-strip :type row :height 72 :anchors {:left 0 :right 0} :gap 4 :arrows horizontal
              (node :repeat {:for s :in "{{range(4)}}"}
                #"{{'shot-' + s}}"
                :width 116
                :height 65
                :focusable true
                :role button
                :cursor hand
                :label "{{t('store.screenshot', { n: s + 1 })}}"
                :selected "{{state.shotIndex == s}}"
                :onClick {:set state.shotIndex :to "{{s}}"}
                :children [{:type image
                    :fill true
                    :fit cover
                    :file "{{'../assets/art/' + g.id + '/' + ['hero', 'shot1', 'shot2', 'shot3'][s] + '.svg'}}"}
                  {:fill true
                    :color "{{state.shotIndex == s ? 'transparent' : '#00000055'}}"
                    :borderWidth "{{state.shotIndex == s ? 2 : 0}}"
                    :borderColor "#ffffff"}]
                :color transparent)))
          (node #store-side :type column :width 314 :height 424 :gap 4
            {:type image
              :height 151
              :anchors {:left 0 :right 0}
              :fit cover
              :file "{{'../assets/art/' + g.id + '/hero.svg'}}"}
            {:type panel :color transparent :height 4 :width 1}
            {:type label
              :font small
              :text "{{g.blurb}}"
              :wrap true
              :height 40
              :anchors {:left 0 :right 0}
              :textColor @store.text}
            {:type row
              :height 20
              :anchors {:left 0 :right 0}
              :children [{:type label
                  :font small
                  :text "{{upper(t('store.recentReviews'))}}"
                  :width 110
                  :textColor @store.label
                  :height 20
                  :align {:horizontal start :vertical center}}
                {:type label
                  :font small
                  :text "{{t('review.' + g.review)}}"
                  :grow 1
                  :textColor @store.chipText
                  :height 20
                  :align {:horizontal start :vertical center}}]}
            {:type row
              :height 20
              :anchors {:left 0 :right 0}
              :children [{:type label
                  :font small
                  :text "{{upper(t('store.allReviews'))}}"
                  :width 110
                  :textColor @store.label
                  :height 20
                  :align {:horizontal start :vertical center}}
                {:type label
                  :font small
                  :text "{{t('review.' + g.review) + ' (' + formatNumber(g.reviewCount) + ')'}}"
                  :grow 1
                  :textColor @store.chipText
                  :height 20
                  :align {:horizontal start :vertical center}}]}
            {:type row
              :height 20
              :anchors {:left 0 :right 0}
              :children [{:type label
                  :font small
                  :text "{{upper(t('game.release'))}}"
                  :width 110
                  :textColor @store.label
                  :height 20
                  :align {:horizontal start :vertical center}}
                {:type label
                  :font small
                  :text "{{g.release}}"
                  :grow 1
                  :textColor @store.text
                  :height 20
                  :align {:horizontal start :vertical center}}]}
            {:type panel :color transparent :height 4 :width 1}
            {:type row
              :height 20
              :anchors {:left 0 :right 0}
              :children [{:type label
                  :font small
                  :text "{{upper(t('game.developer'))}}"
                  :width 110
                  :textColor @store.label
                  :height 20
                  :align {:horizontal start :vertical center}}
                {:type label
                  :font small
                  :text "{{g.developer}}"
                  :grow 1
                  :textColor @store.chipText
                  :height 20
                  :align {:horizontal start :vertical center}}]}
            {:type row
              :height 20
              :anchors {:left 0 :right 0}
              :children [{:type label
                  :font small
                  :text "{{upper(t('game.publisher'))}}"
                  :width 110
                  :textColor @store.label
                  :height 20
                  :align {:horizontal start :vertical center}}
                {:type label
                  :font small
                  :text "{{g.publisher}}"
                  :grow 1
                  :textColor @store.chipText
                  :height 20
                  :align {:horizontal start :vertical center}}]}
            {:type panel :color transparent :height 8 :width 1}
            {:type label
              :font small
              :text "{{t('store.userTags')}}"
              :height 18
              :anchors {:left 0 :right 0}
              :textColor @store.label}
            {:type row
              :wrap true
              :gap 3
              :height 26
              :anchors {:left 0 :right 0}
              :children [{:repeat {:for tag :in "{{g.tags}}"}
                  :type label
                  :font small
                  :text "{{tag}}"
                  :fitText true
                  :padding 7
                  :radius 2
                  :color @store.chip
                  :textColor @store.chipText
                  :cursor hand
                  :hover {:textColor "#ffffff"}
                  :onClick {:run storeSearch :with {:query "{{tag}}"}}}
                {:type label
                  :text "+"
                  :fitText true
                  :font small
                  :padding 7
                  :radius 2
                  :color @store.chip
                  :textColor @store.chipText}]})
          {:type panel :color transparent :width 10 :height 1})
        {:type panel :color transparent :height 16 :width 1}
        (node #store-queue :type row :height 44 :anchors {:left 0 :right 0} :gap 6 :alignItems center :color @store.card
          {:type panel :color transparent :width 4 :height 1}
          (node #store-wish
            :use quietButton
            :with {:title "{{contains(state.wishlist, g.id) ? t('store.onWishlist') : t('store.addWishlist')}}"
              :icon "{{contains(state.wishlist, g.id) ? 'check' : ''}}"
              :on "{{contains(state.wishlist, g.id)}}"}
            :onClick {:run toggleWishlist :with (node #"{{g.id}}")})
          (node #store-follow
            :use quietButton
            :with {:title "{{contains(state.followed, g.id) ? t('store.following') : t('store.follow')}}"
              :icon "{{contains(state.followed, g.id) ? 'check' : ''}}"
              :on "{{contains(state.followed, g.id)}}"}
            :onClick {:run toggleFollow :with (node #"{{g.id}}")})
          {:use quietButton :with {:title "{{t('store.ignore')}}"}}
          {:type panel :color transparent :grow 1 :height 1}
          {:use quietButton :with {:title "{{t('store.queue')}}" :icon list}}
          {:type panel :color transparent :width 4 :height 1})
        {:type panel :color transparent :height 20 :width 1}
        {:type row
          :height 480
          :anchors {:left 0 :right 0}
          :gap 16
          :alignItems start
          :children [{:type column
              :grow 1
              :height 480
              :children [(node #purchase
                  :height 104
                  :anchors {:left 0 :right 0}
                  :radius 4
                  :color transparent
                  :gradient {:kind linear :angle 120 :stops ["#5e4b6c80" "#4a385a66"]}
                  {:when "{{!g.owned}}"
                    :type label
                    :font heading
                    :text "{{t('store.buy', { title: g.title })}}"
                    :textColor "#ffffff"
                    :fitText true
                    :anchors {:left 16 :top 14}}
                  {:when "{{g.owned}}"
                    :type label
                    :font heading
                    :text "{{t('store.owned', { title: g.title })}}"
                    :textColor "#ffffff"
                    :fitText true
                    :anchors {:left 16 :top 14}}
                  {:when "{{!g.owned && g.discount > 0}}"
                    :type label
                    :font small
                    :text "{{upper(t('store.promo', { date: '2 Nov' }))}}"
                    :textColor "#ffd9a0"
                    :fitText true
                    :anchors {:left 16 :top 42}}
                  {:type label :text "" :fitText true :icon monitor :textColor "#e2d8e6" :anchors {:right 16 :top 16}}
                  {:type row
                    :height 44
                    :anchors {:right 14 :bottom 12}
                    :width "{{g.owned ? 118 : g.discount > 0 ? 262 : 200}}"
                    :color "#000000"
                    :radius 3
                    :alignItems center
                    :gap 6
                    :children [{:type panel :color transparent :grow 1 :height 1}
                      {:when "{{!g.owned}}" :use priceTag :with {:g "{{g}}" :big false}}
                      (node :when "{{!g.owned}}"
                        #add-to-cart
                        :use ctaButton
                        :with {:title "{{contains(state.cart, g.id) ? t('store.inCart') : t('store.addToCart')}}"}
                        :onClick [{:if "{{contains(state.cart, g.id)}}"
                            :then [{:run showCart}]
                            :else [{:run addToCart :with (node #"{{g.id}}")}]}])
                      {:when "{{g.owned}}"
                        :use ctaButton
                        :with {:title "{{t('store.playNow')}}"}
                        :onClick {:run openGame :with (node #"{{g.id}}")}}
                      {:type panel :color transparent :width 2 :height 1}]})
                {:type panel :color transparent :height 6 :width 1}
                {:type column
                  :height 34
                  :anchors {:left 0 :right 0}
                  :children [{:type panel :color transparent :height 10 :width 1}
                    {:type label
                      :font caps
                      :text "{{upper(t('store.about'))}}"
                      :textColor @color.bright
                      :height 20
                      :anchors {:left 0 :right 0}}
                    {:type panel
                      :height 1
                      :anchors {:left 0 :right 0}
                      :color transparent
                      :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}}]}
                {:type label
                  :text "{{g.blurb + ' ' + t('store.aboutText', { title: g.title, genre: lower(t('genre.' + g.genre)) })}}"
                  :wrap true
                  :height 76
                  :anchors {:left 0 :right 0}
                  :textColor @store.text
                  :padding 0}
                {:type column
                  :height 34
                  :anchors {:left 0 :right 0}
                  :children [{:type panel :color transparent :height 10 :width 1}
                    {:type label
                      :font caps
                      :text "{{upper(t('store.requirements'))}}"
                      :textColor @color.bright
                      :height 20
                      :anchors {:left 0 :right 0}}
                    {:type panel
                      :height 1
                      :anchors {:left 0 :right 0}
                      :color transparent
                      :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}}]}
                (node #req-tabs
                  :type row
                  :height 40
                  :anchors {:left 0 :right 0}
                  :gap 2
                  :alignItems center
                  :arrows horizontal
                  :role tablist
                  (node :repeat {:for p :in "{{['windows', 'macos', 'linux']}}"}
                    #"{{'req-' + p}}"
                    :type label
                    :font small
                    :text "{{upper(t('req.platform.' + p))}}"
                    :fitText true
                    :padding 12
                    :radius 2
                    :role tab
                    :focusable true
                    :cursor hand
                    :selected "{{state.reqTab == p}}"
                    :color "{{state.reqTab == p ? '@store.chip' : 'transparent'}}"
                    :textColor "{{state.reqTab == p ? '@color.bright' : '@store.chipText'}}"
                    :onClick {:set state.reqTab :to "{{p}}"}))
                {:type row
                  :height 150
                  :anchors {:left 0 :right 0}
                  :gap 20
                  :children [{:use requirements :with {:g "{{g}}" :level minimum}}
                    {:use requirements :with {:g "{{g}}" :level recommended}}]}]}
            {:type column
              :width 308
              :height 480
              :gap 12
              :children [(node #features :type column :height 196 :anchors {:left 0 :right 0} :color @store.card :gap 2
                  {:type panel :color transparent :height 6 :width 1}
                  {:repeat {:for f
                      :in "{{filter([{ id: 'single', icon: 'user', on: true }, { id: 'coop', icon: 'users', on: contains(g.tags, 'Co-op') }, { id: 'achievements', icon: 'trophy', on: true }, { id: 'controller', icon: 'gamepad-2', on: g.controller != 'none' }, { id: 'cloud', icon: 'cloud', on: true }, { id: 'family', icon: 'share-2', on: true }], it.on)}}"}
                    :type row
                    :height 30
                    :anchors {:left 8 :right 8}
                    :gap 8
                    :alignItems center
                    :color "#ffffff0a"
                    :children [{:type label
                        :icon "{{f.icon}}"
                        :text ""
                        :width 30
                        :height 30
                        :align center
                        :textColor @store.chipText
                        :color "#00000033"}
                      {:type label
                        :font small
                        :text "{{f.id == 'controller' ? t('controller.' + g.controller) : t('store.feature.' + f.id)}}"
                        :grow 1
                        :textColor @store.chipText
                        :height 30
                        :align {:horizontal start :vertical center}}]})
                (node #languages :type column :height 150 :anchors {:left 0 :right 0} :color @store.card
                  {:type panel :color transparent :height 8 :width 1}
                  {:type row
                    :height 22
                    :anchors {:left 10 :right 10}
                    :children [{:type label
                        :font caps
                        :text "{{t('store.languages')}}"
                        :grow 1
                        :textColor @store.text
                        :height 22
                        :align {:horizontal start :vertical center}}
                      {:repeat {:for c :in "{{['interface', 'audio', 'subtitles']}}"}
                        :type label
                        :font small
                        :text "{{t('store.lang.' + c)}}"
                        :width 62
                        :align center
                        :textColor @color.muted
                        :height 22}]}
                  {:repeat {:for l
                      :in "{{[['en', true, true, true], ['es', true, false, true], ['fr', true, false, true]]}}"}
                    :type row
                    :height 26
                    :anchors {:left 10 :right 10}
                    :children [{:type label
                        :font small
                        :text "{{t('lang.' + l[0])}}"
                        :grow 1
                        :textColor @store.text
                        :height 26
                        :align {:horizontal start :vertical center}}
                      {:repeat {:for k :in "{{[1, 2, 3]}}"}
                        :type label
                        :icon "{{l[k] ? 'check' : ''}}"
                        :text ""
                        :width 62
                        :align center
                        :textColor @color.ingame
                        :height 26}]})]}]}
        {:type column
          :height 34
          :anchors {:left 0 :right 0}
          :children [{:type panel :color transparent :height 10 :width 1}
            {:type label
              :font caps
              :text "{{upper(t('store.reviews'))}}"
              :textColor @color.bright
              :height 20
              :anchors {:left 0 :right 0}}
            {:type panel
              :height 1
              :anchors {:left 0 :right 0}
              :color transparent
              :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}}]}
        {:type panel :color transparent :height 10 :width 1}
        (node #review-summary
          :type row
          :height 56
          :anchors {:left 0 :right 0}
          :gap 10
          :alignItems center
          :color @store.card
          {:type panel :color transparent :width 4 :height 1}
          {:type label :text "{{t('store.overallReviews')}}" :fitText true :font caps :textColor @store.text}
          {:type label :text "{{t('review.' + g.review)}}" :fitText true :font strong :textColor @store.chipText}
          {:type label
            :text "{{t('store.reviewCount', { n: formatNumber(g.reviewCount) })}}"
            :fitText true
            :font small
            :textColor @color.muted}
          {:type panel :color transparent :grow 1 :height 1}
          {:width 160
            :height 8
            :radius 4
            :color @store.down
            :children [{:height 8
                :radius 4
                :color @store.chipText
                :width "{{text(g.reviewPercent) + '%'}}"
                :anchors {:left 0 :top 0}}]}
          {:type label :text "{{g.reviewPercent}}%" :fitText true :font small :textColor @color.muted}
          {:type panel :color transparent :width 8 :height 1})
        {:type panel :color transparent :height 10 :width 1}
        (node #reviews :type column :height 432 :anchors {:left 0 :right 0} :gap 10
          {:repeat {:for r :in "{{state.reviews}}"}
            :type row
            :height 136
            :anchors {:left 0 :right 0}
            :gap 0
            :color @store.card
            :children [{:type column
                :width 200
                :height 136
                :gap 4
                :children [{:type panel :color transparent :height 10 :width 1}
                  {:type row
                    :height 36
                    :anchors {:left 10 :right 6}
                    :gap 8
                    :alignItems center
                    :children [{:type image
                        :file "{{'../assets/art/avatars/' + r.avatar + '.svg'}}"
                        :width 34
                        :height 34
                        :radius 2
                        :borderWidth 1
                        :borderColor "#6b5280"}
                      {:type label
                        :font strong
                        :text "{{r.author}}"
                        :grow 1
                        :textColor "#f0d9c0"
                        :height 36
                        :align {:horizontal start :vertical center}}]}
                  {:type label
                    :font small
                    :text "{{t('store.products', { n: r.products })}}"
                    :height 18
                    :anchors {:left 10 :right 6}
                    :textColor @color.muted}
                  {:type label
                    :font small
                    :text "{{t('store.reviewsBy', { n: r.reviews })}}"
                    :height 18
                    :anchors {:left 10 :right 6}
                    :textColor @color.muted}]}
              {:type column
                :grow 1
                :height 136
                :gap 6
                :children [{:type panel :color transparent :height 8 :width 1}
                  {:type row
                    :height 40
                    :anchors {:left 0 :right 10}
                    :gap 10
                    :alignItems center
                    :color "#00000033"
                    :children [{:type label
                        :icon "{{r.up ? 'thumbs-up' : 'thumbs-down'}}"
                        :text ""
                        :font heading
                        :width 40
                        :height 40
                        :align center
                        :textColor "{{r.up ? '@store.chipText' : '@store.down'}}"
                        :color "#0000004d"}
                      {:type column
                        :grow 1
                        :height 36
                        :children [{:type label
                            :font heading
                            :text "{{t(r.up ? 'store.recommended' : 'store.notRecommended')}}"
                            :height 20
                            :anchors {:left 0 :right 0}
                            :textColor "#d6d7d8"}
                          {:type label
                            :font small
                            :text "{{t('store.onRecord', { n: formatNumber(r.hours, 1, 1) })}}"
                            :height 16
                            :anchors {:left 0 :right 0}
                            :textColor @color.muted}]}]}
                  {:type label
                    :font small
                    :text "{{upper(t('store.posted', { date: r.posted }))}}"
                    :height 16
                    :anchors {:left 0 :right 0}
                    :textColor @color.dim}
                  {:type label
                    :text "{{r.text}}"
                    :wrap true
                    :height 30
                    :anchors {:left 0 :right 14}
                    :textColor @store.text}
                  {:type row
                    :height 22
                    :anchors {:left 0 :right 0}
                    :gap 4
                    :alignItems center
                    :children [{:type label
                        :text "{{t('store.helpful')}}"
                        :fitText true
                        :font small
                        :textColor @color.dim}
                      (node :repeat {:for b :in "{{['yes', 'no', 'funny']}}"}
                        :let {:on "{{state.votes[slug(r.author)] == b}}"}
                        #"{{'vote-' + slug(r.author) + '-' + b}}"
                        :type label
                        :font small
                        :text "{{t('store.vote.' + b)}}"
                        :fitText true
                        :padding 8
                        :radius 2
                        :color "{{on ? '@color.select' : '@store.chip'}}"
                        :textColor "{{on ? '@color.bright' : '@store.chipText'}}"
                        :focusable true
                        :role button
                        :selected "{{on}}"
                        :cursor hand
                        :hover {:textColor @color.bright}
                        :onClick {:set "{{'state.votes.' + slug(r.author)}}" :to "{{on ? null : b}}"})]}]}]}))}
    :storeCart {:note "The cart: the games and their prices, the estimated total from the wallet, and a receipt after buying."
      :params {}
      :node (node #store-cart-page
        :type column
        :height "{{140 + max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160)}}"
        :anchors {:left 0 :right 0}
        {:type label
          :font title
          :text "{{upper(t('store.yourCart'))}}"
          :textColor @color.bright
          :height 60
          :anchors {:left 0 :right 0}}
        {:type row
          :height "{{max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160)}}"
          :anchors {:left 0 :right 0}
          :gap 16
          :alignItems start
          :children [{:type column
              :grow 1
              :height "{{max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160)}}"
              :gap 6
              :children [(node :when "{{length(state.receipt) > 0}}"
                  #receipt
                  :type column
                  :height "{{length(state.receipt) * 40 + 130}}"
                  :anchors {:left 0 :right 0}
                  :color @store.card
                  :gap 6
                  {:type panel :color transparent :height 8 :width 1}
                  {:type label
                    :font heading
                    :icon circle-check
                    :text " {{t('store.thanks')}}"
                    :height 30
                    :anchors {:left 16 :right 16}
                    :textColor @color.ingame}
                  {:repeat {:for id :in "{{state.receipt}}"}
                    :type row
                    :height 34
                    :anchors {:left 16 :right 16}
                    :gap 10
                    :alignItems center
                    :children [{:type image
                        :file "{{'../assets/art/' + id + '/hero.svg'}}"
                        :fit cover
                        :width 90
                        :height 34}
                      {:type label
                        :text "{{find(data.games, it.id == id).title}}"
                        :grow 1
                        :textColor @store.text
                        :height 34
                        :align {:horizontal start :vertical center}}
                      {:use quietButton
                        :with {:title "{{t('status.button.none')}}" :icon download}
                        :onClick {:run install :with (node #"{{id}}")}}]}
                  {:type row
                    :height 40
                    :anchors {:left 16 :right 16}
                    :alignItems center
                    :children [{:use quietButton :with {:title "{{t('store.toLibrary')}}"} :onClick showLibrary}]})
                {:when "{{length(computed.cartGames) == 0 && length(state.receipt) == 0}}"
                  :type column
                  :height 120
                  :anchors {:left 0 :right 0}
                  :color @store.card
                  :children [{:type panel :color transparent :height 24 :width 1}
                    {:type label
                      :font heading
                      :text "{{t('store.cartEmpty')}}"
                      :align center
                      :height 30
                      :anchors {:left 0 :right 0}
                      :textColor @store.text}
                    {:type row
                      :height 40
                      :anchors {:left 0 :right 0}
                      :justify center
                      :alignItems center
                      :children [{:use quietButton :with {:title "{{t('store.continue')}}"} :onClick storeHome}]}]}
                (node :repeat {:for g :in "{{computed.cartGames}}" :key "{{g.id}}"}
                  #"{{'cart-' + g.id}}"
                  :type row
                  :height 100
                  :anchors {:left 0 :right 0}
                  :gap 14
                  :alignItems center
                  :color @store.card
                  :radius 2
                  {:type panel :color transparent :width 2 :height 1}
                  {:type image
                    :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
                    :fit cover
                    :width 184
                    :height 69
                    :cursor hand
                    :onClick {:run openStore :with (node #"{{g.id}}")}}
                  {:type column
                    :grow 1
                    :height 69
                    :gap 4
                    :children [{:type label
                        :font strong
                        :text "{{g.title}}"
                        :height 22
                        :anchors {:left 0 :right 0}
                        :textColor @color.bright}
                      {:type label :text "" :fitText true :icon monitor :font small :textColor @color.muted}
                      {:type row
                        :height 20
                        :anchors {:left 0 :right 0}
                        :gap 14
                        :children [{:type label
                            :text "{{t('store.remove')}}"
                            :fitText true
                            :font small
                            :textColor @color.muted
                            :cursor hand
                            :role button
                            :focusable true
                            :hover {:textColor @color.bright}
                            :onClick {:run removeFromCart :with (node #"{{g.id}}")}}
                          {:type label
                            :text "{{t('store.moveToWishlist')}}"
                            :fitText true
                            :font small
                            :textColor @color.muted
                            :cursor hand
                            :role button
                            :focusable true
                            :hover {:textColor @color.bright}
                            :onClick {:run toWishlist :with (node #"{{g.id}}")}}]}]}
                  {:use priceTag :with {:g "{{g}}"}}
                  {:type panel :color transparent :width 8 :height 1})]}
            (node #cart-summary :type column :width 308 :height 236 :color @store.card :radius 2 :gap 8
              {:type panel :color transparent :height 8 :width 1}
              {:type row
                :height 30
                :anchors {:left 16 :right 16}
                :alignItems center
                :children [{:type label
                    :font heading
                    :text "{{t('store.total')}}"
                    :grow 1
                    :textColor @color.bright
                    :height 30
                    :align {:horizontal start :vertical center}}
                  {:type label
                    :font heading
                    :text "{{formatCurrency(computed.cartTotal, 'EUR')}}"
                    :fitText true
                    :textColor @color.bright}]}
              {:type label
                :font small
                :text "{{t('store.tax')}}"
                :wrap true
                :height 32
                :anchors {:left 16 :right 16}
                :textColor @color.muted}
              {:type label
                :font small
                :text "{{t('store.walletLeft', { amount: formatCurrency(state.user.wallet, 'EUR') })}}"
                :height 18
                :anchors {:left 16 :right 16}
                :textColor "{{computed.cartTotal > state.user.wallet ? '@store.down' : '@color.muted'}}"}
              {:type row
                :height 44
                :anchors {:left 16 :right 16}
                :alignItems center
                :children [(node #purchase-button
                    :use ctaButton
                    :grow 1
                    :align center
                    :with {:title "{{t('store.purchase')}}"
                      :enabled "{{length(state.cart) > 0 && computed.cartTotal <= state.user.wallet}}"}
                    :onClick purchase)]}
              {:type row
                :height 34
                :anchors {:left 16 :right 16}
                :alignItems center
                :children [{:use quietButton
                    :with {:title "{{t('store.continue')}}"}
                    :grow 1
                    :align center
                    :onClick storeHome}]})]})}
    :storeWishlist {:note "The wishlist: ranked games with their price and Add to Cart, sorted by rank, name, price or discount, and moved up or down."
      :params {}
      :node (node #store-wishlist-page
        :type column
        :height "{{160 + max(1, length(computed.wishSorted)) * 128}}"
        :anchors {:left 0 :right 0}
        {:type row
          :height 60
          :anchors {:left 0 :right 0}
          :alignItems center
          :gap 12
          :children [{:type image
              :file "{{'../assets/art/avatars/' + state.user.avatar + '.svg'}}"
              :width 40
              :height 40
              :radius 2}
            {:type label
              :font title
              :text "{{upper(t('store.whoseWishlist', { name: state.user.name }))}}"
              :textColor @color.bright
              :grow 1
              :height 40}]}
        {:type row
          :height 52
          :anchors {:left 0 :right 0}
          :alignItems center
          :gap 10
          :children [(node #wish-search
              :type textarea
              :width 300
              :height 32
              :padding 10
              :bind state.wishQuery
              :placeholder "{{t('store.wishSearch')}}"
              :placeholderColor @color.dim
              :textColor @store.text
              :color @store.card
              :radius 3
              :label "{{t('store.wishSearch')}}"
              :focused {:borderWidth 1 :borderColor @color.accent})
            {:type panel :color transparent :grow 1 :height 1}
            {:type label :text "{{t('store.sortBy')}}" :fitText true :font small :textColor @color.muted}
            (node #wish-sort
              :type label
              :font small
              :fitText true
              :padding 12
              :radius 2
              :color @store.chip
              :text "{{t('store.wishSort.' + state.wishSort)}} ▾"
              :textColor @store.chipText
              :focusable true
              :role button
              :cursor hand
              :isOpen "{{state.wishSortOpen}}"
              :onClick {:toggle state.wishSortOpen}
              {:use menu
                :with {:open state.wishSortOpen
                  :width 180
                  :items "{{map(['rank', 'name', 'price', 'discount'], { title: t('store.wishSort.' + it), checked: state.wishSort == it, run: 'setWishSort', with: { by: it } })}}"}})]}
        {:type panel :color transparent :height 8 :width 1}
        {:when "{{length(computed.wishSorted) == 0}}"
          :type label
          :text "{{t('store.wishEmpty')}}"
          :height 60
          :anchors {:left 0 :right 0}
          :align center
          :textColor @color.muted}
        (node #wish-list :type column :height "{{length(computed.wishSorted) * 128}}" :anchors {:left 0 :right 0} :gap 8
          (node :repeat {:for g :in "{{computed.wishSorted}}" :key "{{g.id}}" :index i}
            #"{{'wish-' + g.id}}"
            :type row
            :height 120
            :anchors {:left 0 :right 0}
            :gap 14
            :alignItems center
            :color @store.card
            :radius 2
            {:type column
              :width 34
              :height 100
              :children [{:type label
                  :font strong
                  :text "{{state.wishSort == 'rank' ? text(indexOf(state.wishlist, g.id) + 1) : ''}}"
                  :height 32
                  :anchors {:left 0 :right 0}
                  :align center
                  :textColor @color.muted}
                {:when "{{state.wishSort == 'rank' && state.wishQuery == ''}}"
                  :type label
                  :icon chevron-up
                  :text ""
                  :height 30
                  :anchors {:left 0 :right 0}
                  :align center
                  :textColor "{{i > 0 ? '@store.chipText' : '@color.dim'}}"
                  :role button
                  :focusable true
                  :cursor hand
                  :label "{{t('store.moveUp')}}"
                  :hover {:textColor "#ffffff"}
                  :onClick {:if "{{i > 0}}" :then [{:run wishMove :with {:i "{{i}}" :j "{{i - 1}}"}}]}}
                {:when "{{state.wishSort == 'rank' && state.wishQuery == ''}}"
                  :type label
                  :icon chevron-down
                  :text ""
                  :height 30
                  :anchors {:left 0 :right 0}
                  :align center
                  :textColor "{{i < length(state.wishlist) - 1 ? '@store.chipText' : '@color.dim'}}"
                  :role button
                  :focusable true
                  :cursor hand
                  :label "{{t('store.moveDown')}}"
                  :hover {:textColor "#ffffff"}
                  :onClick {:if "{{i < length(state.wishlist) - 1}}"
                    :then [{:run wishMove :with {:i "{{i}}" :j "{{i + 1}}"}}]}}]}
            {:type image
              :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
              :fit cover
              :width 230
              :height 100
              :cursor hand
              :onClick {:run openStore :with (node #"{{g.id}}")}}
            {:type column
              :grow 1
              :height 100
              :gap 4
              :children [{:type label
                  :font heading
                  :text "{{g.title}}"
                  :height 26
                  :anchors {:left 0 :right 0}
                  :textColor @color.bright
                  :cursor hand
                  :onClick {:run openStore :with (node #"{{g.id}}")}}
                {:type row
                  :height 18
                  :anchors {:left 0 :right 0}
                  :gap 6
                  :children [{:type label
                      :text "{{upper(t('store.overall'))}}"
                      :fitText true
                      :font small
                      :textColor @store.label}
                    {:type label
                      :text "{{t('review.' + g.review)}}"
                      :fitText true
                      :font small
                      :textColor @store.chipText}
                    {:type panel :color transparent :width 10 :height 1}
                    {:type label :text "{{upper(t('game.release'))}}" :fitText true :font small :textColor @store.label}
                    {:type label :text "{{g.release}}" :fitText true :font small :textColor @store.text}]}
                {:type row
                  :height 24
                  :anchors {:left 0 :right 0}
                  :gap 4
                  :children [{:repeat {:for tag :in "{{slice(g.tags, 0, 4)}}"}
                      :type label
                      :font small
                      :text "{{tag}}"
                      :fitText true
                      :padding 7
                      :radius 2
                      :color @store.chip
                      :textColor @store.chipText}]}
                {:type label
                  :font small
                  :text "{{t('store.added', { date: '12 Aug' })}}"
                  :height 18
                  :anchors {:left 0 :right 0}
                  :textColor @color.dim}]}
            {:type column
              :width 290
              :height 100
              :gap 8
              :children [{:type panel :color transparent :height 16 :width 1}
                {:type row
                  :height 36
                  :anchors {:left 0 :right 0}
                  :gap 4
                  :alignItems center
                  :children [{:type panel :color transparent :grow 1 :height 1}
                    {:use priceTag :with {:g "{{g}}"}}
                    {:use ctaButton
                      :with {:title "{{contains(state.cart, g.id) ? t('store.inCart') : t('store.addToCart')}}"}
                      :onClick [{:if "{{contains(state.cart, g.id)}}"
                          :then [{:run showCart}]
                          :else [{:run addToCart :with (node #"{{g.id}}")}]}]}]}
                {:type row
                  :height 18
                  :anchors {:left 0 :right 0}
                  :children [{:type panel :color transparent :grow 1 :height 1}
                    {:type label
                      :text "{{t('store.remove')}}"
                      :fitText true
                      :font small
                      :textColor @color.dim
                      :cursor hand
                      :role button
                      :focusable true
                      :hover {:textColor @color.bright}
                      :onClick {:run toggleWishlist :with (node #"{{g.id}}")}}]}]}
            {:type panel :color transparent :width 4 :height 1})))}
    :storePage {:note "The store: the top links and the store bar, then the front page, search results, a game's page, the cart or the wishlist."
      :params {}
      :node (node #store
        :fill true
        :color @color.page
        :gradient {:kind linear :stops [{:at 0 :color @store.glow} {:at 0.35 :color @color.page}]}
        (node #store-scroll :type column :fill true :scroll vertical
          (node #store-content
            :type column
            :height "{{computed.storeHeight}}"
            :anchors {:left "{{(window.width - computed.storeWidth) / 2}}"
              :right "{{(window.width - computed.storeWidth) / 2}}"}
            {:use storeTop}
            {:when "{{computed.storeView == 'home'}}" :use storeHome}
            {:when "{{computed.storeView == 'results'}}" :use storeResults}
            {:when "{{computed.storeView == 'app' && computed.game != null}}"
              :use storeApp
              :with {:g "{{computed.game}}"}}
            {:when "{{computed.storeView == 'cart'}}" :use storeCart}
            {:when "{{computed.storeView == 'wishlist'}}" :use storeWishlist}))
        {:use dialog
          :with {:open state.cartDialog
            :title "{{t('store.addedTitle')}}"
            :message "{{t('store.addedText', { title: find(data.games, it.id == state.cartAdded).title ?? '' })}}"
            :buttons [{:title "{{t('store.viewCart')}}" :run showCart} {:title "{{t('store.keepShopping')}}"}]}})}}
  :strings {:en {:store.keepShopping Continue
      :store.wishlist Wishlist
      :store.cart Cart
      :store.bar.yours "For you"
      :store.bar.noteworthy Fresh
      :store.bar.categories Categories
      :store.bar.points Rewards
      :store.bar.news News
      :store.bar.labs Experiments
      :store.later "Coming in a later version"
      :store.search search
      :store.featured Spotlight
      :store.nowAvailable "Now Available"
      :store.topSeller "Top Seller"
      :store.previous Previous
      :store.next Next
      :store.slide "Slide {n}"
      :store.offers "On sale now"
      :store.browseMore "Browse more"
      :store.weeklong "Deal of the week"
      :store.midweek "Short deal"
      :store.endsOn "Offer ends {date}"
      :store.categories "Browse by category"
      :store.list.new Rising
      :store.list.top "Top Sellers"
      :store.list.specials Specials
      :store.overall "Player reviews:"
      :store.inLibrary "In library"
      :store.results "{n, plural, =0 {No results} one {1 result} other {{n} results}} match “{query}”"
      :store.clear Clear
      :store.noResults "Nothing matches. Try a genre such as Racing or a tag such as Co-op."
      :store.allGames "All Games"
      :store.screenshot "Screenshot {n}"
      :store.recentReviews "Recent reviews:"
      :store.allReviews "All reviews:"
      :store.userTags "Tags players use most:"
      :store.addWishlist "Add to your wishlist"
      :store.onWishlist "On Wishlist"
      :store.follow Follow
      :store.following Following
      :store.ignore Ignore
      :store.queue "More like this"
      :store.buy "Buy {title}"
      :store.owned "{title} is already in your library"
      :store.promo "Special promotion! Offer ends {date}"
      :store.addToCart "Add to Cart"
      :store.inCart "In Cart"
      :store.playNow "Play now"
      :store.about "About this game"
      :store.aboutText "{title} is a hand-made {genre} game by a small team. Play it with a keyboard, a mouse or a controller, pick up where you left off on any computer with cloud saves, and unlock achievements as you go."
      :store.requirements "System requirements"
      :store.languages Languages
      :store.lang.interface Interface
      :store.lang.audio "Full audio"
      :store.lang.subtitles Subtitles
      :store.feature.single Single-player
      :store.feature.coop "Online co-op"
      :store.feature.achievements Achievements
      :store.feature.cloud "Cloud saves"
      :store.feature.family "Family sharing"
      :store.reviews "Customer reviews"
      :store.overallReviews "Overall reviews:"
      :store.reviewCount "({n} reviews)"
      :store.products "{n} products in account"
      :store.reviewsBy "{n, plural, one {1 review} other {{n} reviews}}"
      :store.recommended Recommended
      :store.notRecommended "Not Recommended"
      :store.onRecord "{n} h played"
      :store.posted "Posted: {date}"
      :store.helpful "Was this review helpful?"
      :store.vote.yes Yes
      :store.vote.no No
      :store.vote.funny Funny
      :store.yourCart "Your shopping cart"
      :store.thanks "Thank you! Your games are in your library."
      :store.toLibrary "Go to the library"
      :store.cartEmpty "Your cart is empty."
      :store.continue "Continue shopping"
      :store.remove Remove
      :store.moveToWishlist "Move to wishlist"
      :store.total "Estimated total"
      :store.tax "Sales tax, where it applies, is worked out at checkout."
      :store.walletLeft "Paid from your wallet: {amount}"
      :store.purchase "Purchase for myself"
      :store.whoseWishlist "{name}’s wishlist"
      :store.wishSearch "Search by name or tag"
      :store.sortBy "Sort by:"
      :store.wishSort.rank "Your rank"
      :store.wishSort.name Name
      :store.wishSort.price Price
      :store.wishSort.discount Discount
      :store.wishEmpty "Nothing on your wishlist matches."
      :store.moveUp "Move up"
      :store.moveDown "Move down"
      :store.added "Added on {date}"
      :store.addedTitle "Added to your cart!"
      :store.addedText "{title} is in your cart."
      :store.viewCart "View my cart"
      :review.overwhelminglyPositive "Overwhelmingly Positive"
      :review.veryPositive "Very Positive"
      :review.positive Positive
      :review.mostlyPositive "Mostly Positive"
      :review.mixed Mixed
      :genre.Action Action
      :genre.Adventure Adventure
      :genre.Horror Horror
      :genre.Platformer Platformer
      :genre.Puzzle Puzzle
      :genre.Racing Racing
      :genre.RPG RPG
      :genre.Shooter Shooter
      :genre.Simulation Simulation
      :genre.Strategy Strategy
      :lang.en English
      :lang.es Spanish
      :lang.fr French
      :req.minimum "Minimum:"
      :req.recommended "Recommended:"
      :req.os OS
      :req.cpu Processor
      :req.memory Memory
      :req.gpu Graphics
      :req.storage Storage
      :req.storageValue "{n} GB available space"
      :req.platform.windows Windows
      :req.platform.macos macOS
      :req.platform.linux Linux
      :req.windows.os.minimum "64-bit, version 10"
      :req.windows.os.recommended "64-bit, version 11"
      :req.macos.os.minimum "version 12"
      :req.macos.os.recommended "version 14"
      :req.linux.os.minimum "64-bit, any recent distribution"
      :req.linux.os.recommended "64-bit, kernel 6 or later"
      :req.windows.cpu.minimum "quad-core, 2.5 GHz"
      :req.windows.cpu.recommended "six-core, 3.4 GHz"
      :req.windows.memory.minimum "8 GB RAM"
      :req.windows.memory.recommended "16 GB RAM"
      :req.windows.gpu.minimum "2 GB video memory"
      :req.windows.gpu.recommended "6 GB video memory"
      :req.macos.cpu.minimum "Apple silicon or quad-core Intel"
      :req.macos.cpu.recommended "Apple silicon"
      :req.macos.memory.minimum "8 GB RAM"
      :req.macos.memory.recommended "16 GB RAM"
      :req.macos.gpu.minimum "integrated graphics"
      :req.macos.gpu.recommended "integrated graphics, 16 GB unified memory"
      :req.linux.cpu.minimum "quad-core, 2.5 GHz"
      :req.linux.cpu.recommended "six-core, 3.4 GHz"
      :req.linux.memory.minimum "8 GB RAM"
      :req.linux.memory.recommended "16 GB RAM"
      :req.linux.gpu.minimum "2 GB video memory"
      :req.linux.gpu.recommended "6 GB video memory"}
    :es {:store.keepShopping Continuar
      :store.wishlist "Lista de deseos"
      :store.cart Carrito
      :store.bar.yours "Para ti"
      :store.bar.noteworthy "Recién llegados"
      :store.bar.categories "Categorías"
      :store.bar.points Recompensas
      :store.bar.news Noticias
      :store.bar.labs Experimentos
      :store.later "Llegará en una versión posterior"
      :store.search buscar
      :store.featured "En el foco"
      :store.nowAvailable "Ya disponible"
      :store.topSeller "Más vendido"
      :store.previous Anterior
      :store.next Siguiente
      :store.slide "Diapositiva {n}"
      :store.offers "En oferta ahora"
      :store.browseMore "Ver más"
      :store.weeklong "Oferta de la semana"
      :store.midweek "Oferta corta"
      :store.endsOn "La oferta termina el {date}"
      :store.categories "Explorar por categoría"
      :store.list.new "En alza"
      :store.list.top "Más vendidos"
      :store.list.specials Ofertas
      :store.overall "Reseñas de jugadores:"
      :store.inLibrary "En la biblioteca"
      :store.results "{n, plural, =0 {Ningún resultado coincide} one {1 resultado coincide} other {{n} resultados coinciden}} con “{query}”"
      :store.clear Borrar
      :store.noResults "No hay coincidencias. Prueba con un género como Carreras o una etiqueta como Cooperativo."
      :store.allGames "Todos los juegos"
      :store.screenshot "Captura de pantalla {n}"
      :store.recentReviews "Reseñas recientes:"
      :store.allReviews "Todas las reseñas:"
      :store.userTags "Etiquetas que más usan los jugadores:"
      :store.addWishlist "Añadir a tu lista de deseos"
      :store.onWishlist "En la lista de deseos"
      :store.follow Seguir
      :store.following Siguiendo
      :store.ignore Ignorar
      :store.queue "Más como este"
      :store.buy "Comprar {title}"
      :store.owned "{title} ya está en tu biblioteca"
      :store.promo "¡Promoción especial! La oferta termina el {date}"
      :store.addToCart "Añadir al carrito"
      :store.inCart "En el carrito"
      :store.playNow "Jugar ahora"
      :store.about "Acerca de este juego"
      :store.aboutText "{title} es un juego de {genre} hecho a mano por un equipo pequeño. Juégalo con teclado, ratón o mando, retoma la partida en cualquier ordenador con las partidas guardadas en la nube y desbloquea logros por el camino."
      :store.requirements "Requisitos del sistema"
      :store.languages Idiomas
      :store.lang.interface Interfaz
      :store.lang.audio "Audio completo"
      :store.lang.subtitles "Subtítulos"
      :store.feature.single "Un jugador"
      :store.feature.coop "Cooperativo en línea"
      :store.feature.achievements Logros
      :store.feature.cloud "Partidas en la nube"
      :store.feature.family "Uso compartido en familia"
      :store.reviews "Reseñas de clientes"
      :store.overallReviews "Reseñas globales:"
      :store.reviewCount "({n} reseñas)"
      :store.products "{n} productos en la cuenta"
      :store.reviewsBy "{n, plural, one {1 reseña} other {{n} reseñas}}"
      :store.recommended Recomendado
      :store.notRecommended "No recomendado"
      :store.onRecord "{n} h jugadas"
      :store.posted "Publicada: {date}"
      :store.helpful "¿Te ha resultado útil esta reseña?"
      :store.vote.yes "Sí"
      :store.vote.no No
      :store.vote.funny Divertida
      :store.yourCart "Tu carrito de la compra"
      :store.thanks "¡Gracias! Tus juegos ya están en tu biblioteca."
      :store.toLibrary "Ir a la biblioteca"
      :store.cartEmpty "Tu carrito está vacío."
      :store.continue "Seguir comprando"
      :store.remove Quitar
      :store.moveToWishlist "Mover a la lista de deseos"
      :store.total "Total estimado"
      :store.tax "El impuesto sobre las ventas, cuando corresponda, se calcula al finalizar la compra."
      :store.walletLeft "Pagado con tu monedero: {amount}"
      :store.purchase "Comprar para mí"
      :store.whoseWishlist "Lista de deseos de {name}"
      :store.wishSearch "Buscar por nombre o etiqueta"
      :store.sortBy "Ordenar por:"
      :store.wishSort.rank "Tu clasificación"
      :store.wishSort.name Nombre
      :store.wishSort.price Precio
      :store.wishSort.discount Descuento
      :store.wishEmpty "Nada de tu lista de deseos coincide."
      :store.moveUp Subir
      :store.moveDown Bajar
      :store.added "Añadido el {date}"
      :store.addedTitle "¡Añadido a tu carrito!"
      :store.addedText "{title} está en tu carrito."
      :store.viewCart "Ver mi carrito"
      :review.overwhelminglyPositive "Extremadamente positivas"
      :review.veryPositive "Muy positivas"
      :review.positive Positivas
      :review.mostlyPositive "Mayormente positivas"
      :review.mixed Variadas
      :genre.Action "Acción"
      :genre.Adventure Aventura
      :genre.Horror Terror
      :genre.Platformer Plataformas
      :genre.Puzzle Puzles
      :genre.Racing Carreras
      :genre.RPG RPG
      :genre.Shooter Disparos
      :genre.Simulation "Simulación"
      :genre.Strategy Estrategia
      :lang.en "Inglés"
      :lang.es "Español"
      :lang.fr "Francés"
      :req.minimum "Mínimos:"
      :req.recommended "Recomendados:"
      :req.os SO
      :req.cpu Procesador
      :req.memory Memoria
      :req.gpu "Gráficos"
      :req.storage Almacenamiento
      :req.storageValue "{n} GB de espacio disponible"
      :req.platform.windows Windows
      :req.platform.macos macOS
      :req.platform.linux Linux
      :req.windows.os.minimum "64 bits, versión 10"
      :req.windows.os.recommended "64 bits, versión 11"
      :req.macos.os.minimum "versión 12"
      :req.macos.os.recommended "versión 14"
      :req.linux.os.minimum "64 bits, cualquier distribución reciente"
      :req.linux.os.recommended "64 bits, kernel 6 o posterior"
      :req.windows.cpu.minimum "cuatro núcleos, 2,5 GHz"
      :req.windows.cpu.recommended "seis núcleos, 3,4 GHz"
      :req.windows.memory.minimum "8 GB de RAM"
      :req.windows.memory.recommended "16 GB de RAM"
      :req.windows.gpu.minimum "2 GB de memoria de vídeo"
      :req.windows.gpu.recommended "6 GB de memoria de vídeo"
      :req.macos.cpu.minimum "Apple silicon o Intel de cuatro núcleos"
      :req.macos.cpu.recommended "Apple silicon"
      :req.macos.memory.minimum "8 GB de RAM"
      :req.macos.memory.recommended "16 GB de RAM"
      :req.macos.gpu.minimum "gráficos integrados"
      :req.macos.gpu.recommended "gráficos integrados, 16 GB de memoria unificada"
      :req.linux.cpu.minimum "cuatro núcleos, 2,5 GHz"
      :req.linux.cpu.recommended "seis núcleos, 3,4 GHz"
      :req.linux.memory.minimum "8 GB de RAM"
      :req.linux.memory.recommended "16 GB de RAM"
      :req.linux.gpu.minimum "2 GB de memoria de vídeo"
      :req.linux.gpu.recommended "6 GB de memoria de vídeo"}
    :fr {:store.keepShopping Continuer
      :store.wishlist "Liste de souhaits"
      :store.cart Panier
      :store.bar.yours "Pour vous"
      :store.bar.noteworthy "Tout frais"
      :store.bar.categories "Catégories"
      :store.bar.points "Récompenses"
      :store.bar.news "Actualités"
      :store.bar.labs "Expériences"
      :store.later "Dans une prochaine version"
      :store.search rechercher
      :store.featured "En vedette"
      :store.nowAvailable "Disponible maintenant"
      :store.topSeller "Meilleure vente"
      :store.previous "Précédent"
      :store.next Suivant
      :store.slide "Diapositive {n}"
      :store.offers "En promo"
      :store.browseMore "Voir plus"
      :store.weeklong "Offre de la semaine"
      :store.midweek "Offre éclair"
      :store.endsOn "L'offre se termine le {date}"
      :store.categories "Parcourir par catégorie"
      :store.list.new "En hausse"
      :store.list.top "Meilleures ventes"
      :store.list.specials Promotions
      :store.overall "Avis des joueurs :"
      :store.inLibrary "Dans la bibliothèque"
      :store.results "{n, plural, =0 {Aucun résultat ne correspond} one {1 résultat correspond} other {{n} résultats correspondent}} à « {query} »"
      :store.clear Effacer
      :store.noResults "Aucune correspondance. Essayez un genre comme Course ou une étiquette comme Coopération."
      :store.allGames "Tous les jeux"
      :store.screenshot "Capture d'écran {n}"
      :store.recentReviews "Avis récents :"
      :store.allReviews "Tous les avis :"
      :store.userTags "Étiquettes les plus utilisées par les joueurs :"
      :store.addWishlist "Ajouter à votre liste de souhaits"
      :store.onWishlist "Dans la liste de souhaits"
      :store.follow Suivre
      :store.following Suivi
      :store.ignore Ignorer
      :store.queue "Dans le même genre"
      :store.buy "Acheter {title}"
      :store.owned "{title} est déjà dans votre bibliothèque"
      :store.promo "Promotion spéciale ! L'offre se termine le {date}"
      :store.addToCart "Ajouter au panier"
      :store.inCart "Dans le panier"
      :store.playNow "Jouer maintenant"
      :store.about "À propos de ce jeu"
      :store.aboutText "{title} est un jeu de {genre} fait main par une petite équipe. Jouez au clavier, à la souris ou à la manette, reprenez votre partie sur n'importe quel ordinateur grâce aux sauvegardes dans le cloud et débloquez des succès au fil du jeu."
      :store.requirements "Configuration requise"
      :store.languages Langues
      :store.lang.interface Interface
      :store.lang.audio "Audio complet"
      :store.lang.subtitles Sous-titres
      :store.feature.single Solo
      :store.feature.coop "Coopération en ligne"
      :store.feature.achievements "Succès"
      :store.feature.cloud "Sauvegardes dans le cloud"
      :store.feature.family "Partage familial"
      :store.reviews "Avis des clients"
      :store.overallReviews "Avis globaux :"
      :store.reviewCount "({n} avis)"
      :store.products "{n} produits dans le compte"
      :store.reviewsBy "{n, plural, one {1 avis} other {{n} avis}}"
      :store.recommended "Recommandé"
      :store.notRecommended "Non recommandé"
      :store.onRecord "{n} h de jeu"
      :store.posted "Publié le {date}"
      :store.helpful "Cet avis vous a-t-il été utile ?"
      :store.vote.yes Oui
      :store.vote.no Non
      :store.vote.funny "Drôle"
      :store.yourCart "Votre panier"
      :store.thanks "Merci ! Vos jeux sont dans votre bibliothèque."
      :store.toLibrary "Aller à la bibliothèque"
      :store.cartEmpty "Votre panier est vide."
      :store.continue "Continuer vos achats"
      :store.remove Retirer
      :store.moveToWishlist "Déplacer vers la liste de souhaits"
      :store.total "Total estimé"
      :store.tax "La taxe de vente, le cas échéant, est calculée au moment du paiement."
      :store.walletLeft "Payé avec votre portefeuille : {amount}"
      :store.purchase "Acheter pour moi"
      :store.whoseWishlist "Liste de souhaits de {name}"
      :store.wishSearch "Rechercher par nom ou étiquette"
      :store.sortBy "Trier par :"
      :store.wishSort.rank "Votre classement"
      :store.wishSort.name Nom
      :store.wishSort.price Prix
      :store.wishSort.discount "Réduction"
      :store.wishEmpty "Rien dans votre liste de souhaits ne correspond."
      :store.moveUp Monter
      :store.moveDown Descendre
      :store.added "Ajouté le {date}"
      :store.addedTitle "Ajouté à votre panier !"
      :store.addedText "{title} est dans votre panier."
      :store.viewCart "Voir mon panier"
      :review.overwhelminglyPositive "Extrêmement positifs"
      :review.veryPositive "Très positifs"
      :review.positive Positifs
      :review.mostlyPositive "Plutôt positifs"
      :review.mixed "Mitigés"
      :genre.Action Action
      :genre.Adventure Aventure
      :genre.Horror Horreur
      :genre.Platformer Plateforme
      :genre.Puzzle Puzzle
      :genre.Racing Course
      :genre.RPG RPG
      :genre.Shooter Tir
      :genre.Simulation Simulation
      :genre.Strategy "Stratégie"
      :lang.en Anglais
      :lang.es Espagnol
      :lang.fr "Français"
      :req.minimum "Minimale :"
      :req.recommended "Recommandée :"
      :req.os "Système"
      :req.cpu Processeur
      :req.memory "Mémoire"
      :req.gpu Graphismes
      :req.storage Stockage
      :req.storageValue "{n} Go d'espace disponible"
      :req.platform.windows Windows
      :req.platform.macos macOS
      :req.platform.linux Linux
      :req.windows.os.minimum "64 bits, version 10"
      :req.windows.os.recommended "64 bits, version 11"
      :req.macos.os.minimum "version 12"
      :req.macos.os.recommended "version 14"
      :req.linux.os.minimum "64 bits, toute distribution récente"
      :req.linux.os.recommended "64 bits, noyau 6 ou ultérieur"
      :req.windows.cpu.minimum "quadricœur, 2,5 GHz"
      :req.windows.cpu.recommended "six cœurs, 3,4 GHz"
      :req.windows.memory.minimum "8 Go de RAM"
      :req.windows.memory.recommended "16 Go de RAM"
      :req.windows.gpu.minimum "2 Go de mémoire vidéo"
      :req.windows.gpu.recommended "6 Go de mémoire vidéo"
      :req.macos.cpu.minimum "Apple silicon ou Intel quadricœur"
      :req.macos.cpu.recommended "Apple silicon"
      :req.macos.memory.minimum "8 Go de RAM"
      :req.macos.memory.recommended "16 Go de RAM"
      :req.macos.gpu.minimum "graphismes intégrés"
      :req.macos.gpu.recommended "graphismes intégrés, 16 Go de mémoire unifiée"
      :req.linux.cpu.minimum "quadricœur, 2,5 GHz"
      :req.linux.cpu.recommended "six cœurs, 3,4 GHz"
      :req.linux.memory.minimum "8 Go de RAM"
      :req.linux.memory.recommended "16 Go de RAM"
      :req.linux.gpu.minimum "2 Go de mémoire vidéo"
      :req.linux.gpu.recommended "6 Go de mémoire vidéo"}}}
