;;; friends.lisp --- Arcadia's friends: the list grouped by presence, your own status, and a chat per friend
;;;
;;; A chat's messages are records of models.lisp's messages; an invitation to play is a message with a game. Every
;;; friend and message is invented. The profile page is in profile.lisp.

;;; What the friends page keeps

(defstate chatWith f2)

(defstate chatDraft "")

(defstate friendQuery "")

(defstate statusOpen false)

(defstate typing nil)

(defstate replyTo nil)

(defstate read {})

(defstate addFriend {:open false :name ""})

(defmacro friends-where
  ;; A macro, not a function: the list it writes stays an expression, worked out at its speed.
  "The friends whose presence passes `test` (of `it`), and whose name matches the search."
  [test]
  `(filter (fn [it] (and ~test (or (empty? state.friendQuery) (some? (fuzzy state.friendQuery it.name)))))
           data.friends))

(defcomputed friendGroups "The friends in a game, online (or away), and offline, each group only if it has someone."
  (filter (fn [it] (> (count it.friends) 0))
          [{:key "ingame" :friends (friends-where (= it.presence "ingame"))}
           {:key "online" :friends (friends-where (or (= it.presence "online") (= it.presence "away")))}
           {:key "offline" :friends (friends-where (= it.presence "offline"))}]))

(defcomputed chatFriend "The friend whose chat is open." (find (fn [it] (= it.id state.chatWith)) data.friends))

(defcomputed chat "The messages with that friend." (filter (fn [it] (= it.friend state.chatWith)) data.messages))

(defcomputed inviteGame "The game an invitation is to: the one running, else the installed one played last."
  (or-else (find (fn [it] (= it.id state.running)) data.games)
           (get (sort-by (fn [it] it.lastPlayed)
                         >
                         (filter (fn [it] (and it.installed (some? it.lastPlayed))) computed.owned))
                0)))

(deftokens chat-colours "The colours of your own chat bubbles."
  {:chat.mine {:dark "#5a3f1c" :light "#f8dfb6"} :chat.onMine {:dark "#ffe7c2" :light "#3d2508"}})

;;; A person

(defmacro presence-colour
  ;; A macro: the colour stays an expression of the presence it is given.
  "The colour of a presence: in a game lime, offline (or invisible) grey, else lilac."
  [p]
  `(cond (= ~p "ingame") "@color.ingame"
         (or (= ~p "offline") (= ~p "invisible")) "@color.offline"
         :else "@color.online"))

(defcomponent avatar
  "A person's avatar with a frame in their presence's colour: in a game lime, online lilac, offline grey."
  {:avatar fox :presence online :size 36}
  `(panel :let {:p ~presence} :width ~size :height ~size :radius 4 :color ~(presence-colour p)
     (image :file ~(str "../assets/art/avatars/" avatar ".svg") :fill 2 :fit cover)))

(defcomponent friendRow
  "A friend in the list: avatar, name in their presence's colour, what they are doing; opens the chat."
  {:friend {}}
  `(row :id ~(str "friend-" friend.id)
     :let {:p ~friend.presence :here ~(= state.chatWith friend.id)}
     :height 52
     :anchors {:left 0 :right 0}
     :gap 10
     :alignItems center
     :focusable true
     :role listitem
     :cursor hand
     :label ~friend.name
     :selected ~here
     :color ~(if here "@color.select" "transparent")
     :hover {:color @color.hover}
     :onClick ~(handler openChat friend.id)
     (panel :color transparent :width 14 :height 1)
     (node :use avatar :with {:avatar ~friend.avatar :presence ~p :size 34})
     (column :grow 1 :height 36 :gap 2 :alignSelf center
       (label :font strong :text ~friend.name :textColor ~(presence-colour p) :anchors {:left 0 :right 0} :height 18)
       (label :font small
         :anchors {:left 0 :right 0}
         :height 16
         :textColor ~(if (= p "ingame") "@color.ingame" "@color.muted")
         :text ~(cond (= p "ingame") (or-else (:title (find (fn [it] (= it.id friend.game)) data.games)) (t "presence.ingame"))
                      (and (= p "offline") (> friend.since 0)) (t "friends.lastOnline" {:n friend.since})
                      :else (t (str "presence." p)))))
     ~(unread-dot)
     (panel :color transparent :width 14 :height 1)))

(defview unread-dot "A dot on a friend's row while they have written since their chat was last open." []
  `(panel :when ~(and (> (count (filter (fn [it]
                                          (and (= it.friend friend.id)
                                               (not it.mine)
                                               (> it.at (or-else (get state.read friend.id) 0))))
                                        data.messages))
                         0)
                      (not here))
     :width 8
     :height 8
     :radius 4
     :color @color.accent
     :label ~(t "friends.unread")))

;;; The friends page

(defcomponent friendsPage "Friends: the list on the left, the chat on the right."
  {}
  `(row#friends-page :fill true :color @color.page
     (node :use friendsList)
     (panel :width 1 :anchors {:top 0 :bottom 0} :color @color.border)
     (node :use chatPane)
     (node :use addFriendDialog)))

(defcomponent friendsList
  "The left column: your own card with its status menu, a search, and the friends grouped by presence."
  {}
  `(column#friends-side :width 300 :anchors {:top 0 :bottom 0} :color @color.side
     ~(me-card)
     (row :height 48 :anchors {:left 12 :right 12} :alignItems center
       (textarea#friends-search :grow 1
         :height 28
         :padding 8
         :bind state.friendQuery
         :placeholder ~(t "friends.search")
         :placeholderColor @color.dim
         :textColor @color.text
         :color @color.field
         :radius 3
         :label ~(t "friends.search")
         :focused {:borderWidth 1 :borderColor @color.accent}
         (label :icon search :text "" :textColor @color.dim :fitText true :anchors {:right 8 :top 6})))
     ~(friend-groups)))

(defview me-card "Your own card: the avatar, the name, your status with its menu, and Add a friend." []
  `(row#me-card :height 76 :anchors {:left 0 :right 0} :gap 12 :alignItems center :color @color.sideTop
     (panel :color transparent :width 14 :height 1)
     (node :use avatar :with {:avatar ~state.user.avatar :presence ~state.user.status :size 46})
     (column :grow 1 :height 42 :gap 2 :alignSelf center
       (label :font heading :text ~state.user.name :textColor @color.online :anchors {:left 0 :right 0} :height 22)
       ~(my-status))
     (node#add-friend :style iconButton
       :font heading
       :icon user-plus
       :tooltip ~(t "friends.add")
       :label ~(t "friends.add")
       :onClick ~(handler addFriendDialog))
     (panel :color transparent :width 10 :height 1)))

(defview my-status "Your status under your name, and the menu that changes it." []
  `(label#my-status :font small :fitText true :padding 2 :height 18 :cursor hand :focusable true :role button
     ;; (+ ...), not (str ...): text ending in a space, written around an expression, loses the space
     :runs
     [{:text ~(+ (t (str "presence." state.user.status)) " ")} {:icon chevron-down}]
     :textColor
     @color.muted
     :label
     ~(t "friends.status")
     :hover
     {:textColor @color.bright}
     :isOpen
     ~state.statusOpen
     :onClick
     {:toggle state.statusOpen}
     (node :use menu
       :with {:open state.statusOpen
              :width 180
              :items ~(map (fn [it]
                             {:title (t (str "presence." it))
                              :checked (= state.user.status it)
                              :run "setStatus"
                              :with {:status it}})
                           ["online" "away" "invisible" "offline"])})))

(defview friend-groups "The friends, a caps heading over each group; a line when the search matches no one." []
  `(column#friends-scroll :grow 1
     :anchors {:left 0 :right 0}
     :scroll vertical
     :role list
     :label ~(t "nav.community")
     ~(repeat [group computed.friendGroups :key group.key]
        `(column :id ~(str "friends-group-" group.key)
           :anchors {:left 0 :right 0}
           :height ~(+ 34 (* 52 (count group.friends)))
           (label :style caps
             :height 34
             :anchors {:left 14 :right 14}
             :align {:vertical center}
             :role heading
             :text ~(t (str "friends.group." group.key) {:n (count group.friends)}))
           ~(repeat [f group.friends :key f.id] `(node :use friendRow :with {:friend ~f}))))
     (label :when ~(= (sum computed.friendGroups (fn [it] (count it.friends))) 0)
       :height 60
       :anchors {:left 14 :right 14}
       :wrap true
       :textColor @color.muted
       :text ~(t "friends.noMatch" {:query state.friendQuery}))))

(defcomponent addFriendDialog
  "Add a friend by name: the request goes out and they show as offline until they accept."
  {}
  `(node :use dialog
     :with {:open state.addFriend.open
            :title ~(t "friends.add")
            :height 180
            :message ~(t "friends.addHelp" {:code state.user.code})
            :buttons [{:title ~(t "friends.sendRequest") :run addFriend :default true}
                      {:title ~(t "dialog.cancel")}]}
     (textarea#add-friend-name :height 32
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
       :placeholder ~(t "friends.addPlaceholder")
       :label ~(t "friends.addPlaceholder")
       :autofocus true
       :onSubmit ~(handler addFriend))))

;;; The chat

(defcomponent chatPane "The chat with the friend chosen on the left, or a word to choose one."
  {}
  `(node#chat :grow 1 :anchors {:top 0 :bottom 0} :color @color.main
     (column :when ~(nil? computed.chatFriend) :fill true :justify center :alignItems center :gap 8
       (label :font button :icon message-circle :text "" :fitText true :textColor @color.dim)
       (label#chat-empty :fitText true :textColor @color.muted :text ~(t "chat.choose")))
     (column :when ~(some? computed.chatFriend) :let {:friend ~computed.chatFriend} :fill true
       ~(chat-header)
       (panel :height 1 :anchors {:left 0 :right 0} :color @color.border)
       (node :grow 1 :height 0 :alignSelf stretch :color transparent ~(chat-messages))
       ~(chat-composer))))

(defview chat-header "The chat's header: the friend's avatar, name and what they are doing, and Invite to play." []
  `(row#chat-header :height 68 :anchors {:left 0 :right 0} :gap 12 :alignItems center :color @color.panel
     (panel :color transparent :width 18 :height 1)
     (node :use avatar :with {:avatar ~friend.avatar :presence ~friend.presence :size 40})
     (column :grow 1 :height 40 :gap 2 :alignSelf center
       (label#chat-name :font heading
         :text ~friend.name
         :textColor @color.bright
         :anchors {:left 0 :right 0}
         :height 22)
       (label :font small
         :anchors {:left 0 :right 0}
         :height 16
         :textColor @color.muted
         :text ~(if (= friend.presence "ingame")
                  (t "friends.playing" {:game (:title (find (fn [it] (= it.id friend.game)) data.games))})
                  (t (str "presence." friend.presence)))))
     ~(invite-button)
     (panel :color transparent :width 18 :height 1)))

(defview invite-button "Invite to play: the game running, or the one played last; off when there is none." []
  `(label#chat-invite :font strong
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
     :disabled ~(nil? computed.inviteGame)
     :tooltip ~(if (nil? computed.inviteGame)
                 (t "chat.nothingToInvite")
                 (t "chat.inviteTo" {:game computed.inviteGame.title}))
     :text ~(t "chat.inviteButton")
     :onClick ~(handler inviteToPlay)))

(defview chat-messages "The messages, a line with the day before each new day's first, and the friend typing." []
  `(column#chat-messages :fill true
     :scroll vertical
     :gap 10
     :role list
     :label ~(t "chat.messages" {:name friend.name})
     :onShow [{:scrollTo "#chat-end"}]
     (panel :color transparent :width 1 :height 8)
     ~(repeat [m computed.chat :index i :key m.id]
        `(column :anchors {:left 24 :right 24} :fitContent height :gap 10
           ~(day-line m i)
           (node :use chatMessage :with {:m ~m :friend ~friend})))
     (label#chat-typing :when ~(= state.typing friend.id)
       :font small
       :height 18
       :anchors {:left 24 :right 24}
       :role status
       :live polite
       :textColor @color.muted
       :text ~(t "chat.typing" {:name friend.name}))
     (panel#chat-end :color transparent :height 8 :anchors {:left 0 :right 0})))

(defview day-line "A line with the day (today, yesterday or the date) over a day's first message." [m i]
  `(row :when ~(or (= i 0)
                   (not= (floor (/ (get-in computed.chat [(- i 1) :at]) 86400000)) (floor (/ m.at 86400000))))
     :height 20
     :anchors {:left 0 :right 0}
     :gap 12
     :alignItems center
     (panel :grow 1 :height 1 :color @color.border)
     (label :style caps
       :fitText true
       :text ~(cond (>= m.at state.today) (t "time.today")
                    (>= m.at (- state.today 86400000)) (t "time.yesterday")
                    :else (formatDate m.at "medium" "utc")))
     (panel :grow 1 :height 1 :color @color.border)))

(defcomponent chatMessage
  "One message: a bubble on the right for yours, on the left with the friend's avatar for theirs; an invitation is a card."
  {:m {} :friend {}}
  `(row :let {:g ~(if (nil? m.game) nil (find (fn [it] (= it.id m.game)) data.games))}
     :anchors {:left 0 :right 0}
     :gap 8
     :fitContent height
     :justify ~(if m.mine "end" "start")
     :alignItems end
     :role listitem
     :label ~(str (if m.mine state.user.name friend.name)
                  ": "
                  (if (nil? g) m.text (t "chat.invite" {:game g.title})))
     (image :when ~(not m.mine)
       :file ~(str "../assets/art/avatars/" friend.avatar ".svg")
       :width 28
       :height 28
       :fit cover
       :radius 14)
     (column :fitContent true :gap 3 :alignItems ~(if m.mine "end" "start")
       (label :when ~(nil? g)
         :text ~m.text
         :wrap true
         :fitText {:maxWidth 440}
         :padding 10
         :radius 12
         :selectable true
         :color ~(if m.mine "@chat.mine" "@color.raised")
         :textColor ~(if m.mine "@chat.onMine" "@color.text"))
       ~(invitation m)
       (label :font small
         :fitText true
         :textColor @color.dim
         :text ~(if (< (- (now) m.at) 60000) (t "chat.now") (formatTime m.at "short" "utc"))))))

(defview invitation "An invitation to play: the game's cover and name, and Join (or Install) on theirs." [m]
  `(row :when ~(some? g)
     :width 320
     :height 96
     :gap 12
     :color @color.panel
     :radius 8
     :borderWidth 1
     :borderColor @color.border
     :alignItems center
     (panel :color transparent :width 2 :height 1)
     (image :file ~(str "../assets/art/" g.id "/cover.svg") :width 48 :height 72 :fit cover :radius 3)
     (column :grow 1 :height 64 :gap 4 :alignSelf center
       (label :style caps
         :height 16
         :anchors {:left 0 :right 0}
         :text ~(t (if m.mine "chat.invited" "chat.invites")))
       (label :font strong :height 18 :anchors {:left 0 :right 0} :text ~g.title :textColor @color.bright)
       (label :id ~(str "join-" m.id)
         :when ~(not m.mine)
         :font strong
         :fitText true
         :padding 6
         :height 26
         :radius 3
         :focusable true
         :role button
         :cursor hand
         :text ~(t (if g.installed "chat.join" "chat.install"))
         :gradient {:kind linear
                    :angle 90
                    :stops [~(if g.installed "@color.play" "@color.warm")
                            ~(if g.installed "@color.playEnd" "@color.warmEnd")]}
         :textColor "#ffffff"
         :onClick ~(handler primary g)))))

(defview chat-composer
  "The box to write in, which Enter sends, and the send button, lit once there is something to send."
  []
  `(row :height 64 :anchors {:left 0 :right 0} :gap 10 :alignItems center :color @color.panel
     (panel :color transparent :width 18 :height 1)
     (textarea#chat-input :grow 1
       :height 36
       :padding 10
       :bind state.chatDraft
       :radius 3
       :color @color.field
       :borderWidth 1
       :borderColor @color.border
       :placeholder ~(t "chat.placeholder" {:name friend.name})
       :placeholderColor @color.dim
       :textColor @color.text
       :label ~(t "chat.placeholder" {:name friend.name})
       :focused {:borderColor @color.accent}
       :onSubmit ~(handler sendMessage))
     (label#chat-send :font heading
       :icon send
       :text ""
       :width 40
       :height 36
       :radius 3
       :align center
       :focusable true
       :role button
       :cursor hand
       :label ~(t "chat.send")
       :tooltip ~(t "chat.send")
       :color ~(if (= (trim state.chatDraft) "") "@color.raised" "@color.accent")
       :textColor ~(if (= (trim state.chatDraft) "") "@color.dim" "#ffffff")
       :onClick ~(handler sendMessage))
     (panel :color transparent :width 84 :height 1)))
