{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "A game's player hub: what players post about it, as discussions and guides, opened from the links under the game's banner. The posts are invented, the same for every game, with its name in them."
  :state {:hubTab hub
    :hubPosts [{:kind discussions :key hub.post.firstHours :by 0 :replies 48 :votes 112}
      {:kind guides :key hub.post.allEndings :by 3 :replies 17 :votes 640}
      {:kind discussions :key hub.post.controller :by 1 :replies 9 :votes 21}
      {:kind guides :key hub.post.beginner :by 5 :replies 31 :votes 288}
      {:kind discussions :key hub.post.coop :by 2 :replies 73 :votes 96}
      {:kind guides :key hub.post.secrets :by 4 :replies 12 :votes 154}]}
  :actions {:openHub {:title "Open the game's player hub"
      :category Game
      :description "section is hub (everything), discussions or guides."
      :do [{:set state.hubTab :to "{{section ?? 'hub'}}"} {:run go :with (node :tab library :page hub #"{{id}}")}]}}
  :components {:hubPage {:note "The hub of the game on the page: its name, the three sections and the posts of the one chosen."
      :params {}
      :node {:type column
        :fill true
        :scroll vertical
        :let {:g "{{computed.game}}"}
        :children [{:type row
            :height 84
            :anchors {:left 32 :right 32}
            :gap 16
            :alignItems center
            :children [{:type image
                :file "{{'../assets/art/' + (g.art ?? g.id) + '/cover.svg'}}"
                :width 40
                :height 60
                :radius 6
                :fit cover}
              {:type column
                :grow 1
                :height 48
                :children [{:type label
                    :font heading
                    :height 26
                    :anchors {:left 0 :right 0}
                    :text "{{g.title}}"
                    :textColor @color.bright}
                  {:type label
                    :font small
                    :height 18
                    :anchors {:left 0 :right 0}
                    :text "{{t('hub.subtitle')}}"
                    :textColor @color.muted}]}
              (node #hub-back
                :style sectionLink
                :icon arrow-left
                :text "{{t('hub.back')}}"
                :onClick {:run openGame :with (node #"{{g.id}}")})]}
          (node #hub-tabs
            :type row
            :height 40
            :anchors {:left 32 :right 32}
            :gap 6
            :alignItems center
            :role tablist
            :arrows horizontal
            (node :repeat {:for s :in "{{['hub', 'discussions', 'guides']}}"}
              #"{{'hub-tab-' + s}}"
              :style pillButton
              :role tab
              :selected "{{state.hubTab == s}}"
              :color "{{state.hubTab == s ? '@color.select' : 'transparent'}}"
              :textColor "{{state.hubTab == s ? '@color.bright' : '@color.text'}}"
              :text "{{t(s == 'hub' ? 'hub.all' : 'game.section.' + s)}}"
              :onClick {:set state.hubTab :to "{{s}}"}))
          (node #hub-posts
            :type column
            :anchors {:left 32 :right 32}
            :height "{{8 + count(state.hubPosts, state.hubTab == 'hub' || it.kind == state.hubTab) * 76}}"
            :gap 8
            :role list
            :label "{{t('hub.subtitle')}}"
            {:repeat {:for p :in "{{filter(state.hubPosts, state.hubTab == 'hub' || it.kind == state.hubTab)}}"}
              :let {:who "{{data.friends[p.by % length(data.friends)]}}"}
              :type row
              :role listitem
              :anchors {:left 0 :right 0}
              :height 68
              :gap 12
              :alignItems center
              :color @color.panel
              :radius 8
              :children [{:type panel :width 2 :height 1 :color transparent}
                {:type label
                  :icon "{{p.kind == 'guides' ? 'book-open' : 'messages-square'}}"
                  :text ""
                  :fitText true
                  :textColor @color.accent}
                {:type column
                  :grow 1
                  :height 40
                  :children [{:type label
                      :font strong
                      :height 22
                      :anchors {:left 0 :right 0}
                      :text "{{t(p.key, { game: g.title })}}"
                      :textColor @color.text}
                    {:type label
                      :font small
                      :height 18
                      :anchors {:left 0 :right 0}
                      :text "{{t('hub.by', { who: who.name })}}"
                      :textColor @color.muted}]}
                {:type label
                  :font small
                  :fitText true
                  :icon thumbs-up
                  :text "{{formatNumber(p.votes)}}"
                  :textColor @color.muted}
                {:type label
                  :font small
                  :fitText true
                  :icon message-circle
                  :text "{{formatNumber(p.replies)}}"
                  :textColor @color.muted}
                {:type panel :width 2 :height 1 :color transparent}]})]}}}}
