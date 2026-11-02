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
         :reviews [{:author "MossyGolem"
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
 :components {:priceTag {:note "A game's price: the discount and the old price struck through, or 'In library' when owned."
                         :params {:g {} :big false}
                         :node (row :height "{{big ? 40 : 32}}"
                                 :width "{{g.owned ? 86 : g.discount > 0 ? (big ? 156 : 132) : (big ? 90 : 76)}}"
                                 (label :when "{{g.owned}}"
                                   :font small
                                   :text "{{upper(t('store.inLibrary'))}}"
                                   :grow 1
                                   :height "{{big ? 40 : 32}}"
                                   :align center
                                   :radius 2
                                   :color @store.chip
                                   :textColor @store.chipText)
                                 (label :when "{{!g.owned && g.discount > 0}}"
                                   :font "{{big ? 'price' : 'button'}}"
                                   :text "-{{g.discount}}%"
                                   :width "{{big ? 66 : 56}}"
                                   :height "{{big ? 40 : 32}}"
                                   :align center
                                   :color @color.badge
                                   :textColor @color.onBadge
                                   :radius {:topLeft 2 :bottomLeft 2})
                                 (node :when "{{!g.owned}}"
                                   :grow 1
                                   :height "{{big ? 40 : 32}}"
                                   :color @store.priceBox
                                   :radius "{{g.discount > 0 ? {topRight: 2, bottomRight: 2} : 2}}"
                                   (label :when "{{g.discount > 0}}"
                                     :font small
                                     :text "{{formatCurrency(g.price, 'EUR')}}"
                                     :textColor @store.oldPrice
                                     :fitText true
                                     :anchors {:right 6 :top "{{big ? 3 : 1}}"}
                                     (panel :height 1 :anchors {:left 0 :right 0 :top 8} :color @store.oldPrice))
                                   (label :font "{{big ? 'strong' : 'default'}}"
                                     :fitText true
                                     :text "{{formatCurrency(round(g.price * (100 - g.discount)) / 100, 'EUR')}}"
                                     :textColor "{{g.discount > 0 ? '@store.final' : '@store.price'}}"
                                     :anchors "{{g.discount > 0 ? {right: 6, bottom: big ? 4 : 1} : {right: 8, top: big ? 11 : 7}}}")))}
              :ctaButton {:note "The green call to action of the store (Add to Cart, Purchase)."
                          :params {:title "" :enabled true}
                          :node (label :text "{{title}}"
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
                                  :hover {:textColor "#ffffff" :shadow {:blur 10 :color "#7ff0cf55"}})}
              :quietButton {:note "The store's quiet button (wishlist, follow, player hub)."
                            :params {:title "" :icon "" :on false}
                            :node (label :text "{{title}}"
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
                                    :hover {:textColor "#ffffff"
                                            :gradient {:kind linear :angle 135 :stops ["#ffc46b" "#c9701a"]}})}
              :storeHeader {:note "A store section's heading, in capitals, with an optional button at the end."
                            :params {:title ""}
                            :node (row :height 46 :anchors {:left 0 :right 0} :alignItems end
                                    (label :font caps
                                      :text "{{upper(title)}}"
                                      :textColor @color.bright
                                      :fitText true
                                      :padding 8)
                                    (panel :color transparent :grow 1 :height 1)
                                    (node :slot true))}
              :storeTop {:note "Wishlist and cart links, then the store bar: For you, Fresh, Categories, Rewards, News, Experiments, and the search field."
                         :params {}
                         :node (column :height 86 :anchors {:left 0 :right 0}
                                 (row :height 40 :anchors {:left 0 :right 0} :alignItems center :gap 3
                                   (panel :color transparent :grow 1 :height 1)
                                   (label#store-wishlist :font small
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
                                   (label#store-cart :when "{{length(state.cart) > 0}}"
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
                                     :onClick showCart))
                                 (row#store-bar :height 40
                                   :anchors {:left 0 :right 0}
                                   :alignItems center
                                   :gap 4
                                   :radius 20
                                   :color @color.side
                                   :arrows horizontal
                                   (panel :width 1 :height 1 :color transparent)
                                   (label :id "{{'store-bar-' + item}}"
                                     :repeat {:for item
                                              :in "{{['yours', 'noteworthy', 'categories', 'points', 'news', 'labs']}}"}
                                     :let {:later "{{contains(['points', 'labs'], item)}}"}
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
                                                        :then [{:run libraryHome}
                                                               {:after 50 :do [{:scrollTo "#news-header"}]}]
                                                        :else [{:run storeHome}
                                                               {:if "{{item == 'noteworthy'}}"
                                                                :then [{:set state.storeList :to new}
                                                                       {:after 50 :do [{:scrollTo "#store-tabs"}]}]}]}]}]
                                     (node :when "{{item == 'categories'}}"
                                       :use menu
                                       :with {:open state.storeCatsOpen
                                              :width 200
                                              :items "{{map(['Action', 'Adventure', 'Horror', 'Platformer', 'Puzzle', 'Racing', 'RPG', 'Shooter', 'Simulation', 'Strategy'], { title: t('genre.' + it), run: 'storeSearch', with: { query: it } })}}"}))
                                   (panel :color transparent :grow 1 :height 1)
                                   (textarea#store-search :width 200
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
                                     (label :icon search
                                       :text ""
                                       :width 24
                                       :height 22
                                       :align center
                                       :radius 11
                                       :textColor @color.muted
                                       :anchors {:right 2 :top 2}
                                       :color transparent))
                                   (panel :color transparent :width 6 :height 1)
                                   (panel :width 1 :height 1 :color transparent))
                                 (panel :color transparent :height 10 :width 1))}
              :featured {:note "The featured carousel: a big capsule whose picture follows the screenshot under the pointer, the arrows and the dots."
                         :params {}
                         :node (node#featured :height 383
                                 :anchors {:left -46 :right -46}
                                 :children [(row :height 353
                                              :anchors {:left 46 :right 46}
                                              :shadow {:y 0 :blur 10 :color "#00000080"}
                                              (node#featured-capsule :grow 1
                                                :height 353
                                                :focusable true
                                                :role button
                                                :cursor hand
                                                :label "{{computed.feature.title}}"
                                                :onClick {:run openStore :with {:id "{{computed.feature.id}}"}}
                                                :children [(image :fill true
                                                             :fit cover
                                                             :file "{{'../assets/art/' + computed.feature.id + '/' + (state.storeShot == null ? 'hero' : state.storeShot < 4 ? 'shot' + state.storeShot : 'hero') + '.svg'}}")]
                                                :color transparent)
                                              (node#featured-side :width 324
                                                :height 353
                                                :color transparent
                                                :gradient {:kind linear :stops ["#1a1220" "#2a1d33"]}
                                                (column :anchors {:left 10 :right 10 :top 8 :bottom 10} :gap 6
                                                  (label :font title
                                                    :text "{{computed.feature.title}}"
                                                    :textColor "#ffffff"
                                                    :height 38
                                                    :anchors {:left 0 :right 0})
                                                  (row :wrap true :gap 4 :height 176 :anchors {:left 0 :right 0}
                                                    (node :repeat {:for n :in "{{[1, 2, 3, 4]}}"}
                                                      :width 150
                                                      :height 86
                                                      :cursor hand
                                                      :color "#000000"
                                                      :onHover {:set state.storeShot :to "{{n}}"}
                                                      :onLeave {:set state.storeShot :to nil}
                                                      (image :fill true
                                                        :fit cover
                                                        :file "{{'../assets/art/' + computed.feature.id + '/' + (n < 4 ? 'shot' + n : 'hero') + '.svg'}}")
                                                      (node :fill true
                                                        :color "{{state.storeShot == n ? 'transparent' : '#00000040'}}")))
                                                  (label :font heading
                                                    :text "{{t('store.nowAvailable')}}"
                                                    :textColor "#ffffff"
                                                    :height 26
                                                    :anchors {:left 0 :right 0})
                                                  (row :height 24 :anchors {:left 0 :right 0} :gap 4 :alignItems center
                                                    (label :text "{{t('store.topSeller')}}"
                                                      :fitText true
                                                      :font small
                                                      :padding 7
                                                      :radius 2
                                                      :color "#ffffff26"
                                                      :textColor "#ffffff")
                                                    (label :repeat {:for tag
                                                                    :in "{{slice(computed.feature.tags, 0, 3)}}"}
                                                      :font small
                                                      :text "{{tag}}"
                                                      :fitText true
                                                      :padding 7
                                                      :radius 2
                                                      :color "#ffffff14"
                                                      :textColor "#e2d8e6"))
                                                  (panel :color transparent :height 1 :grow 1)
                                                  (row :height 32 :anchors {:left 0 :right 0} :alignItems center
                                                    (label :text "" :fitText true :icon monitor :textColor "#e2d8e6")
                                                    (panel :color transparent :grow 1 :height 1)
                                                    (node :use priceTag :with {:g "{{computed.feature}}"})))))
                                            (label#featured-prev :icon chevron-left
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
                                            (label#featured-next :icon chevron-right
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
                                            (row :height 12 :anchors {:left 0 :right 0 :bottom 6} :gap 3 :justify center
                                              (node :repeat {:for d :in "{{range(5)}}"}
                                                :width 15
                                                :height 9
                                                :radius 2
                                                :cursor hand
                                                :color "{{d == state.storeFeature ? '@store.dotOn' : '@store.dot'}}"
                                                :role button
                                                :label "{{t('store.slide', { n: d + 1 })}}"
                                                :onClick {:set state.storeFeature :to "{{d}}"}))]
                                 :color transparent)}
              :offerCard {:note "A special offer: the game's banner, when the offer ends and its price."
                          :params {:g {}}
                          :node (node :width "{{(computed.storeWidth - 22) / 3}}"
                                  :height 196
                                  :color @store.card
                                  :radius 2
                                  :focusable true
                                  :role button
                                  :cursor hand
                                  :label "{{g.title}}"
                                  :shadow {:y 2 :blur 6 :color "#00000050"}
                                  :hover {:color @store.cardHover :shadow {:y 4 :blur 14 :color "#f5a52450"}}
                                  :onClick {:run openStore :with {:id "{{g.id}}"}}
                                  (image :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
                                    :fit cover
                                    :height 143
                                    :anchors {:left 0 :right 0 :top 0}
                                    :radius {:topLeft 2 :topRight 2})
                                  (label :text "{{t(g.discount >= 50 ? 'store.weeklong' : 'store.midweek')}}"
                                    :fitText true
                                    :font caps
                                    :textColor @store.text
                                    :anchors {:left 10 :bottom 28})
                                  (label :text "{{t('store.endsOn', { date: '2 Nov' })}}"
                                    :fitText true
                                    :font small
                                    :textColor @color.muted
                                    :anchors {:left 10 :bottom 10})
                                  (node :use priceTag :with {:g "{{g}}"} :anchors {:right 8 :bottom 10}))}
              :categoryTile {:note "A category to browse: a cover under a coloured wash and the category's name."
                             :params {:genre "" :game "" :tint ""}
                             :node (node :width "{{(computed.storeWidth - 36) / 4}}"
                                     :height 170
                                     :radius 4
                                     :color "#000000"
                                     :focusable true
                                     :role button
                                     :cursor hand
                                     :label "{{t('genre.' + genre)}}"
                                     :hover {:scale 1.03 :shadow {:blur 16 :color "#f5a52450"}}
                                     :onClick {:run storeSearch :with {:query "{{genre}}"}}
                                     (image :file "{{'../assets/art/' + game + '/hero.svg'}}"
                                       :fill true
                                       :fit cover
                                       :radius 4)
                                     (node :fill true
                                       :radius 4
                                       :color transparent
                                       :gradient {:kind linear
                                                  :stops [{:at 0.2 :color "#00000000"} {:at 1 :color "{{tint}}"}]})
                                     (label :font heading
                                       :text "{{upper(t('genre.' + genre))}}"
                                       :textColor "#ffffff"
                                       :height 30
                                       :align center
                                       :anchors {:left 0 :right 0 :bottom 14}))}
              :storeRow {:note "A game in a store list: its banner, title, tags and price; it lights up when it is the one previewed."
                         :params {:g {} :lit false}
                         :node (row :height 69
                                 :anchors {:left 0 :right 0}
                                 :alignItems center
                                 :gap 12
                                 :radius 2
                                 :color "{{lit ? '@store.preview' : '@store.card'}}"
                                 :focusable true
                                 :role button
                                 :cursor hand
                                 :label "{{g.title}}"
                                 :onClick {:run openStore :with {:id "{{g.id}}"}}
                                 (image :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
                                   :fit cover
                                   :width 184
                                   :height 69
                                   :radius {:topLeft 2 :bottomLeft 2})
                                 (column :grow 1 :height 46
                                   (label :text "{{g.title}}"
                                     :height 22
                                     :anchors {:left 0 :right 0}
                                     :textColor "{{lit ? '@store.ink' : '@store.text'}}")
                                   (label :font small
                                     :height 18
                                     :anchors {:left 0 :right 0}
                                     :icon monitor
                                     :text "  {{join(slice(g.tags, 0, 3), ', ')}}"
                                     :textColor "{{lit ? '@store.inkSoft' : '@color.muted'}}"))
                                 (node :use priceTag :with {:g "{{g}}"})
                                 (panel :color transparent :width 4 :height 1))}
              :storeHome {:note "The store's front page: featured, special offers, categories and the tabbed lists with a preview."
                          :params {}
                          :node (column#store-home :height 1960 :anchors {:left 0 :right 0}
                                  (node :use storeHeader :with {:title "{{t('store.featured')}}"})
                                  (node :use featured)
                                  (node :use storeHeader :with {:title "{{t('store.offers')}}"}
                                    (node :use quietButton
                                      :with {:title "{{t('store.browseMore')}}"}
                                      :onClick {:run storeSearch :with {:query "{{''}}"}}))
                                  (row#offers :wrap true :gap 11 :height 410 :anchors {:left 0 :right 0}
                                    (node :id "{{'offer-' + g.id}}"
                                      :repeat {:for g :in "{{computed.offers}}" :key "{{g.id}}"}
                                      :use offerCard
                                      :with {:g "{{g}}"}))
                                  (panel :color transparent :height 14 :width 1)
                                  (node :use storeHeader :with {:title "{{t('store.categories')}}"})
                                  (row#categories :gap 12 :height 170 :anchors {:left 0 :right 0}
                                    (node :use categoryTile :with {:genre "Racing" :game aurora :tint "#d8357ae0"})
                                    (node :use categoryTile :with {:genre "Strategy" :game clockwork :tint "#2f6fd8e0"})
                                    (node :use categoryTile
                                      :with {:genre "Simulation" :game saltmarsh :tint "#2aa37ae0"})
                                    (node :use categoryTile :with {:genre "Shooter" :game glasswing :tint "#d8762fe0"}))
                                  (panel :color transparent :height 30 :width 1)
                                  (row#store-tabs :height 34
                                    :anchors {:left 0 :right 0}
                                    :arrows horizontal
                                    :role tablist
                                    (label :id "{{'store-tab-' + tab}}"
                                      :repeat {:for tab :in "{{['new', 'top', 'specials']}}"}
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
                                  (row :height 600 :anchors {:left 0 :right 0} :gap 0 :alignItems start
                                    (column#store-list :grow 1 :height 600 :gap 5
                                      (node :id "{{'store-row-' + g.id}}"
                                        :repeat {:for g :in "{{computed.storeRows}}" :key "{{g.id}}"}
                                        :use storeRow
                                        :with {:g "{{g}}" :lit "{{computed.storePreview.id == g.id}}"}
                                        :onHover {:set state.storeHover :to "{{g.id}}"}))
                                    (node#store-preview :width 308
                                      :height 592
                                      :color transparent
                                      :gradient {:kind linear :stops [@store.preview @store.previewEnd]}
                                      (column :anchors {:left 14 :right 14 :top 12 :bottom 12} :gap 6
                                        (label :font heading
                                          :text "{{computed.storePreview.title}}"
                                          :textColor @store.ink
                                          :height 26
                                          :anchors {:left 0 :right 0})
                                        (row :height 20 :anchors {:left 0 :right 0} :gap 6
                                          (label :text "{{t('store.overall')}}"
                                            :fitText true
                                            :font small
                                            :textColor @store.inkSoft)
                                          (label :text "{{t('review.' + computed.storePreview.review)}}"
                                            :fitText true
                                            :font small
                                            :textColor "#5a2f70")
                                          (label :text "({{formatNumber(computed.storePreview.reviewCount)}})"
                                            :fitText true
                                            :font small
                                            :textColor @store.inkSoft))
                                        (row :height 24 :anchors {:left 0 :right 0} :gap 4
                                          (label :repeat {:for tag :in "{{slice(computed.storePreview.tags, 0, 4)}}"}
                                            :font small
                                            :text "{{tag}}"
                                            :fitText true
                                            :padding 6
                                            :radius 2
                                            :color "#ffffff55"
                                            :textColor @store.inkSoft))
                                        (image :repeat {:for n :in "{{[1, 2, 3]}}"}
                                          :height 158
                                          :anchors {:left 0 :right 0}
                                          :fit cover
                                          :file "{{'../assets/art/' + computed.storePreview.id + '/shot' + n + '.svg'}}")))))}
              :storeResults {:params {}
                             :node (column#store-results :height "{{110 + 74 * max(1, length(computed.results))}}"
                                     :anchors {:left 0 :right 0}
                                     :gap 5
                                     (row :height 50 :anchors {:left 0 :right 0} :alignItems center :gap 10
                                       (label :text "{{t('store.results', { n: length(computed.results), query: state.storeQuery })}}"
                                         :fitText true
                                         :textColor @store.text)
                                       (panel :color transparent :grow 1 :height 1)
                                       (node :use quietButton
                                         :with {:title "{{t('store.clear')}}" :icon circle-x}
                                         :onClick {:set state.storeQuery :to ""}))
                                     (label :when "{{length(computed.results) == 0}}"
                                       :text "{{t('store.noResults')}}"
                                       :textColor @color.muted
                                       :height 40
                                       :anchors {:left 0 :right 0})
                                     (node :id "{{'result-' + g.id}}"
                                       :repeat {:for g :in "{{computed.results}}" :key "{{g.id}}"}
                                       :use storeRow
                                       :with {:g "{{g}}"}))}
              :requirements {:params {:g {} :level ""}
                             :node (column :grow 1 :height 150 :gap 4
                                     (label :font caps
                                       :text "{{upper(t('req.' + level))}}"
                                       :textColor @store.text
                                       :height 22
                                       :anchors {:left 0 :right 0})
                                     (label :repeat {:for r :in "{{['os', 'cpu', 'memory', 'gpu', 'storage']}}"}
                                       :font small
                                       :height 20
                                       :anchors {:left 0 :right 0}
                                       :runs [{:text "{{t('req.' + r) + ': '}}" :bold true :color @store.text}
                                              {:text "{{r == 'storage' ? t('req.storageValue', { n: ceil(g.sizeGb * (level == 'recommended' ? 1.5 : 1)) }) : t('req.' + state.reqTab + '.' + r + '.' + level)}}"
                                               :color @color.muted}]))}
              :storeApp {:note "A game's store page: the gallery and its strip, the header capsule with reviews and tags, buying, the description, requirements and reviews."
                         :params {:g {}}
                         :node (column#store-app :height 1670 :anchors {:left 0 :right 0}
                                 (row :height 22 :anchors {:left 0 :right 0} :gap 6 :alignItems center
                                   (label :text "{{t('store.allGames')}}"
                                     :fitText true
                                     :font small
                                     :textColor @color.muted
                                     :cursor hand
                                     :role link
                                     :focusable true
                                     :hover {:textColor @color.bright}
                                     :onClick storeHome)
                                   (label :text ""
                                     :fitText true
                                     :icon chevron-right
                                     :mirror true
                                     :font small
                                     :textColor @color.dim)
                                   (label :text "{{t('genre.' + g.genre)}}"
                                     :fitText true
                                     :font small
                                     :textColor @color.muted
                                     :cursor hand
                                     :role link
                                     :focusable true
                                     :hover {:textColor @color.bright}
                                     :onClick {:run storeSearch :with {:query "{{g.genre}}"}})
                                   (label :text ""
                                     :fitText true
                                     :icon chevron-right
                                     :mirror true
                                     :font small
                                     :textColor @color.dim)
                                   (label :text "{{g.title}}" :fitText true :font small :textColor @color.muted))
                                 (row :height 48 :anchors {:left 0 :right 0} :alignItems center
                                   (label#store-title :font title
                                     :text "{{g.title}}"
                                     :textColor @color.bright
                                     :grow 1
                                     :height 40)
                                   (node :use quietButton :with {:title "{{t('game.section.hub')}}"}))
                                 (row#store-top :height 424 :anchors {:left 0 :right 0} :gap 16 :color @store.card
                                   (column#gallery :grow 1 :height 424 :gap 8
                                     (image :height "{{min(338, round((computed.storeWidth - 340) * 0.5625))}}"
                                       :anchors {:left 0 :right 0}
                                       :fit cover
                                       :color "#000000"
                                       :file "{{'../assets/art/' + g.id + '/' + ['hero', 'shot1', 'shot2', 'shot3'][state.shotIndex] + '.svg'}}")
                                     (row#gallery-strip :height 72 :anchors {:left 0 :right 0} :gap 4 :arrows horizontal
                                       (node :id "{{'shot-' + s}}"
                                         :repeat {:for s :in "{{range(4)}}"}
                                         :width 116
                                         :height 65
                                         :focusable true
                                         :role button
                                         :cursor hand
                                         :label "{{t('store.screenshot', { n: s + 1 })}}"
                                         :selected "{{state.shotIndex == s}}"
                                         :onClick {:set state.shotIndex :to "{{s}}"}
                                         :children [(image :fill true
                                                      :fit cover
                                                      :file "{{'../assets/art/' + g.id + '/' + ['hero', 'shot1', 'shot2', 'shot3'][s] + '.svg'}}")
                                                    (node :fill true
                                                      :color "{{state.shotIndex == s ? 'transparent' : '#00000055'}}"
                                                      :borderWidth "{{state.shotIndex == s ? 2 : 0}}"
                                                      :borderColor "#ffffff")]
                                         :color transparent)))
                                   (column#store-side :width 314 :height 424 :gap 4
                                     (image :height 151
                                       :anchors {:left 0 :right 0}
                                       :fit cover
                                       :file "{{'../assets/art/' + g.id + '/hero.svg'}}")
                                     (panel :color transparent :height 4 :width 1)
                                     (label :font small
                                       :text "{{g.blurb}}"
                                       :wrap true
                                       :height 40
                                       :anchors {:left 0 :right 0}
                                       :textColor @store.text)
                                     (row :height 20 :anchors {:left 0 :right 0}
                                       (label :font small
                                         :text "{{upper(t('store.recentReviews'))}}"
                                         :width 110
                                         :textColor @store.label
                                         :height 20
                                         :align {:horizontal start :vertical center})
                                       (label :font small
                                         :text "{{t('review.' + g.review)}}"
                                         :grow 1
                                         :textColor @store.chipText
                                         :height 20
                                         :align {:horizontal start :vertical center}))
                                     (row :height 20 :anchors {:left 0 :right 0}
                                       (label :font small
                                         :text "{{upper(t('store.allReviews'))}}"
                                         :width 110
                                         :textColor @store.label
                                         :height 20
                                         :align {:horizontal start :vertical center})
                                       (label :font small
                                         :text "{{t('review.' + g.review) + ' (' + formatNumber(g.reviewCount) + ')'}}"
                                         :grow 1
                                         :textColor @store.chipText
                                         :height 20
                                         :align {:horizontal start :vertical center}))
                                     (row :height 20 :anchors {:left 0 :right 0}
                                       (label :font small
                                         :text "{{upper(t('game.release'))}}"
                                         :width 110
                                         :textColor @store.label
                                         :height 20
                                         :align {:horizontal start :vertical center})
                                       (label :font small
                                         :text "{{g.release}}"
                                         :grow 1
                                         :textColor @store.text
                                         :height 20
                                         :align {:horizontal start :vertical center}))
                                     (panel :color transparent :height 4 :width 1)
                                     (row :height 20 :anchors {:left 0 :right 0}
                                       (label :font small
                                         :text "{{upper(t('game.developer'))}}"
                                         :width 110
                                         :textColor @store.label
                                         :height 20
                                         :align {:horizontal start :vertical center})
                                       (label :font small
                                         :text "{{g.developer}}"
                                         :grow 1
                                         :textColor @store.chipText
                                         :height 20
                                         :align {:horizontal start :vertical center}))
                                     (row :height 20 :anchors {:left 0 :right 0}
                                       (label :font small
                                         :text "{{upper(t('game.publisher'))}}"
                                         :width 110
                                         :textColor @store.label
                                         :height 20
                                         :align {:horizontal start :vertical center})
                                       (label :font small
                                         :text "{{g.publisher}}"
                                         :grow 1
                                         :textColor @store.chipText
                                         :height 20
                                         :align {:horizontal start :vertical center}))
                                     (panel :color transparent :height 8 :width 1)
                                     (label :font small
                                       :text "{{t('store.userTags')}}"
                                       :height 18
                                       :anchors {:left 0 :right 0}
                                       :textColor @store.label)
                                     (row :wrap true :gap 3 :height 26 :anchors {:left 0 :right 0}
                                       (label :repeat {:for tag :in "{{g.tags}}"}
                                         :font small
                                         :text "{{tag}}"
                                         :fitText true
                                         :padding 7
                                         :radius 2
                                         :color @store.chip
                                         :textColor @store.chipText
                                         :cursor hand
                                         :hover {:textColor "#ffffff"}
                                         :onClick {:run storeSearch :with {:query "{{tag}}"}})
                                       (label :text "+"
                                         :fitText true
                                         :font small
                                         :padding 7
                                         :radius 2
                                         :color @store.chip
                                         :textColor @store.chipText)))
                                   (panel :color transparent :width 10 :height 1))
                                 (panel :color transparent :height 16 :width 1)
                                 (row#store-queue :height 44
                                   :anchors {:left 0 :right 0}
                                   :gap 6
                                   :alignItems center
                                   :color @store.card
                                   (panel :color transparent :width 4 :height 1)
                                   (node#store-wish :use quietButton
                                     :with {:title "{{contains(state.wishlist, g.id) ? t('store.onWishlist') : t('store.addWishlist')}}"
                                            :icon "{{contains(state.wishlist, g.id) ? 'check' : ''}}"
                                            :on "{{contains(state.wishlist, g.id)}}"}
                                     :onClick {:run toggleWishlist :with {:id "{{g.id}}"}})
                                   (node#store-follow :use quietButton
                                     :with {:title "{{contains(state.followed, g.id) ? t('store.following') : t('store.follow')}}"
                                            :icon "{{contains(state.followed, g.id) ? 'check' : ''}}"
                                            :on "{{contains(state.followed, g.id)}}"}
                                     :onClick {:run toggleFollow :with {:id "{{g.id}}"}})
                                   (node :use quietButton :with {:title "{{t('store.ignore')}}"})
                                   (panel :color transparent :grow 1 :height 1)
                                   (node :use quietButton :with {:title "{{t('store.queue')}}" :icon list})
                                   (panel :color transparent :width 4 :height 1))
                                 (panel :color transparent :height 20 :width 1)
                                 (row :height 480 :anchors {:left 0 :right 0} :gap 16 :alignItems start
                                   (column :grow 1 :height 480
                                     (node#purchase :height 104
                                       :anchors {:left 0 :right 0}
                                       :radius 4
                                       :color transparent
                                       :gradient {:kind linear :angle 120 :stops ["#5e4b6c80" "#4a385a66"]}
                                       (label :when "{{!g.owned}}"
                                         :font heading
                                         :text "{{t('store.buy', { title: g.title })}}"
                                         :textColor "#ffffff"
                                         :fitText true
                                         :anchors {:left 16 :top 14})
                                       (label :when "{{g.owned}}"
                                         :font heading
                                         :text "{{t('store.owned', { title: g.title })}}"
                                         :textColor "#ffffff"
                                         :fitText true
                                         :anchors {:left 16 :top 14})
                                       (label :when "{{!g.owned && g.discount > 0}}"
                                         :font small
                                         :text "{{upper(t('store.promo', { date: '2 Nov' }))}}"
                                         :textColor "#ffd9a0"
                                         :fitText true
                                         :anchors {:left 16 :top 42})
                                       (label :text ""
                                         :fitText true
                                         :icon monitor
                                         :textColor "#e2d8e6"
                                         :anchors {:right 16 :top 16})
                                       (row :height 44
                                         :anchors {:right 14 :bottom 12}
                                         :width "{{g.owned ? 118 : g.discount > 0 ? 262 : 200}}"
                                         :color "#000000"
                                         :radius 3
                                         :alignItems center
                                         :gap 6
                                         (panel :color transparent :grow 1 :height 1)
                                         (node :when "{{!g.owned}}" :use priceTag :with {:g "{{g}}" :big false})
                                         (node#add-to-cart :when "{{!g.owned}}"
                                           :use ctaButton
                                           :with {:title "{{contains(state.cart, g.id) ? t('store.inCart') : t('store.addToCart')}}"}
                                           :onClick [{:if "{{contains(state.cart, g.id)}}"
                                                      :then [{:run showCart}]
                                                      :else [{:run addToCart :with {:id "{{g.id}}"}}]}])
                                         (node :when "{{g.owned}}"
                                           :use ctaButton
                                           :with {:title "{{t('store.playNow')}}"}
                                           :onClick {:run openGame :with {:id "{{g.id}}"}})
                                         (panel :color transparent :width 2 :height 1)))
                                     (panel :color transparent :height 6 :width 1)
                                     (column :height 34 :anchors {:left 0 :right 0}
                                       (panel :color transparent :height 10 :width 1)
                                       (label :font caps
                                         :text "{{upper(t('store.about'))}}"
                                         :textColor @color.bright
                                         :height 20
                                         :anchors {:left 0 :right 0})
                                       (panel :height 1
                                         :anchors {:left 0 :right 0}
                                         :color transparent
                                         :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}))
                                     (label :text "{{g.blurb + ' ' + t('store.aboutText', { title: g.title, genre: lower(t('genre.' + g.genre)) })}}"
                                       :wrap true
                                       :height 76
                                       :anchors {:left 0 :right 0}
                                       :textColor @store.text
                                       :padding 0)
                                     (column :height 34 :anchors {:left 0 :right 0}
                                       (panel :color transparent :height 10 :width 1)
                                       (label :font caps
                                         :text "{{upper(t('store.requirements'))}}"
                                         :textColor @color.bright
                                         :height 20
                                         :anchors {:left 0 :right 0})
                                       (panel :height 1
                                         :anchors {:left 0 :right 0}
                                         :color transparent
                                         :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}))
                                     (row#req-tabs :height 40
                                       :anchors {:left 0 :right 0}
                                       :gap 2
                                       :alignItems center
                                       :arrows horizontal
                                       :role tablist
                                       (label :id "{{'req-' + p}}"
                                         :repeat {:for p :in "{{['windows', 'macos', 'linux']}}"}
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
                                     (row :height 150 :anchors {:left 0 :right 0} :gap 20
                                       (node :use requirements :with {:g "{{g}}" :level minimum})
                                       (node :use requirements :with {:g "{{g}}" :level recommended})))
                                   (column :width 308 :height 480 :gap 12
                                     (column#features :height 196 :anchors {:left 0 :right 0} :color @store.card :gap 2
                                       (panel :color transparent :height 6 :width 1)
                                       (row :repeat {:for f
                                                     :in "{{filter([{ id: 'single', icon: 'user', on: true }, { id: 'coop', icon: 'users', on: contains(g.tags, 'Co-op') }, { id: 'achievements', icon: 'trophy', on: true }, { id: 'controller', icon: 'gamepad-2', on: g.controller != 'none' }, { id: 'cloud', icon: 'cloud', on: true }, { id: 'family', icon: 'share-2', on: true }], it.on)}}"}
                                         :height 30
                                         :anchors {:left 8 :right 8}
                                         :gap 8
                                         :alignItems center
                                         :color "#ffffff0a"
                                         (label :icon "{{f.icon}}"
                                           :text ""
                                           :width 30
                                           :height 30
                                           :align center
                                           :textColor @store.chipText
                                           :color "#00000033")
                                         (label :font small
                                           :text "{{f.id == 'controller' ? t('controller.' + g.controller) : t('store.feature.' + f.id)}}"
                                           :grow 1
                                           :textColor @store.chipText
                                           :height 30
                                           :align {:horizontal start :vertical center})))
                                     (column#languages :height 150 :anchors {:left 0 :right 0} :color @store.card
                                       (panel :color transparent :height 8 :width 1)
                                       (row :height 22 :anchors {:left 10 :right 10}
                                         (label :font caps
                                           :text "{{t('store.languages')}}"
                                           :grow 1
                                           :textColor @store.text
                                           :height 22
                                           :align {:horizontal start :vertical center})
                                         (label :repeat {:for c :in "{{['interface', 'audio', 'subtitles']}}"}
                                           :font small
                                           :text "{{t('store.lang.' + c)}}"
                                           :width 62
                                           :align center
                                           :textColor @color.muted
                                           :height 22))
                                       (row :repeat {:for l
                                                     :in "{{[['en', true, true, true], ['es', true, false, true], ['fr', true, false, true]]}}"}
                                         :height 26
                                         :anchors {:left 10 :right 10}
                                         (label :font small
                                           :text "{{t('lang.' + l[0])}}"
                                           :grow 1
                                           :textColor @store.text
                                           :height 26
                                           :align {:horizontal start :vertical center})
                                         (label :repeat {:for k :in "{{[1, 2, 3]}}"}
                                           :icon "{{l[k] ? 'check' : ''}}"
                                           :text ""
                                           :width 62
                                           :align center
                                           :textColor @color.ingame
                                           :height 26)))))
                                 (column :height 34 :anchors {:left 0 :right 0}
                                   (panel :color transparent :height 10 :width 1)
                                   (label :font caps
                                     :text "{{upper(t('store.reviews'))}}"
                                     :textColor @color.bright
                                     :height 20
                                     :anchors {:left 0 :right 0})
                                   (panel :height 1
                                     :anchors {:left 0 :right 0}
                                     :color transparent
                                     :gradient {:kind linear :angle 90 :stops ["#5a3f6b" "#5a3f6b00"]}))
                                 (panel :color transparent :height 10 :width 1)
                                 (row#review-summary :height 56
                                   :anchors {:left 0 :right 0}
                                   :gap 10
                                   :alignItems center
                                   :color @store.card
                                   (panel :color transparent :width 4 :height 1)
                                   (label :text "{{t('store.overallReviews')}}"
                                     :fitText true
                                     :font caps
                                     :textColor @store.text)
                                   (label :text "{{t('review.' + g.review)}}"
                                     :fitText true
                                     :font strong
                                     :textColor @store.chipText)
                                   (label :text "{{t('store.reviewCount', { n: formatNumber(g.reviewCount) })}}"
                                     :fitText true
                                     :font small
                                     :textColor @color.muted)
                                   (panel :color transparent :grow 1 :height 1)
                                   (node :width 160 :height 8 :radius 4 :color @store.down
                                     (node :height 8
                                       :radius 4
                                       :color @store.chipText
                                       :width "{{text(g.reviewPercent) + '%'}}"
                                       :anchors {:left 0 :top 0}))
                                   (label :text "{{g.reviewPercent}}%" :fitText true :font small :textColor @color.muted)
                                   (panel :color transparent :width 8 :height 1))
                                 (panel :color transparent :height 10 :width 1)
                                 (column#reviews :height 432 :anchors {:left 0 :right 0} :gap 10
                                   (row :repeat {:for r :in "{{state.reviews}}"}
                                     :height 136
                                     :anchors {:left 0 :right 0}
                                     :gap 0
                                     :color @store.card
                                     (column :width 200 :height 136 :gap 4
                                       (panel :color transparent :height 10 :width 1)
                                       (row :height 36 :anchors {:left 10 :right 6} :gap 8 :alignItems center
                                         (image :file "{{'../assets/art/avatars/' + r.avatar + '.svg'}}"
                                           :width 34
                                           :height 34
                                           :radius 2
                                           :borderWidth 1
                                           :borderColor "#6b5280")
                                         (label :font strong
                                           :text "{{r.author}}"
                                           :grow 1
                                           :textColor "#f0d9c0"
                                           :height 36
                                           :align {:horizontal start :vertical center}))
                                       (label :font small
                                         :text "{{t('store.products', { n: r.products })}}"
                                         :height 18
                                         :anchors {:left 10 :right 6}
                                         :textColor @color.muted)
                                       (label :font small
                                         :text "{{t('store.reviewsBy', { n: r.reviews })}}"
                                         :height 18
                                         :anchors {:left 10 :right 6}
                                         :textColor @color.muted))
                                     (column :grow 1 :height 136 :gap 6
                                       (panel :color transparent :height 8 :width 1)
                                       (row :height 40
                                         :anchors {:left 0 :right 10}
                                         :gap 10
                                         :alignItems center
                                         :color "#00000033"
                                         (label :icon "{{r.up ? 'thumbs-up' : 'thumbs-down'}}"
                                           :text ""
                                           :font heading
                                           :width 40
                                           :height 40
                                           :align center
                                           :textColor "{{r.up ? '@store.chipText' : '@store.down'}}"
                                           :color "#0000004d")
                                         (column :grow 1 :height 36
                                           (label :font heading
                                             :text "{{t(r.up ? 'store.recommended' : 'store.notRecommended')}}"
                                             :height 20
                                             :anchors {:left 0 :right 0}
                                             :textColor "#d6d7d8")
                                           (label :font small
                                             :text "{{t('store.onRecord', { n: formatNumber(r.hours, 1, 1) })}}"
                                             :height 16
                                             :anchors {:left 0 :right 0}
                                             :textColor @color.muted)))
                                       (label :font small
                                         :text "{{upper(t('store.posted', { date: r.posted }))}}"
                                         :height 16
                                         :anchors {:left 0 :right 0}
                                         :textColor @color.dim)
                                       (label :text "{{r.text}}"
                                         :wrap true
                                         :height 30
                                         :anchors {:left 0 :right 14}
                                         :textColor @store.text)
                                       (row :height 22 :anchors {:left 0 :right 0} :gap 4 :alignItems center
                                         (label :text "{{t('store.helpful')}}"
                                           :fitText true
                                           :font small
                                           :textColor @color.dim)
                                         (label :id "{{'vote-' + slug(r.author) + '-' + b}}"
                                           :repeat {:for b :in "{{['yes', 'no', 'funny']}}"}
                                           :let {:on "{{state.votes[slug(r.author)] == b}}"}
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
                                           :onClick {:set "{{'state.votes.' + slug(r.author)}}" :to "{{on ? null : b}}"}))))))}
              :storeCart {:note "The cart: the games and their prices, the estimated total from the wallet, and a receipt after buying."
                          :params {}
                          :node (column#store-cart-page :height "{{140 + max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160)}}"
                                  :anchors {:left 0 :right 0}
                                  (label :font title
                                    :text "{{upper(t('store.yourCart'))}}"
                                    :textColor @color.bright
                                    :height 60
                                    :anchors {:left 0 :right 0})
                                  (row :height "{{max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160)}}"
                                    :anchors {:left 0 :right 0}
                                    :gap 16
                                    :alignItems start
                                    (column :grow 1
                                      :height "{{max(250, length(computed.cartGames) * 106 + 60, length(state.receipt) * 40 + 160)}}"
                                      :gap 6
                                      (column#receipt :when "{{length(state.receipt) > 0}}"
                                        :height "{{length(state.receipt) * 40 + 130}}"
                                        :anchors {:left 0 :right 0}
                                        :color @store.card
                                        :gap 6
                                        (panel :color transparent :height 8 :width 1)
                                        (label :font heading
                                          :icon circle-check
                                          :text " {{t('store.thanks')}}"
                                          :height 30
                                          :anchors {:left 16 :right 16}
                                          :textColor @color.ingame)
                                        (row :repeat {:for id :in "{{state.receipt}}"}
                                          :height 34
                                          :anchors {:left 16 :right 16}
                                          :gap 10
                                          :alignItems center
                                          (image :file "{{'../assets/art/' + id + '/hero.svg'}}"
                                            :fit cover
                                            :width 90
                                            :height 34)
                                          (label :text "{{find(data.games, it.id == id).title}}"
                                            :grow 1
                                            :textColor @store.text
                                            :height 34
                                            :align {:horizontal start :vertical center})
                                          (node :use quietButton
                                            :with {:title "{{t('status.button.none')}}" :icon download}
                                            :onClick {:run install :with {:id "{{id}}"}}))
                                        (row :height 40 :anchors {:left 16 :right 16} :alignItems center
                                          (node :use quietButton
                                            :with {:title "{{t('store.toLibrary')}}"}
                                            :onClick showLibrary)))
                                      (column :when "{{length(computed.cartGames) == 0 && length(state.receipt) == 0}}"
                                        :height 120
                                        :anchors {:left 0 :right 0}
                                        :color @store.card
                                        (panel :color transparent :height 24 :width 1)
                                        (label :font heading
                                          :text "{{t('store.cartEmpty')}}"
                                          :align center
                                          :height 30
                                          :anchors {:left 0 :right 0}
                                          :textColor @store.text)
                                        (row :height 40 :anchors {:left 0 :right 0} :justify center :alignItems center
                                          (node :use quietButton
                                            :with {:title "{{t('store.continue')}}"}
                                            :onClick storeHome)))
                                      (row :id "{{'cart-' + g.id}}"
                                        :repeat {:for g :in "{{computed.cartGames}}" :key "{{g.id}}"}
                                        :height 100
                                        :anchors {:left 0 :right 0}
                                        :gap 14
                                        :alignItems center
                                        :color @store.card
                                        :radius 2
                                        (panel :color transparent :width 2 :height 1)
                                        (image :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
                                          :fit cover
                                          :width 184
                                          :height 69
                                          :cursor hand
                                          :onClick {:run openStore :with {:id "{{g.id}}"}})
                                        (column :grow 1 :height 69 :gap 4
                                          (label :font strong
                                            :text "{{g.title}}"
                                            :height 22
                                            :anchors {:left 0 :right 0}
                                            :textColor @color.bright)
                                          (label :text ""
                                            :fitText true
                                            :icon monitor
                                            :font small
                                            :textColor @color.muted)
                                          (row :height 20 :anchors {:left 0 :right 0} :gap 14
                                            (label :text "{{t('store.remove')}}"
                                              :fitText true
                                              :font small
                                              :textColor @color.muted
                                              :cursor hand
                                              :role button
                                              :focusable true
                                              :hover {:textColor @color.bright}
                                              :onClick {:run removeFromCart :with {:id "{{g.id}}"}})
                                            (label :text "{{t('store.moveToWishlist')}}"
                                              :fitText true
                                              :font small
                                              :textColor @color.muted
                                              :cursor hand
                                              :role button
                                              :focusable true
                                              :hover {:textColor @color.bright}
                                              :onClick {:run toWishlist :with {:id "{{g.id}}"}})))
                                        (node :use priceTag :with {:g "{{g}}"})
                                        (panel :color transparent :width 8 :height 1)))
                                    (column#cart-summary :width 308 :height 236 :color @store.card :radius 2 :gap 8
                                      (panel :color transparent :height 8 :width 1)
                                      (row :height 30 :anchors {:left 16 :right 16} :alignItems center
                                        (label :font heading
                                          :text "{{t('store.total')}}"
                                          :grow 1
                                          :textColor @color.bright
                                          :height 30
                                          :align {:horizontal start :vertical center})
                                        (label :font heading
                                          :text "{{formatCurrency(computed.cartTotal, 'EUR')}}"
                                          :fitText true
                                          :textColor @color.bright))
                                      (label :font small
                                        :text "{{t('store.tax')}}"
                                        :wrap true
                                        :height 32
                                        :anchors {:left 16 :right 16}
                                        :textColor @color.muted)
                                      (label :font small
                                        :text "{{t('store.walletLeft', { amount: formatCurrency(state.user.wallet, 'EUR') })}}"
                                        :height 18
                                        :anchors {:left 16 :right 16}
                                        :textColor "{{computed.cartTotal > state.user.wallet ? '@store.down' : '@color.muted'}}")
                                      (row :height 44 :anchors {:left 16 :right 16} :alignItems center
                                        (node#purchase-button :use ctaButton
                                          :grow 1
                                          :align center
                                          :with {:title "{{t('store.purchase')}}"
                                                 :enabled "{{length(state.cart) > 0 && computed.cartTotal <= state.user.wallet}}"}
                                          :onClick purchase))
                                      (row :height 34 :anchors {:left 16 :right 16} :alignItems center
                                        (node :use quietButton
                                          :with {:title "{{t('store.continue')}}"}
                                          :grow 1
                                          :align center
                                          :onClick storeHome)))))}
              :storeWishlist {:note "The wishlist: ranked games with their price and Add to Cart, sorted by rank, name, price or discount, and moved up or down."
                              :params {}
                              :node (column#store-wishlist-page :height "{{160 + max(1, length(computed.wishSorted)) * 128}}"
                                      :anchors {:left 0 :right 0}
                                      (row :height 60 :anchors {:left 0 :right 0} :alignItems center :gap 12
                                        (image :file "{{'../assets/art/avatars/' + state.user.avatar + '.svg'}}"
                                          :width 40
                                          :height 40
                                          :radius 2)
                                        (label :font title
                                          :text "{{upper(t('store.whoseWishlist', { name: state.user.name }))}}"
                                          :textColor @color.bright
                                          :grow 1
                                          :height 40))
                                      (row :height 52 :anchors {:left 0 :right 0} :alignItems center :gap 10
                                        (textarea#wish-search :width 300
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
                                        (panel :color transparent :grow 1 :height 1)
                                        (label :text "{{t('store.sortBy')}}"
                                          :fitText true
                                          :font small
                                          :textColor @color.muted)
                                        (label#wish-sort :font small
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
                                          (node :use menu
                                            :with {:open state.wishSortOpen
                                                   :width 180
                                                   :items "{{map(['rank', 'name', 'price', 'discount'], { title: t('store.wishSort.' + it), checked: state.wishSort == it, run: 'setWishSort', with: { by: it } })}}"})))
                                      (panel :color transparent :height 8 :width 1)
                                      (label :when "{{length(computed.wishSorted) == 0}}"
                                        :text "{{t('store.wishEmpty')}}"
                                        :height 60
                                        :anchors {:left 0 :right 0}
                                        :align center
                                        :textColor @color.muted)
                                      (column#wish-list :height "{{length(computed.wishSorted) * 128}}"
                                        :anchors {:left 0 :right 0}
                                        :gap 8
                                        (row :id "{{'wish-' + g.id}}"
                                          :repeat {:for g :in "{{computed.wishSorted}}" :key "{{g.id}}" :index i}
                                          :height 120
                                          :anchors {:left 0 :right 0}
                                          :gap 14
                                          :alignItems center
                                          :color @store.card
                                          :radius 2
                                          (column :width 34 :height 100
                                            (label :font strong
                                              :text "{{state.wishSort == 'rank' ? text(indexOf(state.wishlist, g.id) + 1) : ''}}"
                                              :height 32
                                              :anchors {:left 0 :right 0}
                                              :align center
                                              :textColor @color.muted)
                                            (label :when "{{state.wishSort == 'rank' && state.wishQuery == ''}}"
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
                                              :onClick {:if "{{i > 0}}"
                                                        :then [{:run wishMove :with {:i "{{i}}" :j "{{i - 1}}"}}]})
                                            (label :when "{{state.wishSort == 'rank' && state.wishQuery == ''}}"
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
                                                        :then [{:run wishMove :with {:i "{{i}}" :j "{{i + 1}}"}}]}))
                                          (image :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
                                            :fit cover
                                            :width 230
                                            :height 100
                                            :cursor hand
                                            :onClick {:run openStore :with {:id "{{g.id}}"}})
                                          (column :grow 1 :height 100 :gap 4
                                            (label :font heading
                                              :text "{{g.title}}"
                                              :height 26
                                              :anchors {:left 0 :right 0}
                                              :textColor @color.bright
                                              :cursor hand
                                              :onClick {:run openStore :with {:id "{{g.id}}"}})
                                            (row :height 18 :anchors {:left 0 :right 0} :gap 6
                                              (label :text "{{upper(t('store.overall'))}}"
                                                :fitText true
                                                :font small
                                                :textColor @store.label)
                                              (label :text "{{t('review.' + g.review)}}"
                                                :fitText true
                                                :font small
                                                :textColor @store.chipText)
                                              (panel :color transparent :width 10 :height 1)
                                              (label :text "{{upper(t('game.release'))}}"
                                                :fitText true
                                                :font small
                                                :textColor @store.label)
                                              (label :text "{{g.release}}"
                                                :fitText true
                                                :font small
                                                :textColor @store.text))
                                            (row :height 24 :anchors {:left 0 :right 0} :gap 4
                                              (label :repeat {:for tag :in "{{slice(g.tags, 0, 4)}}"}
                                                :font small
                                                :text "{{tag}}"
                                                :fitText true
                                                :padding 7
                                                :radius 2
                                                :color @store.chip
                                                :textColor @store.chipText))
                                            (label :font small
                                              :text "{{t('store.added', { date: '12 Aug' })}}"
                                              :height 18
                                              :anchors {:left 0 :right 0}
                                              :textColor @color.dim))
                                          (column :width 290 :height 100 :gap 8
                                            (panel :color transparent :height 16 :width 1)
                                            (row :height 36 :anchors {:left 0 :right 0} :gap 4 :alignItems center
                                              (panel :color transparent :grow 1 :height 1)
                                              (node :use priceTag :with {:g "{{g}}"})
                                              (node :use ctaButton
                                                :with {:title "{{contains(state.cart, g.id) ? t('store.inCart') : t('store.addToCart')}}"}
                                                :onClick [{:if "{{contains(state.cart, g.id)}}"
                                                           :then [{:run showCart}]
                                                           :else [{:run addToCart :with {:id "{{g.id}}"}}]}]))
                                            (row :height 18 :anchors {:left 0 :right 0}
                                              (panel :color transparent :grow 1 :height 1)
                                              (label :text "{{t('store.remove')}}"
                                                :fitText true
                                                :font small
                                                :textColor @color.dim
                                                :cursor hand
                                                :role button
                                                :focusable true
                                                :hover {:textColor @color.bright}
                                                :onClick {:run toggleWishlist :with {:id "{{g.id}}"}})))
                                          (panel :color transparent :width 4 :height 1))))}
              :storePage {:note "The store: the top links and the store bar, then the front page, search results, a game's page, the cart or the wishlist."
                          :params {}
                          :node (node#store :fill true
                                  :color @color.page
                                  :gradient {:kind linear
                                             :stops [{:at 0 :color @store.glow} {:at 0.35 :color @color.page}]}
                                  (column#store-scroll :fill true :scroll vertical
                                    (column#store-content :height "{{computed.storeHeight}}"
                                      :anchors {:left "{{(window.width - computed.storeWidth) / 2}}"
                                                :right "{{(window.width - computed.storeWidth) / 2}}"}
                                      (node :use storeTop)
                                      (node :when "{{computed.storeView == 'home'}}" :use storeHome)
                                      (node :when "{{computed.storeView == 'results'}}" :use storeResults)
                                      (node :when "{{computed.storeView == 'app' && computed.game != null}}"
                                        :use storeApp
                                        :with {:g "{{computed.game}}"})
                                      (node :when "{{computed.storeView == 'cart'}}" :use storeCart)
                                      (node :when "{{computed.storeView == 'wishlist'}}" :use storeWishlist)))
                                  (node :use dialog
                                    :with {:open state.cartDialog
                                           :title "{{t('store.addedTitle')}}"
                                           :message "{{t('store.addedText', { title: find(data.games, it.id == state.cartAdded).title ?? '' })}}"
                                           :buttons [{:title "{{t('store.viewCart')}}" :run showCart}
                                                     {:title "{{t('store.keepShopping')}}"}]}))}}}
