{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Arcadia's friends and profile: the friends list grouped by presence with your own status, a chat per friend (its messages are records of models.json's messages, an invitation to play is a message with a game) and the profile page. Every friend and message is invented."
  :tokens {:chat.mine {:dark "#5a3f1c" :light "#f8dfb6"}
    :chat.onMine {:dark "#ffe7c2" :light "#3d2508"}
    :profile.bannerStart {:dark "#3b2346" :light "#eadcf2"}
    :profile.bannerEnd {:dark "#211a28" :light "#f6f2f7"}}
  :state {:chatWith f2
    :chatDraft ""
    :friendQuery ""
    :statusOpen false
    :typing nil
    :replyTo nil
    :read {}
    :addFriend {:open false :name ""}}
  :computed {:friendGroups "{{filter([{ key: 'ingame', friends: filter(data.friends, it.presence == 'ingame' && (empty(state.friendQuery) || fuzzy(state.friendQuery, it.name) != null)) }, { key: 'online', friends: filter(data.friends, (it.presence == 'online' || it.presence == 'away') && (empty(state.friendQuery) || fuzzy(state.friendQuery, it.name) != null)) }, { key: 'offline', friends: filter(data.friends, it.presence == 'offline' && (empty(state.friendQuery) || fuzzy(state.friendQuery, it.name) != null)) }], length(it.friends) > 0)}}"
    :chatFriend "{{find(data.friends, it.id == state.chatWith)}}"
    :chat "{{filter(data.messages, it.friend == state.chatWith)}}"
    :inviteGame "{{find(data.games, it.id == state.running) ?? sort(filter(computed.owned, it.installed && it.lastPlayed != null), it.lastPlayed, 'desc')[0]}}"
    :profileRecent "{{slice(sort(filter(computed.owned, it.lastPlayed != null), it.lastPlayed, 'desc'), 0, 4)}}"}
  :actions {:openChat {:title "Chat with a friend"
      :category Friends
      :icon message-circle
      :do [{:set state.chatWith :to "{{id}}"}
        {:set state.chatDraft :to ""}
        {:set state.read :to "{{merge(state.read, fromEntries([{ key: id, value: now() }]))}}"}
        {:run go :with {:tab community :page home :id nil}}
        {:scrollTo "#chat-end"}
        {:focus "#chat-input"}]}
    :sendMessage {:title "Send the message"
      :category Friends
      :icon send
      :enablement "{{computed.chatFriend != null && trim(state.chatDraft) != ''}}"
      :do [{:if "{{computed.chatFriend != null && trim(state.chatDraft) != ''}}"
          :then [{:insert messages
              :value {:friend "{{state.chatWith}}" :mine true :at "{{now()}}" :text "{{trim(state.chatDraft)}}"}}
            {:set state.chatDraft :to ""}
            {:after 50 :do [{:scrollTo "#chat-end"}]}
            {:run awaitReply}]}]}
    :awaitReply {:title "The friend answers"
      :category Friends
      :description "An online friend starts typing a moment later and answers; an offline one reads it later."
      :do [{:if "{{computed.chatFriend.presence != 'offline' && state.replyTo == null}}"
          :then [{:set state.replyTo :to "{{state.chatWith}}"}
            {:after 900 :do [{:set state.typing :to "{{state.replyTo}}"} {:scrollTo "#chat-end"}]}
            {:after 3200
              :do [{:insert messages
                  :value {:friend "{{state.replyTo}}"
                    :at "{{now()}}"
                    :text "{{t('chat.reply' + text(count(data.messages, it.mine) % 3))}}"}}
                {:set state.typing :to nil}
                {:set state.replyTo :to nil}
                {:scrollTo "#chat-end"}]}]}]}
    :inviteToPlay {:title "Invite to play"
      :category Friends
      :icon gamepad-2
      :enablement "{{computed.chatFriend != null && computed.inviteGame != null}}"
      :do [{:if "{{computed.chatFriend != null && computed.inviteGame != null}}"
          :then [{:insert messages
              :value {:friend "{{state.chatWith}}" :mine true :at "{{now()}}" :game "{{computed.inviteGame.id}}"}}
            {:after 50 :do [{:scrollTo "#chat-end"}]}]}]}
    :setStatus {:title "Set your status" :category Friends :do [{:set state.user.status :to "{{status}}"}]}
    :addFriend {:title "Add a friend"
      :category Friends
      :icon user-plus
      :description "Sends a friend request by name; they show as offline until they answer."
      :do [{:if "{{trim(state.addFriend.name) != ''}}"
          :then [{:insert friends
              :value {:name "{{trim(state.addFriend.name)}}" :avatar ghost :presence offline :since 0}}
            {:set state.addFriend :to {:open false :name ""}}]}]}
    :addFriendDialog {:title "Add a friend"
      :category Friends
      :icon user-plus
      :do [{:run go :with {:tab community :page home :id nil}} {:set state.addFriend :to {:open true :name ""}}]}
    :showProfileOf {:title "Your profile" :category Friends :do [{:run go :with {:tab profile :page home :id nil}}]}}
  :components {:avatar {:note "A person's avatar with a frame in their presence's colour: in a game lime, online lilac, offline grey."
      :params {:avatar fox :presence online :size 36}
      :node {:let {:p "{{presence}}"}
        :type panel
        :width "{{size}}"
        :height "{{size}}"
        :radius 4
        :color "{{p == 'ingame' ? '@color.ingame' : p == 'offline' || p == 'invisible' ? '@color.offline' : '@color.online'}}"
        :children [{:type image :file "{{'../assets/art/avatars/' + avatar + '.svg'}}" :fill 2 :fit cover}]}}
    :friendRow {:note "A friend in the list: avatar, name in their presence's colour, what they are doing; opens the chat."
      :params {:friend {}}
      :node (node :let {:p "{{friend.presence}}" :here "{{state.chatWith == friend.id}}"}
        #"{{'friend-' + friend.id}}"
        :type row
        :height 52
        :anchors {:left 0 :right 0}
        :gap 10
        :alignItems center
        :focusable true
        :role listitem
        :cursor hand
        :label "{{friend.name}}"
        :selected "{{here}}"
        :color "{{here ? '@color.select' : 'transparent'}}"
        :hover {:color @color.hover}
        :onClick {:run openChat :with (node #"{{friend.id}}")}
        {:type panel :color transparent :width 14 :height 1}
        {:use avatar :with {:avatar "{{friend.avatar}}" :presence "{{p}}" :size 34}}
        {:type column
          :grow 1
          :height 36
          :gap 2
          :alignSelf center
          :children [{:type label
              :font strong
              :text "{{friend.name}}"
              :textColor "{{p == 'ingame' ? '@color.ingame' : p == 'offline' || p == 'invisible' ? '@color.offline' : '@color.online'}}"
              :anchors {:left 0 :right 0}
              :height 18}
            {:type label
              :font small
              :anchors {:left 0 :right 0}
              :height 16
              :textColor "{{p == 'ingame' ? '@color.ingame' : '@color.muted'}}"
              :text "{{p == 'ingame' ? find(data.games, it.id == friend.game).title ?? t('presence.ingame') : p == 'offline' && friend.since > 0 ? t('friends.lastOnline', { n: friend.since }) : t('presence.' + p)}}"}]}
        {:when "{{count(data.messages, it.friend == friend.id && !it.mine && it.at > (state.read[friend.id] ?? 0)) > 0 && !here}}"
          :type panel
          :width 8
          :height 8
          :radius 4
          :color @color.accent
          :label "{{t('friends.unread')}}"}
        {:type panel :color transparent :width 14 :height 1})}
    :friendsList {:note "The left column: your own card with its status menu, a search, and the friends grouped by presence."
      :params {}
      :node (node #friends-side :type column :width 300 :anchors {:top 0 :bottom 0} :color @color.side
        (node #me-card :type row :height 76 :anchors {:left 0 :right 0} :gap 12 :alignItems center :color @color.sideTop
          {:type panel :color transparent :width 14 :height 1}
          {:use avatar :with {:avatar "{{state.user.avatar}}" :presence "{{state.user.status}}" :size 46}}
          {:type column
            :grow 1
            :height 42
            :gap 2
            :alignSelf center
            :children [{:type label
                :font heading
                :text "{{state.user.name}}"
                :textColor @color.online
                :anchors {:left 0 :right 0}
                :height 22}
              (node #my-status
                :type label
                :font small
                :fitText true
                :padding 2
                :height 18
                :cursor hand
                :focusable true
                :role button
                :runs [{:text "{{t('presence.' + state.user.status) + ' '}}"} {:icon chevron-down}]
                :textColor @color.muted
                :label "{{t('friends.status')}}"
                :hover {:textColor @color.bright}
                :isOpen "{{state.statusOpen}}"
                :onClick {:toggle state.statusOpen}
                {:use menu
                  :with {:open state.statusOpen
                    :width 180
                    :items "{{map(['online', 'away', 'invisible', 'offline'], { title: t('presence.' + it), checked: state.user.status == it, run: 'setStatus', with: { status: it } })}}"}})]}
          (node #add-friend
            :style iconButton
            :font heading
            :icon user-plus
            :tooltip "{{t('friends.add')}}"
            :label "{{t('friends.add')}}"
            :onClick {:run addFriendDialog})
          {:type panel :color transparent :width 10 :height 1})
        {:type row
          :height 48
          :anchors {:left 12 :right 12}
          :alignItems center
          :children [(node #friends-search
              :type textarea
              :grow 1
              :height 28
              :padding 8
              :bind state.friendQuery
              :placeholder "{{t('friends.search')}}"
              :placeholderColor @color.dim
              :textColor @color.text
              :color @color.field
              :radius 3
              :label "{{t('friends.search')}}"
              :focused {:borderWidth 1 :borderColor @color.accent}
              {:type label :icon search :text "" :textColor @color.dim :fitText true :anchors {:right 8 :top 6}})]}
        (node #friends-scroll
          :type column
          :grow 1
          :anchors {:left 0 :right 0}
          :scroll vertical
          :role list
          :label "{{t('nav.community')}}"
          (node :repeat {:for group :in "{{computed.friendGroups}}" :key "{{group.key}}"}
            #"{{'friends-group-' + group.key}}"
            :type column
            :anchors {:left 0 :right 0}
            :height "{{34 + 52 * length(group.friends)}}"
            {:type label
              :style caps
              :height 34
              :anchors {:left 14 :right 14}
              :align {:vertical center}
              :role heading
              :text "{{t('friends.group.' + group.key, { n: length(group.friends) })}}"}
            {:repeat {:for f :in "{{group.friends}}" :key "{{f.id}}"} :use friendRow :with {:friend "{{f}}"}})
          {:when "{{sum(computed.friendGroups, length(it.friends)) == 0}}"
            :type label
            :height 60
            :anchors {:left 14 :right 14}
            :wrap true
            :textColor @color.muted
            :text "{{t('friends.noMatch', { query: state.friendQuery })}}"}))}
    :chatMessage {:note "One message: a bubble on the right for yours, on the left with the friend's avatar for theirs; an invitation to play is a card with the game's cover and Join."
      :params {:m {} :friend {}}
      :node {:let {:g "{{m.game == null ? null : find(data.games, it.id == m.game)}}"}
        :type row
        :anchors {:left 0 :right 0}
        :gap 8
        :fitContent height
        :justify "{{m.mine ? 'end' : 'start'}}"
        :alignItems end
        :role listitem
        :label "{{(m.mine ? state.user.name : friend.name) + ': ' + (g == null ? m.text : t('chat.invite', { game: g.title }))}}"
        :children [{:when "{{!m.mine}}"
            :type image
            :file "{{'../assets/art/avatars/' + friend.avatar + '.svg'}}"
            :width 28
            :height 28
            :fit cover
            :radius 14}
          {:type column
            :fitContent true
            :gap 3
            :alignItems "{{m.mine ? 'end' : 'start'}}"
            :children [{:when "{{g == null}}"
                :type label
                :text "{{m.text}}"
                :wrap true
                :fitText {:maxWidth 440}
                :padding 10
                :radius 12
                :selectable true
                :color "{{m.mine ? '@chat.mine' : '@color.raised'}}"
                :textColor "{{m.mine ? '@chat.onMine' : '@color.text'}}"}
              {:when "{{g != null}}"
                :type row
                :width 320
                :height 96
                :gap 12
                :color @color.panel
                :radius 8
                :borderWidth 1
                :borderColor @color.border
                :alignItems center
                :children [{:type panel :color transparent :width 2 :height 1}
                  {:type image
                    :file "{{'../assets/art/' + g.id + '/cover.svg'}}"
                    :width 48
                    :height 72
                    :fit cover
                    :radius 3}
                  {:type column
                    :grow 1
                    :height 64
                    :gap 4
                    :alignSelf center
                    :children [{:type label
                        :style caps
                        :height 16
                        :anchors {:left 0 :right 0}
                        :text "{{t(m.mine ? 'chat.invited' : 'chat.invites')}}"}
                      {:type label
                        :font strong
                        :height 18
                        :anchors {:left 0 :right 0}
                        :text "{{g.title}}"
                        :textColor @color.bright}
                      (node :when "{{!m.mine}}"
                        #"{{'join-' + m.id}}"
                        :type label
                        :font strong
                        :fitText true
                        :padding 6
                        :height 26
                        :radius 3
                        :focusable true
                        :role button
                        :cursor hand
                        :text "{{t(g.installed ? 'chat.join' : 'chat.install')}}"
                        :gradient {:kind linear
                          :angle 90
                          :stops ["{{g.installed ? '@color.play' : '@color.warm'}}"
                            "{{g.installed ? '@color.playEnd' : '@color.warmEnd'}}"]}
                        :textColor "#ffffff"
                        :onClick {:run primary :with {:game "{{g}}"}})]}]}
              {:type label
                :font small
                :fitText true
                :textColor @color.dim
                :text "{{now() - m.at < 60000 ? t('chat.now') : formatTime(m.at, 'short', 'utc')}}"}]}]}}
    :chatPane {:note "The chat with the friend chosen on the left: their name and presence, the messages with a line for each day, the friend typing, and a box to write in (Enter sends)."
      :params {}
      :node (node #chat :grow 1 :anchors {:top 0 :bottom 0} :color @color.main
        {:when "{{computed.chatFriend == null}}"
          :type column
          :fill true
          :justify center
          :alignItems center
          :gap 8
          :children [{:type label :font button :icon message-circle :text "" :fitText true :textColor @color.dim}
            (node #chat-empty :type label :fitText true :textColor @color.muted :text "{{t('chat.choose')}}")]}
        {:when "{{computed.chatFriend != null}}"
          :let {:friend "{{computed.chatFriend}}"}
          :type column
          :fill true
          :children [(node #chat-header
              :type row
              :height 68
              :anchors {:left 0 :right 0}
              :gap 12
              :alignItems center
              :color @color.panel
              {:type panel :color transparent :width 18 :height 1}
              {:use avatar :with {:avatar "{{friend.avatar}}" :presence "{{friend.presence}}" :size 40}}
              {:type column
                :grow 1
                :height 40
                :gap 2
                :alignSelf center
                :children [(node #chat-name
                    :type label
                    :font heading
                    :text "{{friend.name}}"
                    :textColor @color.bright
                    :anchors {:left 0 :right 0}
                    :height 22)
                  {:type label
                    :font small
                    :anchors {:left 0 :right 0}
                    :height 16
                    :textColor @color.muted
                    :text "{{friend.presence == 'ingame' ? t('friends.playing', { game: find(data.games, it.id == friend.game).title }) : t('presence.' + friend.presence)}}"}]}
              (node #chat-invite
                :type label
                :font strong
                :fitText true
                :padding 9
                :height 34
                :radius 3
                :icon gamepad-2
                :focusable true
                :role button
                :cursor hand
                :color @color.raised
                :textColor @color.text
                :hover {:color @color.hover :textColor @color.bright}
                :disabled "{{computed.inviteGame == null}}"
                :tooltip "{{computed.inviteGame == null ? t('chat.nothingToInvite') : t('chat.inviteTo', { game: computed.inviteGame.title })}}"
                :text "{{t('chat.inviteButton')}}"
                :onClick {:run inviteToPlay})
              {:type panel :color transparent :width 18 :height 1})
            {:type panel :height 1 :anchors {:left 0 :right 0} :color @color.border}
            {:grow 1
              :height 0
              :alignSelf stretch
              :color transparent
              :children [(node #chat-messages
                  :type column
                  :fill true
                  :scroll vertical
                  :gap 10
                  :role list
                  :label "{{t('chat.messages', { name: friend.name })}}"
                  :onShow [{:scrollTo "#chat-end"}]
                  {:type panel :color transparent :width 1 :height 8}
                  {:repeat {:for m :in "{{computed.chat}}" :index i :key "{{m.id}}"}
                    :type column
                    :anchors {:left 24 :right 24}
                    :fitContent height
                    :gap 10
                    :children [{:when "{{i == 0 || floor(computed.chat[i - 1].at / 86400000) != floor(m.at / 86400000)}}"
                        :type row
                        :height 20
                        :anchors {:left 0 :right 0}
                        :gap 12
                        :alignItems center
                        :children [{:type panel :grow 1 :height 1 :color @color.border}
                          {:type label
                            :style caps
                            :fitText true
                            :text "{{m.at >= state.today ? t('time.today') : m.at >= state.today - 86400000 ? t('time.yesterday') : formatDate(m.at, 'medium', 'utc')}}"}
                          {:type panel :grow 1 :height 1 :color @color.border}]}
                      {:use chatMessage :with {:m "{{m}}" :friend "{{friend}}"}}]}
                  (node :when "{{state.typing == friend.id}}"
                    #chat-typing
                    :type label
                    :font small
                    :height 18
                    :anchors {:left 24 :right 24}
                    :role status
                    :live polite
                    :textColor @color.muted
                    :text "{{t('chat.typing', { name: friend.name })}}")
                  (node #chat-end :type panel :color transparent :height 8 :anchors {:left 0 :right 0}))]}
            {:type row
              :height 64
              :anchors {:left 0 :right 0}
              :gap 10
              :alignItems center
              :color @color.panel
              :children [{:type panel :color transparent :width 18 :height 1}
                (node #chat-input
                  :type textarea
                  :grow 1
                  :height 36
                  :padding 10
                  :bind state.chatDraft
                  :radius 3
                  :color @color.field
                  :borderWidth 1
                  :borderColor @color.border
                  :placeholder "{{t('chat.placeholder', { name: friend.name })}}"
                  :placeholderColor @color.dim
                  :textColor @color.text
                  :label "{{t('chat.placeholder', { name: friend.name })}}"
                  :focused {:borderColor @color.accent}
                  :onSubmit {:run sendMessage})
                (node #chat-send
                  :type label
                  :font heading
                  :icon send
                  :text ""
                  :width 40
                  :height 36
                  :radius 3
                  :align center
                  :focusable true
                  :role button
                  :cursor hand
                  :label "{{t('chat.send')}}"
                  :tooltip "{{t('chat.send')}}"
                  :color "{{trim(state.chatDraft) == '' ? '@color.raised' : '@color.accent'}}"
                  :textColor "{{trim(state.chatDraft) == '' ? '@color.dim' : '#ffffff'}}"
                  :onClick {:run sendMessage})
                {:type panel :color transparent :width 84 :height 1}]}]})}
    :addFriendDialog {:note "Add a friend by name: the request goes out and they show as offline until they accept."
      :params {}
      :node {:use dialog
        :with {:open state.addFriend.open
          :title "{{t('friends.add')}}"
          :height 180
          :message "{{t('friends.addHelp', { code: state.user.code })}}"
          :buttons [{:title "{{t('friends.sendRequest')}}" :run addFriend :default true}
            {:title "{{t('dialog.cancel')}}"}]}
        :children [(node #add-friend-name
            :type textarea
            :height 32
            :padding 8
            :anchors {:left 20 :right 20 :top 80}
            :borderWidth 1
            :borderColor @color.border
            :focused {:borderColor @color.accent}
            :bind state.addFriend.name
            :radius 3
            :color @color.field
            :textColor @color.text
            :placeholderColor @color.dim
            :placeholder "{{t('friends.addPlaceholder')}}"
            :label "{{t('friends.addPlaceholder')}}"
            :autofocus true
            :onSubmit {:run addFriend})]}}
    :friendsPage {:note "Friends: the list on the left, the chat on the right."
      :params {}
      :node (node #friends-page :type row :fill true :color @color.page
        {:use friendsList}
        {:type panel :width 1 :anchors {:top 0 :bottom 0} :color @color.border}
        {:use chatPane}
        {:use addFriendDialog})}
    :profileStat {:note "A number with its name, on the profile."
      :params (node :value "" :title "" #"")
      :node (node #"{{id}}" :type column :width 120 :height 56 :gap 2
        {:type label :font nav :text "{{value}}" :textColor @color.bright :anchors {:left 0 :right 0} :height 26}
        {:type label :font small :text "{{title}}" :textColor @color.muted :anchors {:left 0 :right 0} :height 16})}
    :profilePage {:note "The profile: the banner with the avatar, level and status; numbers; a showcase of favourites; the games played lately with their hours and achievements; and friends."
      :params {}
      :node (node #profile-page :fill true :color @color.page
        (node #profile-scroll :type column :fill true :scroll vertical
          (node #profile-banner
            :type row
            :height 196
            :anchors {:left 0 :right 0}
            :gap 24
            :alignItems center
            :gradient {:kind linear :angle 0 :stops [@profile.bannerStart @profile.bannerEnd]}
            {:type panel :color transparent :width 16 :height 1}
            {:use avatar :with {:avatar "{{state.user.avatar}}" :presence "{{state.user.status}}" :size 132}}
            {:type column
              :grow 1
              :height 84
              :gap 6
              :alignSelf center
              :children [(node #profile-name
                  :type label
                  :font logo
                  :text "{{state.user.name}}"
                  :textColor @color.bright
                  :anchors {:left 0 :right 0}
                  :height 58
                  :role heading)
                {:type label
                  :anchors {:left 0 :right 0}
                  :height 20
                  :textColor @color.muted
                  :text "{{t('profile.since', { year: text(state.user.since) })}} · {{t('presence.' + state.user.status)}}"}]}
            (node #profile-level :type column :width 140 :height 120 :gap 6 :alignItems center :alignSelf center
              {:type label :font small :text "{{t('profile.level')}}" :textColor @color.muted :fitText true}
              {:type label
                :font nav
                :text "{{state.user.level}}"
                :width 56
                :height 56
                :radius 28
                :align center
                :borderWidth 3
                :borderColor @color.accent
                :textColor @color.bright
                :label "{{t('profile.levelIs', { n: state.user.level })}}"})
            {:type panel :color transparent :width 32 :height 1})
          (node #profile-stats :type row :height 88 :anchors {:left 40 :right 40} :gap 24 :alignItems center
            {:use profileStat
              :with (node #stat-games :value "{{length(computed.owned)}}" :title "{{t('profile.games')}}")}
            {:use profileStat
              :with (node #stat-hours
                :value "{{formatNumber(sum(computed.owned, it.hours), 0, 0)}}"
                :title "{{t('profile.hours')}}")}
            {:use profileStat
              :with (node #stat-achievements
                :value "{{sum(computed.owned, it.achievements[0] ?? 0)}}"
                :title "{{t('profile.achievements')}}")}
            {:use profileStat
              :with (node #stat-friends :value "{{length(data.friends)}}" :title "{{t('profile.friends')}}")})
          {:type panel :height 1 :anchors {:left 40 :right 40} :color @color.border}
          {:type row
            :anchors {:left 40 :right 40}
            :gap 32
            :height "{{max(560, 120 + 96 * length(computed.profileRecent))}}"
            :children [{:type column
                :grow 1
                :gap 12
                :alignSelf stretch
                :children [{:type panel :color transparent :width 1 :height 12}
                  {:type label
                    :style caps
                    :height 18
                    :anchors {:left 0 :right 0}
                    :role heading
                    :text "{{t('profile.showcase')}}"}
                  (node #profile-showcase :type row :height 180 :gap 14 :anchors {:left 0 :right 0}
                    {:repeat {:for g :in "{{slice(filter(computed.owned, it.favourite), 0, 5)}}" :key "{{g.id}}"}
                      :use capsule
                      :with {:game "{{g}}" :width 116}})
                  {:type label
                    :style caps
                    :height 18
                    :anchors {:left 0 :right 0}
                    :role heading
                    :text "{{t('profile.recent')}}"}
                  (node :repeat {:for g :in "{{computed.profileRecent}}" :key "{{g.id}}"}
                    #"{{'profile-recent-' + g.id}}"
                    :type row
                    :height 84
                    :anchors {:left 0 :right 0}
                    :style card
                    :gap 14
                    :alignItems center
                    :focusable true
                    :role link
                    :cursor hand
                    :label "{{g.title}}"
                    :hover {:color @color.raised}
                    :onClick {:run openGame :with (node #"{{g.id}}")}
                    {:type panel :color transparent :width 4 :height 1}
                    {:type image
                      :file "{{'../assets/art/' + g.id + '/hero.svg'}}"
                      :width 150
                      :height 64
                      :fit cover
                      :radius 3}
                    {:type column
                      :grow 1
                      :height 40
                      :gap 4
                      :alignSelf center
                      :children [{:type label
                          :font strong
                          :text "{{g.title}}"
                          :textColor @color.bright
                          :anchors {:left 0 :right 0}
                          :height 18}
                        {:type label
                          :font small
                          :anchors {:left 0 :right 0}
                          :height 16
                          :textColor @color.muted
                          :text "{{t('game.hoursTotal', { n: g.hours })}}"}]}
                    {:when "{{g.achievements != null}}"
                      :type column
                      :width 180
                      :height 28
                      :gap 6
                      :alignSelf center
                      :children [{:type label
                          :font small
                          :anchors {:left 0 :right 0}
                          :height 16
                          :textColor @color.muted
                          :text "{{t('profile.achieved', { got: g.achievements[0], total: g.achievements[1] })}}"}
                        {:height 6
                          :radius 3
                          :anchors {:left 0 :right 0}
                          :color @color.raised
                          :children [{:anchors {:left 0 :top 0 :bottom 0}
                              :radius 3
                              :width "{{text(floor(100 * g.achievements[0] / max(1, g.achievements[1]))) + '%'}}"
                              :gradient {:kind linear :angle 90 :stops [@color.warm @color.accent]}}]}]}
                    {:type panel :color transparent :width 16 :height 1})]}
              (node #profile-friends :type column :width 260 :gap 8 :alignSelf stretch
                {:type panel :color transparent :width 1 :height 16}
                {:type label
                  :style caps
                  :height 18
                  :anchors {:left 0 :right 0}
                  :role heading
                  :text "{{t('profile.friendsCount', { n: length(data.friends) })}}"}
                {:repeat {:for f
                    :in "{{slice(sort(data.friends, it.presence == 'offline' ? 1 : 0), 0, 8)}}"
                    :key "{{f.id}}"}
                  :type row
                  :height 40
                  :anchors {:left 0 :right 0}
                  :gap 10
                  :alignItems center
                  :focusable true
                  :role link
                  :cursor hand
                  :label "{{f.name}}"
                  :hover {:color @color.raised}
                  :onClick {:run openChat :with (node #"{{f.id}}")}
                  :children [{:use avatar :with {:avatar "{{f.avatar}}" :presence "{{f.presence}}" :size 30}}
                    {:let {:p "{{f.presence}}"}
                      :type label
                      :grow 1
                      :height 18
                      :text "{{f.name}}"
                      :textColor "{{p == 'ingame' ? '@color.ingame' : p == 'offline' || p == 'invisible' ? '@color.offline' : '@color.online'}}"}]})]}))}}}
