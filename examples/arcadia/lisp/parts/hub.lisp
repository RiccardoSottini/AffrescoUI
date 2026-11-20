;;; hub.lisp --- a game's player hub: what players post about it, as discussions and guides, and a post's thread
;;;
;;; Opened from the links under a game's banner. The posts and their first replies are invented, the same for every
;;; game, with its name in them. A post opens on its thread: what it says, its replies, and a box to reply in.

(ns arcadia.parts.hub)

;;; The posts

(defstate hubTab hub)

(defstate hubPosts
  [{:id firstHours :kind discussions :key hub.post.firstHours :by 0 :replies 48 :votes 112}
   {:id allEndings :kind guides :key hub.post.allEndings :by 3 :replies 17 :votes 640}
   {:id controller :kind discussions :key hub.post.controller :by 1 :replies 9 :votes 21}
   {:id beginner :kind guides :key hub.post.beginner :by 5 :replies 31 :votes 288}
   {:id coop :kind discussions :key hub.post.coop :by 2 :replies 73 :votes 96}
   {:id secrets :kind guides :key hub.post.secrets :by 4 :replies 12 :votes 154}])

(defstate hubPost nil)

(defstate hubDraft "")

(defstate hubReplies [])

(defstate hubVoted [])

(defcomputed hubOpen "The post whose thread is open, or nil on the list."
  (find #(= (:id %) state.hubPost) state.hubPosts))

(defcomputed hubThread "The open post's replies: three from players, then the person's own, in the order written."
  (if (some? computed.hubOpen)
    (concat (map (fn [k]
                   {:by (mod (+ computed.hubOpen.by k 1) (count data.friends))
                    :key (str "hub.reply." (mod (+ computed.hubOpen.by k) 4))
                    :mine false})
                 [0 1 2])
            (map #(assoc % :mine true) (filter #(= (:post %) state.hubPost) state.hubReplies)))
    []))

(defmacro hub-posts-shown
  ;; A macro, not a function: the list it writes stays an expression, for a repeat and a height.
  "The posts of the section chosen, or all of them on the hub's own tab."
  []
  `(filter #(or (= state.hubTab :hub) (= (:kind %) state.hubTab)) state.hubPosts))

(defmacro hub-votes
  ;; A macro: the sum stays an expression, read again when a vote changes.
  "A post's votes, with the person's own."
  [p]
  `(+ (:votes ~p) (count (filter #(= % (:id ~p)) state.hubVoted))))

(defmacro hub-reply-count
  ;; A macro: the count stays an expression, read again when the person replies.
  "A post's replies, with the person's own."
  [p]
  `(+ (:replies ~p) (count (filter #(= (:post %) (:id ~p)) state.hubReplies))))

;;; The hub page

(defcomponent hubPage
  "The hub of the game on the page: its name, the three sections and the posts of the one chosen."
  {}
  `(column :fill true :scroll vertical :let {:g ~computed.game}
     ~(hub-header)
     ~(hub-tabs)
     ~(hub-thread)
     (column#hub-posts :when ~(nil? state.hubPost)
       :anchors {:left 32 :right 32}
       :height ~(+ 8 (* (count (hub-posts-shown)) 76))
       :gap 8
       :role list
       :label ~(t "hub.subtitle")
       ~(repeat [p (hub-posts-shown)] (hub-post p)))))

(defview hub-header "The game's cover and name over the hub's subtitle, and the way back to the game's page." []
  `(row :height 84 :anchors {:left 32 :right 32} :gap 16 :alignItems center
     (image :file ~(str "../assets/art/" (or-else g.art g.id) "/cover.svg")
       :width 40
       :height 60
       :radius 6
       :fit cover)
     (column :grow 1 :height 48
       (label :font heading :height 26 :anchors {:left 0 :right 0} :text ~g.title :textColor @color.bright)
       (label :font small :height 18 :anchors {:left 0 :right 0} :text ~(t "hub.subtitle") :textColor @color.muted))
     (node#hub-back :style sectionLink :icon arrow-left :text ~(t "hub.back") :onClick ~(handler openGame g.id))))

(defview hub-tabs "The hub's three sections, everything, the discussions and the guides, the chosen one lit." []
  `(row#hub-tabs :when ~(nil? state.hubPost)
     :height 40
     :anchors {:left 32 :right 32} :gap 6 :alignItems center :role tablist :arrows horizontal
     ~(repeat [s ["hub" "discussions" "guides"]]
        `(node :id ~(str "hub-tab-" s)
           :style pillButton
           :role tab
           :selected ~(= state.hubTab s)
           :color ~(if (= state.hubTab s) "@color.select" "transparent")
           :textColor ~(if (= state.hubTab s) "@color.bright" "@color.text")
           :text ~(t (if (= s "hub") "hub.all" (str "game.section." s)))
           :onClick ~(handler show-hub-tab s)))))

;;; A post

(defview hub-post "A post of the hub: its kind's icon, its title and author, its votes and replies." [p]
  `(row :id ~(str "hub-post-" p.id)
     :let {:who ~(get data.friends (mod p.by (count data.friends)))}
     :role listitem
     :label ~(t p.key {:game g.title})
     :focusable true
     :cursor hand
     :anchors {:left 0 :right 0}
     :height 68
     :gap 12
     :alignItems center
     :color @color.panel
     :radius 8
     :hover {:color @color.raised}
     :onClick ~(handler openHubPost p.id)
     (panel :width 2 :height 1 :color transparent)
     (label :icon ~(if (= p.kind "guides") "book-open" "messages-square")
       :text ""
       :fitText true
       :textColor @color.accent)
     ~(hub-post-title p)
     (label :font small :fitText true :icon thumbs-up :text ~(formatNumber (hub-votes p)) :textColor @color.muted)
     (label :font small :fitText true :icon message-circle :text ~(formatNumber (hub-reply-count p)) :textColor @color.muted)
     (panel :width 2 :height 1 :color transparent)))

(defview hub-post-title "A post's title, with the game's name in it, over who wrote it." [p]
  `(column :grow 1 :height 40
     (label :font strong
       :height 22
       :anchors {:left 0 :right 0}
       :text ~(t p.key {:game g.title})
       :textColor @color.text)
     (label :font small
       :height 18
       :anchors {:left 0 :right 0}
       :text ~(t "hub.by" {:who who.name})
       :textColor @color.muted)))

;;; A post's thread

(defview hub-thread "The open post: the way back to the list, the post itself, its replies, and the box to reply in." []
  `(column#hub-thread :when ~(some? computed.hubOpen)
     :let {:p ~computed.hubOpen}
     :anchors {:left 32 :right 32}
     :height ~(+ 40 196 18 (* (count computed.hubThread) 80) 72 48)
     :gap 12
     (row :height 40 :anchors {:left 0 :right 0} :alignItems center
       (node#hub-thread-back :style sectionLink
         :icon arrow-left
         :text ~(t "hub.backToPosts")
         :onClick ~(handler closeHubPost)))
     ~(hub-thread-post)
     (label :font caps
       :height 18
       :anchors {:left 0 :right 0}
       :text ~(upper-case (t "hub.replies" {:n (hub-reply-count p)}))
       :textColor @color.muted)
     (column#hub-replies :anchors {:left 0 :right 0}
       :height ~(* (count computed.hubThread) 80)
       :gap 8
       :role list
       :label ~(t "hub.replies" {:n (hub-reply-count p)})
       ~(repeat [r computed.hubThread] (hub-reply r)))
     ~(hub-reply-box)))

(defview hub-thread-post "The post: its kind, title and author, what it says, and its vote." []
  `(column#hub-thread-post :let {:who ~(get data.friends (mod p.by (count data.friends)))}
     :anchors {:left 0 :right 0}
     :height 196
     :color @color.panel
     :radius 10
     :gap 8
     (panel :height 6 :anchors {:left 0 :right 0} :color transparent)
     (label :font caps
       :height 18
       :anchors {:left 20 :right 20}
       :icon ~(if (= p.kind "guides") "book-open" "messages-square")
       :text ~(upper-case (t (str "game.section." p.kind)))
       :textColor @color.accent)
     (label#hub-thread-title :font heading
       :height 26
       :anchors {:left 20 :right 20}
       :text ~(t p.key {:game g.title})
       :textColor @color.bright)
     (row :height 24 :anchors {:left 20 :right 20} :gap 8 :alignItems center
       (node :use avatar :with {:avatar ~who.avatar :presence ~who.presence :size 22})
       (label :font small :fitText true :text ~(t "hub.by" {:who who.name}) :textColor @color.muted))
     (label :font default
       :height 40
       :anchors {:left 20 :right 20}
       :wrap true
       :text ~(t (str "hub.body." p.id) {:game g.title})
       :textColor @color.text)
     (row :height 28 :anchors {:left 20 :right 20} :gap 8 :alignItems center
       (label#hub-vote :font small
         :fitText true
         :padding 10
         :height 26
         :radius 13
         :icon thumbs-up
         :text ~(str (t "hub.helpful") "  " (formatNumber (hub-votes p)))
         :focusable true
         :role button
         :cursor hand
         :selected ~(contains state.hubVoted p.id)
         :color ~(if (contains state.hubVoted p.id) "@color.accent" "@color.raised")
         :textColor ~(if (contains state.hubVoted p.id) "@color.onAccent" "@color.text")
         :hover {:textColor @color.bright}
         :onClick ~(handler voteHubPost p.id)))))

(defview hub-reply "A reply: who wrote it, the person's own marked as theirs, and what it says." [r]
  `(row :let {:who ~(if r.mine nil (get data.friends r.by))}
     :role listitem
     :anchors {:left 0 :right 0}
     :height 72
     :gap 12
     :alignItems center
     :color ~(if r.mine "@color.raised" "@color.panel")
     :radius 8
     (panel :width 4 :height 1 :color transparent)
     (node :use avatar
       :with {:avatar ~(if r.mine state.user.avatar who.avatar)
              :presence ~(if r.mine state.user.status who.presence)
              :size 36})
     (column :grow 1 :height 44 :gap 4
       (label :font strong
         :height 20
         :anchors {:left 0 :right 0}
         :text ~(if r.mine (t "hub.you" {:name state.user.name}) who.name)
         :textColor ~(if r.mine "@color.accent" "@color.bright"))
       (label :font default
         :height 20
         :anchors {:left 0 :right 0}
         :text ~(if r.mine r.text (t r.key {:game g.title}))
         :textColor @color.text))
     (panel :width 4 :height 1 :color transparent)))

(defview hub-reply-box "The box to reply in, which Enter posts, and Reply, lit once there is something to post." []
  `(row :height 72 :anchors {:left 0 :right 0} :gap 10 :alignItems center
     (textarea#hub-reply-input :grow 1
       :height 38
       :padding 12
       :bind state.hubDraft
       :radius 6
       :color @color.field
       :borderWidth 1
       :borderColor @color.border
       :placeholder ~(t "hub.replyPlaceholder")
       :placeholderColor @color.dim
       :textColor @color.text
       :label ~(t "hub.replyPlaceholder")
       :focused {:borderColor @color.accent}
       :onSubmit ~(handler replyToPost))
     (label#hub-reply-send :font strong
       :fitText true
       :padding 16
       :height 38
       :radius 6
       :icon send
       :text ~(t "hub.reply")
       :focusable true
       :role button
       :cursor hand
       :color ~(if (= (trim state.hubDraft) "") "@color.raised" "@color.accent")
       :textColor ~(if (= (trim state.hubDraft) "") "@color.dim" "@color.onAccent")
       :onClick ~(handler replyToPost))))
